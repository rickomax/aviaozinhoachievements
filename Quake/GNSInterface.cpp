#include <cstdio>
#include <cstdint>
#include <cstring>
#include <string>
#include <deque>
#include <vector>
#include <unordered_map>
#include <mutex>
#include <atomic>
#include <thread>
#include <future>
#include <stdexcept>
#include <chrono>
#include "q_stdinc.h"
#include "arch_def.h"
#include "net_sys.h"
#include "quakedef.h"
#include "net_defs.h"
#include "pipe.h"
#include "rtc/rtc.hpp"
#include <nlohmann/json.hpp>
#include <windows.h>
#include <shellapi.h>

extern "C" {
	extern char pipe_available;
}

template<typename T>
bool tryGetJsonValue(const nlohmann::json& json, const std::string& key, T& out)
{
	auto it = json.find(key);
	if (it == json.end()) {
		return false;
	}
	out = it->get<T>();
	return true;
}

template<typename TKey, typename TValue>
bool tryGetMapValuePtr(std::unordered_map<TKey, TValue>& map, const TKey& key, TValue*& out)
{
	auto it = map.find(key);
	if (it == map.end()) return false;
	out = &it->second;
	return true;
}

template<typename TKey, typename TValue>
bool tryGetMapValue(const std::unordered_map<TKey, TValue>& map, const TKey& key, TValue& out)
{
	auto it = map.find(key);
	if (it == map.end()) {
		return false;
	}
	out = it->second;
	return true;
}

inline void WriteU32LE(uint8_t* p, uint32_t v)
{
	p[0] = (uint8_t)(v);
	p[1] = (uint8_t)(v >> 8);
	p[2] = (uint8_t)(v >> 16);
	p[3] = (uint8_t)(v >> 24);
}

inline void WriteU64LE(uint8_t* p, uint64_t v)
{
	p[0] = (uint8_t)(v);
	p[1] = (uint8_t)(v >> 8);
	p[2] = (uint8_t)(v >> 16);
	p[3] = (uint8_t)(v >> 24);
	p[4] = (uint8_t)(v >> 32);
	p[5] = (uint8_t)(v >> 40);
	p[6] = (uint8_t)(v >> 48);
	p[7] = (uint8_t)(v >> 56);
}

inline uint32_t ReadU32LE(const uint8_t* p)
{
	return (uint32_t)p[0]
		| ((uint32_t)p[1] << 8)
		| ((uint32_t)p[2] << 16)
		| ((uint32_t)p[3] << 24);
}

inline uint64_t ReadU64LE(const uint8_t* p)
{
	return (uint64_t)p[0]
		| ((uint64_t)p[1] << 8)
		| ((uint64_t)p[2] << 16)
		| ((uint64_t)p[3] << 24)
		| ((uint64_t)p[4] << 32)
		| ((uint64_t)p[5] << 40)
		| ((uint64_t)p[6] << 48)
		| ((uint64_t)p[7] << 56);
}

#define STUN1 "stun.l.google.com", 19302
#define STUN2 "stun1.l.google.com", 19302
#define GNS_WS_CONNECT_TIMEOUT_MS 10000

uint64_t mySteamId;

enum InitializationState {
	uninitialized,
	initializing,
	initialized
};

struct DataPacket
{
	uint64_t peerId = 0;
	rtc::binary data;
	DataPacket() = default;
	DataPacket(uint64_t peerId_, const rtc::binary& data_) : peerId(peerId_), data(data_)
	{
	}
};

std::atomic<InitializationState> initializationState = InitializationState::uninitialized;

rtc::WebSocket webSocket;
std::mutex webSocketMutex;
std::atomic<bool> webSocketReady{ false };

rtc::Configuration configuration;

std::unordered_map<uint64_t, std::shared_ptr<rtc::PeerConnection>> peerConnectionMap;
std::mutex peerConnectionMutex;

std::unordered_map<uint64_t, std::shared_ptr<rtc::DataChannel>> peerDataChannelMap;
std::mutex peerDataChannelMutex;

std::unordered_map<uint32_t, std::deque<DataPacket>> incomingPackets;
std::mutex incomingPacketsMutex;

std::unordered_map<uint64_t, std::deque<rtc::binary>> outgoingPacketsByPeer;
std::mutex outgoingPacketsMutex;

static inline uint64_t nowMs()
{
	return (uint64_t)GetTickCount64();
}

