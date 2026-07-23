#pragma once

class ISteamNetworkingSockets;
class ISteamNetworkingConnectionSignaling;
struct SteamNetworkingIdentity;
typedef char SteamNetworkingErrMsg[1024];

class IWebSocketSignalingClient
{
public:
	virtual ~IWebSocketSignalingClient() {}
	virtual ISteamNetworkingConnectionSignaling* CreateSignalingForConnection(
		const SteamNetworkingIdentity& identityPeer,
		SteamNetworkingErrMsg& errMsg
	) = 0;
	virtual void Poll(ISteamNetworkingSockets* pSockets) = 0;
	virtual void Release() = 0;
};

extern IWebSocketSignalingClient* signalingClient;

ISteamNetworkingConnectionSignaling* CreateWebSocketSignalingClient(
	ISteamNetworkingSockets* pLocalInterface,
	const SteamNetworkingIdentity& identityPeer,
	int nLocalVirtualPort,
	int nRemoteVirtualPort);
