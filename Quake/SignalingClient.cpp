// WebSocket-based signaling client for GameNetworkingSockets P2P
// Adapted from trivial_signaling_client.cpp to use WebSocket instead of raw sockets

#include <string>
#include <mutex>
#include <deque>
#include <cstdint>
#include <steam/isteamnetworkingsockets.h>
#include <steam/isteamnetworkingutils.h>
#include <steam/steamnetworkingcustomsignaling.h>
#include "ixwebsocket/IXWebSocket.h"
#include "ixwebsocket/IXNetSystem.h"

#include "SignalingClient.hpp"

uint64_t GetSteamID(const SteamNetworkingIdentity& identityPeer) {
	int cbLen;
	const uint8_t* data = identityPeer.GetGenericBytes(cbLen);
	uint64_t result;
	memcpy(&result, data, sizeof(result));
	return result;
}

IWebSocketSignalingClient* signalingClient = nullptr;

class CWebSocketSignalingClient : public IWebSocketSignalingClient
{
private:
	// Binary message protocol
	static constexpr uint8_t MSG_HELLO = 0x01;
	static constexpr uint8_t MSG_SIGNAL = 0x02;

	// Per-connection signaling object
	struct ConnectionSignaling : ISteamNetworkingConnectionSignaling
	{
		CWebSocketSignalingClient* const m_pOwner;
		uint64_t const m_nPeerID;

		ConnectionSignaling(CWebSocketSignalingClient* owner, uint64_t peerID)
			: m_pOwner(owner), m_nPeerID(peerID)
		{
		}

		virtual bool SendSignal(HSteamNetConnection hConn, const SteamNetConnectionInfo_t& info, const void* pMsg, int cbMsg) override
		{
			(void)hConn;
			(void)info;

			return m_pOwner->SendSignalToPeer(m_nPeerID, pMsg, cbMsg);
		}

		virtual void Release() override
		{
			delete this;
		}
	};

	ix::WebSocket m_webSocket;
	uint64_t m_nMyID;
	std::string m_sServerURL;

	std::mutex m_mutexIncoming;
	std::deque<std::vector<uint8_t>> m_queueIncoming;

	std::mutex m_mutexOutgoing;
	std::deque<std::vector<uint8_t>> m_queueOutgoing;

	bool m_bConnected;

	// Extract uint64 ID from SteamNetworkingIdentity
	static uint64_t GetIDFromIdentity(const SteamNetworkingIdentity& identity)
	{
		int cbLen;
		const uint8_t* pData = identity.GetGenericBytes(cbLen);
		if (cbLen != sizeof(uint64_t))
			return 0;

		uint64_t result;
		memcpy(&result, pData, sizeof(result));
		return result;
	}

	void SendHello()
	{
		std::vector<uint8_t> msg;
		msg.resize(1 + sizeof(uint64_t));

		uint8_t* p = msg.data();
		*p++ = MSG_HELLO;
		memcpy(p, &m_nMyID, sizeof(m_nMyID));

		auto result = m_webSocket.sendBinary(std::string((char*)msg.data(), msg.size()));
		if (!result.success)
		{
			printf("[WebSocket] Failed to send HELLO\n");
		}
	}

	void ProcessIncomingMessage(const std::vector<uint8_t>& msg)
	{
		if (msg.size() < 1)
			return;

		if (msg[0] != MSG_SIGNAL)
			return;

		if (msg.size() < 21)
			return;

		const uint8_t* p = msg.data() + 1;
		p += 8; // Skip from_id
		p += 8; // Skip to_id

		uint32_t payloadSize;
		memcpy(&payloadSize, p, 4);
		p += 4;

		size_t expectedSize = 21 + payloadSize;
		if (msg.size() != expectedSize)
			return;

		std::lock_guard<std::mutex> lock(m_mutexIncoming);
		m_queueIncoming.emplace_back(p, p + payloadSize);

		while (m_queueIncoming.size() > 256)
		{
			printf("[WebSocket] Incoming signal queue backed up, dropping oldest\n");
			m_queueIncoming.pop_front();
		}
	}

public:
	CWebSocketSignalingClient(const char* serverURL, uint64_t myID)
		: m_nMyID(myID)
		, m_sServerURL(serverURL)
		, m_bConnected(false)
	{
		ix::initNetSystem();
		m_webSocket.setUrl(m_sServerURL);

		m_webSocket.setOnMessageCallback([this](const ix::WebSocketMessagePtr& msg)
			{
				if (msg->type == ix::WebSocketMessageType::Open)
				{
					printf("[WebSocket] Connected to signaling server\n");
					m_bConnected = true;
					SendHello();
					return;
				}

				if (msg->type == ix::WebSocketMessageType::Close)
				{
					printf("[WebSocket] Disconnected from signaling server\n");
					m_bConnected = false;
					return;
				}

				if (msg->type == ix::WebSocketMessageType::Error)
				{
					printf("[WebSocket] Error: %s\n", msg->errorInfo.reason.c_str());
					return;
				}

				if (msg->type == ix::WebSocketMessageType::Message)
				{
					if (!msg->binary)
						return;

					std::vector<uint8_t> data(msg->str.begin(), msg->str.end());
					ProcessIncomingMessage(data);
				}
			});

		m_webSocket.start();
	}