static inline void _dbgPrint(const char* tag, const char* fmt, ...)
{
	char buf[2048];
	int off = 0;
	off += snprintf(buf + off, sizeof(buf) - (size_t)off, "[%llu][%s] ",
		(unsigned long long)nowMs(), tag ? tag : "?");
	va_list args;
	va_start(args, fmt);
	vsnprintf(buf + off, sizeof(buf) - (size_t)off, fmt ? fmt : "", args);
	va_end(args);
	std::printf("%s\n", buf);
}

#ifdef _DEBUG
#define dbgPrint _dbgPrint
#else
#define dbgPrint (void)
#endif

static inline void enqueueOutgoing(uint64_t peerId, rtc::binary&& packet)
{
	std::lock_guard<std::mutex> lk(outgoingPacketsMutex);
	outgoingPacketsByPeer[peerId].push_back(std::move(packet));
}

static inline bool tryDequeueOutgoing(uint64_t peerId, rtc::binary& outPacket)
{
	std::lock_guard<std::mutex> lk(outgoingPacketsMutex);
	auto it = outgoingPacketsByPeer.find(peerId);
	if (it == outgoingPacketsByPeer.end() || it->second.empty()) {
		return false;
	}
	outPacket = std::move(it->second.front());
	it->second.pop_front();
	if (it->second.empty()) {
		outgoingPacketsByPeer.erase(it);
	}
	return true;
}

static inline size_t queuedOutgoingCount(uint64_t peerId)
{
	std::lock_guard<std::mutex> lk(outgoingPacketsMutex);
	auto it = outgoingPacketsByPeer.find(peerId);
	if (it == outgoingPacketsByPeer.end()) return 0;
	return it->second.size();
}

static inline void clearOutgoing(uint64_t peerId)
{
	std::lock_guard<std::mutex> lk(outgoingPacketsMutex);
	outgoingPacketsByPeer.erase(peerId);
}

static inline const char* initStateToStr(InitializationState s)
{
	switch (s) {
	case InitializationState::uninitialized: return "uninitialized";
	case InitializationState::initializing: return "initializing";
	case InitializationState::initialized: return "initialized";
	default: return "?";
	}
}

void removePeer(uint64_t peerId) {
	dbgPrint("PEER", "removePeer peer=%llu", (unsigned long long)peerId);

	{
		std::lock_guard<std::mutex> peerDataChannelLock(peerDataChannelMutex);
		std::shared_ptr<rtc::DataChannel>* dataChannelPtr = nullptr;
		if (tryGetMapValuePtr(peerDataChannelMap, peerId, dataChannelPtr)) {
			dbgPrint("DC", "closing DC peer=%llu", (unsigned long long)peerId);
			try { dataChannelPtr->get()->close(); }
			catch (...) {}
			peerDataChannelMap.erase(peerId);
		}
	}
	{
		std::lock_guard<std::mutex> peerConnectionLock(peerConnectionMutex);
		std::shared_ptr<rtc::PeerConnection>* peerConnectionPtr = nullptr;
		if (tryGetMapValuePtr(peerConnectionMap, peerId, peerConnectionPtr)) {
			dbgPrint("PC", "closing PC peer=%llu", (unsigned long long)peerId);
			try { peerConnectionPtr->get()->close(); }
			catch (...) {}
			peerConnectionMap.erase(peerId);
		}
	}
	{
		std::lock_guard<std::mutex> incomingPacketsLock(incomingPacketsMutex);
		incomingPackets.erase(peerId);
	}
	clearOutgoing(peerId);
}

void wireDataChannel(uint64_t peerId, const std::shared_ptr<rtc::DataChannel>& dataChannel)
{
	if (!dataChannel) {
		dbgPrint("DC", "wireDataChannel peer=%llu dataChannel=NULL", (unsigned long long)peerId);
		return;
	}

	dbgPrint("DC", "wireDataChannel peer=%llu (attach callbacks)", (unsigned long long)peerId);

	dataChannel->onOpen([peerId]() {
		dbgPrint("DC", "onOpen peer=%llu queued=%zu", (unsigned long long)peerId, queuedOutgoingCount(peerId));

		std::shared_ptr<rtc::DataChannel> dc;
		{
			std::lock_guard<std::mutex> peerDataChannelLock(peerDataChannelMutex);
			(void)tryGetMapValue(peerDataChannelMap, peerId, dc);
		}
		if (!dc) {
			dbgPrint("DC", "onOpen peer=%llu but map has no DC (race?)", (unsigned long long)peerId);
			return;
		}
		if (!dc->isOpen()) {
			dbgPrint("DC", "onOpen peer=%llu but dc->isOpen()==false (unexpected)", (unsigned long long)peerId);
			return;
		}

		size_t flushed = 0;
		for (;;) {
			rtc::binary pkt;
			if (!tryDequeueOutgoing(peerId, pkt)) {
				break;
			}
			try {
				if (!dc->isOpen()) {
					dbgPrint("DC", "flush stop peer=%llu dc closed mid-flush, requeue 1", (unsigned long long)peerId);
					enqueueOutgoing(peerId, std::move(pkt));
					break;
				}
				dc->send(std::move(pkt));
				++flushed;
			}
			catch (const std::exception& e) {
				dbgPrint("DC", "flush send EX peer=%llu after=%zu err=%s (requeue 1 + stop)",
					(unsigned long long)peerId, flushed, e.what());
				enqueueOutgoing(peerId, std::move(pkt));
				break;
			}
			catch (...) {
				dbgPrint("DC", "flush send EX peer=%llu after=%zu err=unknown (requeue 1 + stop)",
					(unsigned long long)peerId, flushed);
				enqueueOutgoing(peerId, std::move(pkt));
				break;
			}
		}
		dbgPrint("DC", "onOpen flush done peer=%llu flushed=%zu remaining=%zu",
			(unsigned long long)peerId, flushed, queuedOutgoingCount(peerId));
		});

	dataChannel->onClosed([peerId]() {
		dbgPrint("DC", "onClosed peer=%llu queued=%zu", (unsigned long long)peerId, queuedOutgoingCount(peerId));
		});

	dataChannel->onError([peerId](std::string err) {
		dbgPrint("DC", "onError peer=%llu err=%s", (unsigned long long)peerId, err.c_str());
		});

	dataChannel->onMessage(
		[peerId](rtc::binary data)
		{
			if (data.size() < sizeof(uint32_t)) {
				dbgPrint("DC", "onMessage peer=%llu tooSmall=%zu", (unsigned long long)peerId, (size_t)data.size());
				return;
			}
			uint8_t* ptr = reinterpret_cast<uint8_t*>(data.data());
			const uint32_t socketId = ReadU32LE(ptr);
			ptr += sizeof(uint32_t);
			if (socketId == UINT32_MAX) {
				uint64_t peerId2 = ReadU64LE(ptr);
				dbgPrint("DC", "onMessage peer=%llu CLOSECMD peerInPayload=%llu", (unsigned long long)peerId, (unsigned long long)peerId2);
				removePeer(peerId2);
				return;
			}
			rtc::binary payload(data.begin() + sizeof(uint32_t), data.end());
			{
				std::lock_guard<std::mutex> incomingPacketsLock(incomingPacketsMutex);
				incomingPackets[socketId].emplace_back(peerId, std::move(payload));
			}
		},
		nullptr
	);

	{
		std::lock_guard<std::mutex> peerDataChannelLock(peerDataChannelMutex);
		std::shared_ptr<rtc::DataChannel>* existing = nullptr;
		if (tryGetMapValuePtr(peerDataChannelMap, peerId, existing)) {
			*existing = dataChannel;
			dbgPrint("DC", "wireDataChannel peer=%llu stored (replaced)", (unsigned long long)peerId);
		}
		else {
			peerDataChannelMap.emplace(peerId, dataChannel);
			dbgPrint("DC", "wireDataChannel peer=%llu stored (new)", (unsigned long long)peerId);
		}
	}
}