	bool SendSignalToPeer(uint64_t peerID, const void* pMsg, int cbMsg)
	{
		if (cbMsg < 0)
			return false;

		const uint32_t payloadSize = (uint32_t)cbMsg;
		const size_t totalSize = 1 + 8 + 8 + 4 + payloadSize;

		std::vector<uint8_t> msg;
		msg.resize(totalSize);

		uint8_t* p = msg.data();
		*p++ = MSG_SIGNAL;
		memcpy(p, &m_nMyID, 8); p += 8;
		memcpy(p, &peerID, 8); p += 8;
		memcpy(p, &payloadSize, 4); p += 4;

		if (payloadSize > 0)
			memcpy(p, pMsg, payloadSize);

		if (m_bConnected && m_webSocket.getReadyState() == ix::ReadyState::Open)
		{
			auto result = m_webSocket.sendBinary(std::string((char*)msg.data(), msg.size()));
			if (result.success)
				return true;
		}

		std::lock_guard<std::mutex> lock(m_mutexOutgoing);
		m_queueOutgoing.push_back(std::move(msg));

		while (m_queueOutgoing.size() > 256)
		{
			printf("[WebSocket] Outgoing signal queue backed up, dropping oldest\n");
			m_queueOutgoing.pop_front();
		}

		return true;
	}

	ISteamNetworkingConnectionSignaling* CreateSignalingForConnection(
		const SteamNetworkingIdentity& identityPeer,
		SteamNetworkingErrMsg& errMsg
	) override
	{
		uint64_t peerID = GetIDFromIdentity(identityPeer);
		if (peerID == 0)
		{
			sprintf(errMsg, "Failed to extract peer ID from identity");
			return nullptr;
		}

		printf("[WebSocket] Creating signaling for peer %llu\n", (unsigned long long)peerID);
		return new ConnectionSignaling(this, peerID);
	}

	void Poll(ISteamNetworkingSockets* pSockets) override
	{
		if (m_webSocket.getReadyState() == ix::ReadyState::Open)
		{
			std::lock_guard<std::mutex> lock(m_mutexOutgoing);
			while (!m_queueOutgoing.empty())
			{
				auto& msg = m_queueOutgoing.front();
				auto result = m_webSocket.sendBinary(std::string((char*)msg.data(), msg.size()));
				if (!result.success)
					break;

				m_queueOutgoing.pop_front();
			}
		}

		std::deque<std::vector<uint8_t>> localQueue;
		{
			std::lock_guard<std::mutex> lock(m_mutexIncoming);
			localQueue.swap(m_queueIncoming);
		}

		struct Context : ISteamNetworkingSignalingRecvContext
		{
			CWebSocketSignalingClient* m_pOwner;

			virtual ISteamNetworkingConnectionSignaling* OnConnectRequest(
				HSteamNetConnection hConn,
				const SteamNetworkingIdentity& identityPeer,
				int nLocalVirtualPort
			) override
			{
				(void)hConn;
				(void)nLocalVirtualPort;

				SteamNetworkingErrMsg ignoreErrMsg;
				return m_pOwner->CreateSignalingForConnection(identityPeer, ignoreErrMsg);
			}

			virtual void SendRejectionSignal(
				const SteamNetworkingIdentity& identityPeer,
				const void* pMsg, int cbMsg
			) override
			{
				(void)identityPeer;
				(void)pMsg;
				(void)cbMsg;
			}
		};

		Context context;
		context.m_pOwner = this;

		for (auto& signal : localQueue)
		{
			pSockets->ReceivedP2PCustomSignal(signal.data(), (int)signal.size(), &context);
		}
	}

	void Release() override
	{
		m_webSocket.stop(1000, "Normal closure");
		delete this;
	}
};

ISteamNetworkingConnectionSignaling* CreateWebSocketSignalingClient(
	ISteamNetworkingSockets* pLocalInterface,
	const SteamNetworkingIdentity& identityPeer,
	int nLocalVirtualPort,
	int nRemoteVirtualPort)
{
	if (signalingClient)
	{
		signalingClient->Release();
		signalingClient = nullptr;
	}
	uint64_t steamId = GetSteamID(identityPeer);
	//todo: external
	CWebSocketSignalingClient* webSocketSignalingClient = new CWebSocketSignalingClient("ws://129.121.36.81:8080", steamId);
	signalingClient = webSocketSignalingClient;
	SteamNetworkingErrMsg msg;
	return webSocketSignalingClient->CreateSignalingForConnection(identityPeer, msg);
}