std::shared_ptr<rtc::PeerConnection> createPeerConnection(uint64_t peerId)
{
	dbgPrint("PC", "createPeerConnection peer=%llu", (unsigned long long)peerId);

	auto peerConnection = std::make_shared<rtc::PeerConnection>(configuration);

	peerConnection->onStateChange([peerId](rtc::PeerConnection::State state) {
		const char* s = "?";
		switch (state) {
		case rtc::PeerConnection::State::New: s = "New"; break;
		case rtc::PeerConnection::State::Connecting: s = "Connecting"; break;
		case rtc::PeerConnection::State::Connected: s = "Connected"; break;
		case rtc::PeerConnection::State::Disconnected: s = "Disconnected"; break;
		case rtc::PeerConnection::State::Failed: s = "Failed"; break;
		case rtc::PeerConnection::State::Closed: s = "Closed"; break;
		}
		dbgPrint("PC", "onStateChange peer=%llu state=%s", (unsigned long long)peerId, s);
		});

	peerConnection->onGatheringStateChange([peerId](rtc::PeerConnection::GatheringState state) {
		const char* s = "?";
		switch (state) {
		case rtc::PeerConnection::GatheringState::New: s = "New"; break;
		case rtc::PeerConnection::GatheringState::InProgress: s = "InProgress"; break;
		case rtc::PeerConnection::GatheringState::Complete: s = "Complete"; break;
		}
		dbgPrint("PC", "onGatheringStateChange peer=%llu state=%s", (unsigned long long)peerId, s);
		});

	peerConnection->onLocalDescription([peerId](rtc::Description description) {
		dbgPrint("SIG", "onLocalDescription peer=%llu type=%s wsReady=%d",
			(unsigned long long)peerId,
			description.typeString().c_str(),
			webSocketReady.load(std::memory_order_acquire) ? 1 : 0);

		if (!webSocketReady.load(std::memory_order_acquire)) {
			dbgPrint("SIG", "drop localDescription peer=%llu (ws not ready)", (unsigned long long)peerId);
			return;
		}
		nlohmann::json message = {
			{"id", std::to_string(peerId)},
			{"type", description.typeString()},
			{"description", std::string(description)}
		};
		{
			std::lock_guard<std::mutex> webSocketLock(webSocketMutex);
			if (webSocketReady.load(std::memory_order_acquire)) {
				try {
					webSocket.send(message.dump());
					dbgPrint("SIG", "TX desc peer=%llu type=%s bytes=%zu",
						(unsigned long long)peerId, description.typeString().c_str(), message.dump().size());
				}
				catch (const std::exception& e) {
					dbgPrint("SIG", "TX desc FAILED peer=%llu err=%s", (unsigned long long)peerId, e.what());
				}
				catch (...) {
					dbgPrint("SIG", "TX desc FAILED peer=%llu err=unknown", (unsigned long long)peerId);
				}
			}
			else {
				dbgPrint("SIG", "drop localDescription peer=%llu (ws became not ready)", (unsigned long long)peerId);
			}
		}
		});

	peerConnection->onLocalCandidate([peerId](rtc::Candidate candidate) {
		if (!webSocketReady.load(std::memory_order_acquire)) {
			return;
		}
		nlohmann::json message = {
			{"id", std::to_string(peerId)},
			{"type", "candidate"},
			{"candidate", std::string(candidate)},
			{"mid", candidate.mid()}
		};
		{
			std::lock_guard<std::mutex> webSocketLock(webSocketMutex);
			if (webSocketReady.load(std::memory_order_acquire)) {
				try {
					webSocket.send(message.dump());
				}
				catch (...) {
					dbgPrint("SIG", "TX candidate FAILED peer=%llu", (unsigned long long)peerId);
				}
			}
		}
		});

	peerConnection->onDataChannel([peerId](std::shared_ptr<rtc::DataChannel> dataChannel) {
		dbgPrint("DC", "onDataChannel peer=%llu (remote created)", (unsigned long long)peerId);
		wireDataChannel(peerId, dataChannel);
		});

	peerConnection->onIceStateChange([peerId](rtc::PeerConnection::IceState state) {
		const char* s = "?";
		switch (state) {
		case rtc::PeerConnection::IceState::New: s = "New"; break;
		case rtc::PeerConnection::IceState::Checking: s = "Checking"; break;
		case rtc::PeerConnection::IceState::Connected: s = "Connected"; break;
		case rtc::PeerConnection::IceState::Completed: s = "Completed"; break;
		case rtc::PeerConnection::IceState::Failed: s = "Failed"; break;
		case rtc::PeerConnection::IceState::Disconnected: s = "Disconnected"; break;
		case rtc::PeerConnection::IceState::Closed: s = "Closed"; break;
		}
		dbgPrint("PC", "onIceStateChange peer=%llu ice=%s", (unsigned long long)peerId, s);
		});

	{
		std::lock_guard<std::mutex> peerConnectionLock(peerConnectionMutex);
		peerConnectionMap[peerId] = peerConnection;
	}
	return peerConnection;
}

#include "pipe.h"

extern "C" {
#include "GNSInterface.h"

#define GNS_MAGIC0 'G'
#define GNS_MAGIC1 'N'
#define GNS_MAGIC2 'S'
#define GNS_MAGIC3  0
#define GNS_ID_OFFSET 4

#define GNS_MAX_SOCKETS 4096

	unsigned char socketsInUse[GNS_MAX_SOCKETS];

	qboolean isAllDigits(const char* s)
	{
		if (!s || !*s) { return false; }
		for (; *s; ++s)
		{
			if (*s < '0' || *s > '9') { return false; }
		}
		return true;
	}

	qboolean validSteamId(const char* address)
	{
		if (!address) { return false; }
		const size_t prefix_len = strlen(GNS_PREFIX);
		if (strncmp(address, GNS_PREFIX, prefix_len) != 0) { return false; }
		const char* p = address + prefix_len;
		if (*p == '\0' || *p < '0' || *p > '9') { return false; }
		while (*p >= '0' && *p <= '9') { p++; }
		if (*p == '\0') { return true; }
		if (*p != ':') { return false; }
		p++;
		if (*p == '\0') { return false; }
		while (*p >= '0' && *p <= '9') { p++; }
		return (*p == '\0');
	}

	qboolean isSteamAddr(const struct qsockaddr* addr)
	{
		if (!addr) { return false; }
		if (addr->qsa_family != AF_INET) { return false; }
		const unsigned char* d = (const unsigned char*)addr->qsa_data;
		if (d[0] != GNS_MAGIC0 || d[1] != GNS_MAGIC1 || d[2] != GNS_MAGIC2 || d[3] != GNS_MAGIC3)
		{
			return false;
		}
		const char* s = (const char*)(addr->qsa_data + GNS_ID_OFFSET);
		const size_t maxlen = sizeof(addr->qsa_data) - GNS_ID_OFFSET;
		size_t i;
		for (i = 0; i < maxlen; ++i)
		{
			char c = s[i];
			if (c == '\0') { break; }
			if (c < '0' || c > '9') { return false; }
		}
		return (i > 0 && i < maxlen);
	}

	uint64_t decodeSteamId(const struct qsockaddr* addr)
	{
		if (!isSteamAddr(addr)) { return 0; }
		return _strtoui64((const char*)addr->qsa_data + GNS_ID_OFFSET, NULL, 10);
	}

	void encodeSteamId(struct qsockaddr* addr, uint64_t steamId)
	{
		if (!addr) { return; }
		memset(addr, 0, sizeof(*addr));
		addr->qsa_family = AF_INET;
		addr->qsa_data[0] = GNS_MAGIC0;
		addr->qsa_data[1] = GNS_MAGIC1;
		addr->qsa_data[2] = GNS_MAGIC2;
		addr->qsa_data[3] = GNS_MAGIC3;
		snprintf((char*)addr->qsa_data + GNS_ID_OFFSET,
			sizeof(addr->qsa_data) - GNS_ID_OFFSET,
			"%llu",
			(unsigned long long)steamId);
	}

	int tryEncodeSteamId(const char* string, struct qsockaddr* addr)
	{
		if (!string || !addr) { return -1; }
		const size_t prefix_len = strlen(GNS_PREFIX);
		if (strncmp(string, GNS_PREFIX, prefix_len) != 0) { return -1; }
		const char* token = string + prefix_len;
		if (!isAllDigits(token)) { return -1; }
		char* endptr = NULL;
		uint64_t steamId = _strtoui64(token, &endptr, 10);
		if (endptr == token || (endptr && *endptr != '\0') || steamId == 0) { return -1; }
		encodeSteamId(addr, steamId);
		return 0;
	}

	bool tryGetSteamIdFromCmd(uint64_t& outSteamId)
	{
		outSteamId = 0;
		int argc = 0;
		LPWSTR* wargv = CommandLineToArgvW(GetCommandLineW(), &argc);
		if (!wargv) return false;
		for (int i = 0; i + 1 < argc; ++i)
		{
			if (_wcsicmp(wargv[i], L"-steamid") == 0)
			{
				const wchar_t* v = wargv[i + 1];
				outSteamId = _wcstoui64(v, nullptr, 10);
				LocalFree(wargv);
				return outSteamId != 0;
			}
		}
		LocalFree(wargv);
		return false;
	}

	int gns_init(void)
	{
		InitializationState expected = InitializationState::uninitialized;
		if (!initializationState.compare_exchange_strong(expected, InitializationState::initializing))
		{
			if (expected == InitializationState::initialized) {
				return 1;
			}
			dbgPrint("INIT", "gns_init: already initializing, return 0 (state=%s)", initStateToStr(expected));
			return 0;
		}

		dbgPrint("INIT", "gns_init begin state->initializing");

		try {

			if (!tryGetSteamIdFromCmd(mySteamId))
			{
				dbgPrint("INIT", "steamid not in cmdline, pipe_available=%d", pipe_available ? 1 : 0);
				if (!pipe_available) {
					initializationState.store(InitializationState::uninitialized, std::memory_order_release);
					dbgPrint("INIT", "pipe not available -> fail init");
					return 0;
				}
				Pipe_Write("get_steam_id");
				while (!Pipe_Read()) {}
				mySteamId = _strtoui64(pipe_buffer, NULL, 10);
			}

			dbgPrint("INIT", "mySteamId=%llu", (unsigned long long)mySteamId);

			configuration.iceServers.clear();
			configuration.iceServers.emplace_back(STUN1);
			configuration.iceServers.emplace_back(STUN2);
			configuration.enableIceUdpMux = true;

			webSocketReady.store(false, std::memory_order_release);

			auto webSocketPromise = std::make_shared<std::promise<void>>();
			auto webSocketFuture = webSocketPromise->get_future();
			auto promiseSet = std::make_shared<std::atomic<bool>>(false);

			auto fulfillOnce = [webSocketPromise, promiseSet]() {
				if (!promiseSet->exchange(true, std::memory_order_acq_rel)) {
					try { webSocketPromise->set_value(); }
					catch (...) {}
				}
				};

			webSocket.onOpen([fulfillOnce] {
				webSocketReady.store(true, std::memory_order_release);
				initializationState.store(InitializationState::initialized, std::memory_order_release);
				dbgPrint("WS", "onOpen -> wsReady=1 init=initialized");
				fulfillOnce();
				});

			webSocket.onError([fulfillOnce](std::string err) {
				webSocketReady.store(false, std::memory_order_release);
				initializationState.store(InitializationState::uninitialized, std::memory_order_release);
				dbgPrint("WS", "onError -> wsReady=0 init=uninitialized err=%s", err.c_str());
				fulfillOnce();
				});

			webSocket.onClosed([fulfillOnce] {
				webSocketReady.store(false, std::memory_order_release);
				initializationState.store(InitializationState::uninitialized, std::memory_order_release);
				dbgPrint("WS", "onClosed -> wsReady=0 init=uninitialized");
				fulfillOnce();
				});

			webSocket.onMessage(nullptr, [](rtc::string data) {
				try {
					nlohmann::json message = nlohmann::json::parse(data);

					std::string id;
					if (!tryGetJsonValue(message, "id", id)) return;
					uint64_t peerId = std::stoull(id);

					std::string type;
					if (!tryGetJsonValue(message, "type", type)) return;

					dbgPrint("SIG", "RX peer=%llu type=%s bytes=%zu",
						(unsigned long long)peerId, type.c_str(), (size_t)data.size());

					std::shared_ptr<rtc::PeerConnection> peerConnection;
					{
						std::lock_guard<std::mutex> peerConnectionLock(peerConnectionMutex);
						std::shared_ptr<rtc::PeerConnection>* peerConnectionPtr = nullptr;
						if (tryGetMapValuePtr(peerConnectionMap, peerId, peerConnectionPtr)) {
							peerConnection = *peerConnectionPtr;
						}
					}

					if (!peerConnection) {
						if (type != "offer") {
							dbgPrint("SIG", "drop peer=%llu type=%s (no PC and not offer)", (unsigned long long)peerId, type.c_str());
							return;
						}
						dbgPrint("SIG", "peer=%llu offer but no PC -> create", (unsigned long long)peerId);
						peerConnection = createPeerConnection(peerId);
					}

					if (type == "offer" || type == "answer") {
						std::string description;
						if (tryGetJsonValue(message, "description", description)) {
							try {
								peerConnection->setRemoteDescription(rtc::Description(description, type));
								dbgPrint("SIG", "setRemoteDescription OK peer=%llu type=%s", (unsigned long long)peerId, type.c_str());
							}
							catch (const std::exception& e) {
								dbgPrint("SIG", "setRemoteDescription FAIL peer=%llu type=%s err=%s",
									(unsigned long long)peerId, type.c_str(), e.what());
							}
							catch (...) {
								dbgPrint("SIG", "setRemoteDescription FAIL peer=%llu type=%s err=unknown",
									(unsigned long long)peerId, type.c_str());
							}
						}
					}
					else if (type == "candidate") {
						std::string candidate;
						if (!tryGetJsonValue(message, "candidate", candidate)) return;
						std::string mid;
						if (!tryGetJsonValue(message, "mid", mid)) return;

						try {
							peerConnection->addRemoteCandidate(rtc::Candidate(candidate, mid));
						}
						catch (const std::exception& e) {
							dbgPrint("SIG", "addRemoteCandidate FAIL peer=%llu err=%s", (unsigned long long)peerId, e.what());
						}
						catch (...) {
							dbgPrint("SIG", "addRemoteCandidate FAIL peer=%llu err=unknown", (unsigned long long)peerId);
						}
					}
				}
				catch (const std::exception& e) {
					dbgPrint("WS", "onMessage parse FAIL err=%s", e.what());
					return;
				}
				catch (...) {
					dbgPrint("WS", "onMessage parse FAIL err=unknown");
					return;
				}
				});

			std::string webSocketUrl = std::string(SIGNAL_URL) + std::to_string(mySteamId);
			{
				std::lock_guard<std::mutex> webSocketLock(webSocketMutex);
				dbgPrint("WS", "open %s", webSocketUrl.c_str());
				webSocket.open(webSocketUrl);
			}

			if (webSocketFuture.wait_for(std::chrono::milliseconds(GNS_WS_CONNECT_TIMEOUT_MS)) == std::future_status::timeout)
			{
				dbgPrint("WS", "connect timeout after %d ms", GNS_WS_CONNECT_TIMEOUT_MS);

				webSocketReady.store(false, std::memory_order_release);
				initializationState.store(InitializationState::uninitialized, std::memory_order_release);

				try {
					std::lock_guard<std::mutex> webSocketLock(webSocketMutex);
					webSocket.close();
				}
				catch (...) {}

				return 0;
			}

			webSocketFuture.get();

			dbgPrint("INIT", "gns_init end state=%s wsReady=%d",
				initStateToStr(initializationState.load(std::memory_order_acquire)),
				webSocketReady.load(std::memory_order_acquire) ? 1 : 0);

			return (initializationState.load(std::memory_order_acquire) == InitializationState::initialized) ? 1 : 0;
		}
		catch (const std::exception& e) {
			dbgPrint("INIT", "gns_init exception -> reset err=%s", e.what());
			initializationState.store(InitializationState::uninitialized, std::memory_order_release);
			webSocketReady.store(false, std::memory_order_release);
			try {
				std::lock_guard<std::mutex> webSocketLock(webSocketMutex);
				webSocket.close();
			}
			catch (...) {}
			return 0;
		}
	}

	void gns_shutdown(void)
	{
		dbgPrint("INIT", "gns_shutdown");
		initializationState.store(InitializationState::uninitialized, std::memory_order_release);
		webSocketReady.store(false, std::memory_order_release);
		std::lock_guard<std::mutex> webSocketGuard(webSocketMutex);
		try { webSocket.close(); }
		catch (...) {}
	}

	int gns_socket(int domain, int type, int protocol)
	{
		if (!gns_init())
		{
			dbgPrint("SOCK", "socket() init failed");
			return -1;
		}
		int s;
		for (s = 0; s < GNS_MAX_SOCKETS; ++s)
		{
			if (!socketsInUse[s])
			{
				socketsInUse[s] = 1;
				dbgPrint("SOCK", "socket() -> %d", s);
				return s;
			}
		}
		dbgPrint("SOCK", "socket() fail (no slots)");
		return -1;
	}

	int gns_bind(int s, const struct sockaddr* addr, int addrlen)
	{
		if (!gns_init())
		{
			dbgPrint("SOCK", "bind(%d) init failed", s);
			return -1;
		}
		dbgPrint("SOCK", "bind(%d) ok", s);
		return 0;
	}

	int gns_sendto(int s, const void* buf, int len, int flags, const struct sockaddr* to, int tolen)
	{
		if (!gns_init()) {
			dbgPrint("SEND", "sendto(%d) init failed", s);
			return -1;
		}
		if (s < 0 || !buf || len <= 0 || !to) {
			dbgPrint("SEND", "sendto invalid args s=%d buf=%p len=%d to=%p", s, buf, len, to);
			return -1;
		}

		const struct qsockaddr* qaddr = (const struct qsockaddr*)to;
		const uint64_t peerId = decodeSteamId(qaddr);
		if (peerId == 0) {
			dbgPrint("SEND", "sendto(%d) invalid peerId", s);
			return -1;
		}

		std::shared_ptr<rtc::PeerConnection> peerConnection;
		{
			std::lock_guard<std::mutex> peerConnectionLock(peerConnectionMutex);
			(void)tryGetMapValue(peerConnectionMap, peerId, peerConnection);
		}
		if (!peerConnection) {
			dbgPrint("SEND", "peer=%llu no PC -> create", (unsigned long long)peerId);
			peerConnection = createPeerConnection(peerId);
		}

		std::shared_ptr<rtc::DataChannel> dataChannel;
		{
			std::lock_guard<std::mutex> peerDataChannelLock(peerDataChannelMutex);
			(void)tryGetMapValue(peerDataChannelMap, peerId, dataChannel);
		}
		if (!dataChannel) {
			dbgPrint("SEND", "peer=%llu no DC -> create", (unsigned long long)peerId);
			rtc::DataChannelInit init;
			init.reliability.unordered = true;
			init.reliability.maxRetransmits = 0;
			dataChannel = peerConnection->createDataChannel("game", init);
			wireDataChannel(peerId, dataChannel);
		}

		rtc::binary data;
		data.resize(sizeof(uint32_t) + (size_t)len);
		uint8_t* ptr = reinterpret_cast<uint8_t*>(data.data());
		WriteU32LE(ptr, (uint32_t)s);
		std::memcpy(ptr + sizeof(uint32_t), buf, (size_t)len);

		bool sent = false;
		if (dataChannel && dataChannel->isOpen()) {
			try {
				dataChannel->send(rtc::binary(data));
				sent = true;
			}
			catch (const std::exception& e) {
				dbgPrint("SEND", "peer=%llu DC send EX -> queue err=%s", (unsigned long long)peerId, e.what());
				sent = false;
			}
			catch (...) {
				dbgPrint("SEND", "peer=%llu DC send EX -> queue err=unknown", (unsigned long long)peerId);
				sent = false;
			}
		}

		if (!sent) {
			enqueueOutgoing(peerId, std::move(data));
			dbgPrint("SEND", "peer=%llu queued bytes=%d queuedNow=%zu dcOpen=%d",
				(unsigned long long)peerId, len, queuedOutgoingCount(peerId), (dataChannel && dataChannel->isOpen()) ? 1 : 0);
		}

		return len;
	}

	int gns_recvfrom(int s, void* buf, int len, int flags, struct sockaddr* from, int* fromlen)
	{
		if (!gns_init())
		{
			return -1;
		}
		if (s < 0 || !buf || len <= 0)
		{
			return -1;
		}
		DataPacket dataPacket;
		{
			std::lock_guard<std::mutex> incomingPacketsLock(incomingPacketsMutex);
			std::deque<DataPacket>* packets;
			if (!tryGetMapValuePtr(incomingPackets, (uint32_t)s, packets)) {
				return -1;
			}
			if (packets->empty()) {
				return -1;
			}
			dataPacket = std::move(packets->front());
			packets->pop_front();
		}
		const int payloadLength = (int)dataPacket.data.size();
		int copyLength = payloadLength;
		if (copyLength > len)
		{
			copyLength = len;
		}
		if (copyLength > 0)
		{
			std::memcpy(buf, dataPacket.data.data(), (size_t)copyLength);
		}
		if (from && fromlen)
		{
			struct qsockaddr* qaddrOut = (struct qsockaddr*)from;
			encodeSteamId(qaddrOut, dataPacket.peerId);
			*fromlen = (int)sizeof(struct qsockaddr);
		}
		return copyLength;
	}

	int gns_closesocket(int s)
	{
		if (!gns_init())
		{
			return -1;
		}
		if (s < 0 || s >= GNS_MAX_SOCKETS) {
			return -1;
		}
		dbgPrint("SOCK", "closesocket(%d)", s);
		socketsInUse[s] = 0;

		{
			std::lock_guard<std::mutex> peerDataChannelLock(peerDataChannelMutex);
			for (auto kvp : peerDataChannelMap) {

				rtc::binary data;
				data.resize(sizeof(uint32_t) + sizeof(uint64_t));
				uint8_t* ptr = reinterpret_cast<uint8_t*>(data.data());
				WriteU32LE(ptr, UINT32_MAX);
				ptr += sizeof(uint32_t);
				WriteU64LE(ptr, mySteamId);
				try {
					kvp.second->send(data);
				}
				catch (...) {
				}
				try { kvp.second->close(); }
				catch (...) {}
			}
			peerDataChannelMap.clear();
		}
		{
			std::lock_guard<std::mutex> peerConnectionLock(peerConnectionMutex);
			for (auto kvp : peerConnectionMap) {
				try { kvp.second->close(); }
				catch (...) {}
			};
			peerConnectionMap.clear();
		}
		{
			std::lock_guard<std::mutex> incomingPacketsLock(incomingPacketsMutex);
			incomingPackets.clear();
		}
		{
			std::lock_guard<std::mutex> outgoingPacketsLock(outgoingPacketsMutex);
			outgoingPacketsByPeer.clear();
		}

		return 0;
	}

	int gns_ioctlsocket(int s, long cmd, u_long* argp)
	{
		if (!gns_init())
		{
			return -1;
		}
		return 0;
	}

	int gns_setsockopt(int socket, int level, int option_name, const void* option_value, socklen_t option_len)
	{
		if (!gns_init())
		{
			return -1;
		}
		if (option_name == SO_BROADCAST && option_value && *(const char*)option_value)
		{
			return -1;
		}
		return 0;
	}
}