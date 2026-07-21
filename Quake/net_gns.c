#include "q_stdinc.h"
#include "arch_def.h"
#include "net_sys.h"
#include "quakedef.h"
#include "net_defs.h"

#include "net_gns.h"
#include "GNSInterface.h"

extern cvar_t sv_public;
extern cvar_t steamserver;

static sys_socket_t gns_acceptsocket = INVALID_SOCKET;
static sys_socket_t gns_controlsocket = INVALID_SOCKET;

static double gns_next_update_time = 0;
static int gns_last_clients = -1;
static int gns_last_maxclients = -1;
static char gns_last_map[MAX_QPATH] = "";
static char gns_last_hostname[256] = "";

sys_socket_t GNS_Init(void)
{
	gns_controlsocket = GNS_OpenSocket(0);
	if (gns_controlsocket == INVALID_SOCKET)
	{
		Con_SafePrintf("GNS_Init: failed to open control socket\n");
		return INVALID_SOCKET;
	}

	Con_SafePrintf("GNS landriver initialized\n");
	return gns_controlsocket;
}

void GNS_Shutdown(void)
{
	GNS_Listen(false);

	if (gns_controlsocket != INVALID_SOCKET)
	{
		GNS_CloseSocket(gns_controlsocket);
		gns_controlsocket = INVALID_SOCKET;
	}
}

sys_socket_t GNS_Listen(qboolean state)
{
	if (state)
	{
		if (gns_acceptsocket != INVALID_SOCKET)
			return gns_acceptsocket;

		gns_acceptsocket = GNS_OpenSocket(net_hostport);
		if (sv_public.value) {
			Pipe_Write("host");
		}
		return gns_acceptsocket;
	}

	if (gns_acceptsocket == INVALID_SOCKET)
		return INVALID_SOCKET;

	GNS_CloseSocket(gns_acceptsocket);
	gns_acceptsocket = INVALID_SOCKET;

	Pipe_Write("unhost");
	return INVALID_SOCKET;
}

sys_socket_t GNS_OpenSocket(int port)
{
	sys_socket_t s = gns_socket(PF_INET, SOCK_DGRAM, IPPROTO_UDP);
	if (s == INVALID_SOCKET)
		return INVALID_SOCKET;
	return s;
}

int GNS_CloseSocket(sys_socket_t socketid)
{
	return gns_closesocket(socketid);
}

int GNS_Connect(sys_socket_t socketid, struct qsockaddr* addr)
{
	return 0;
}

sys_socket_t GNS_CheckNewConnections(void)
{
	char buf[4096];

	if (gns_acceptsocket == INVALID_SOCKET)
		return INVALID_SOCKET;

	if (gns_recvfrom(gns_acceptsocket, buf, (int)sizeof(buf), MSG_PEEK, NULL, NULL) > 0)
		return gns_acceptsocket;

	return INVALID_SOCKET;
}

int GNS_Read(sys_socket_t socketid, byte* buf, int len, struct qsockaddr* addr)
{
	int addrlen = (int)sizeof(struct qsockaddr);

	int ret = gns_recvfrom(
		socketid,
		(char*)buf,
		len,
		0,
		(struct sockaddr*)addr,
		&addrlen
	);

	if (ret <= 0)
		return 0;

	return ret;
}

int GNS_Write(sys_socket_t socketid, byte* buf, int len, struct qsockaddr* addr)
{
	int ret = gns_sendto(
		socketid,
		(char*)buf,
		len,
		0,
		(struct sockaddr*)addr,
		(int)sizeof(struct qsockaddr)
	);

	if (ret == SOCKET_ERROR)
		return 0;

	return ret;
}

int GNS_Broadcast(sys_socket_t socketid, byte* buf, int len)
{
	return -1;
}

int GNS_GetAddresses(qhostaddr_t* addresses, int maxaddresses)
{
	return 0;
}

const char* GNS_AddrToString(struct qsockaddr* addr, qboolean masked)
{
	static char buffer[64];

	if (!addr)
		return "<null>";

	if (isSteamAddr(addr))
	{
		uint64_t id = decodeSteamId(addr);
		if (id)
			q_snprintf(buffer, sizeof(buffer), "%s%llu", GNS_PREFIX, (unsigned long long)id);
		else
			q_strlcpy(buffer, "<gns-invalid>", sizeof(buffer));
		return buffer;
	}

	q_snprintf(buffer, sizeof(buffer), "<af:%d>", (int)addr->qsa_family);
	return buffer;
}

int GNS_StringToAddr(const char* string, struct qsockaddr* addr)
{
	if (!string || !addr)
		return -1;

	return tryEncodeSteamId(string, addr);
}

int GNS_GetSocketPort(struct qsockaddr* addr)
{
	return net_hostport;
}

int GNS_GetSocketAddr(sys_socket_t socketid, struct qsockaddr* addr)
{
	if (!addr)
		return -1;

	encodeSteamId(addr, 0);
	return 0;
}

int GNS_GetNameFromAddr(struct qsockaddr* addr, char* name)
{
	if (!addr || !name)
		return -1;

	q_strlcpy(name, GNS_AddrToString(addr, false), NET_NAMELEN);
	return 0;
}

int GNS_GetAddrFromName(const char* name, struct qsockaddr* addr)
{
	if (!name || !addr)
		return -1;

	return tryEncodeSteamId(name, addr);
}

int GNS_AddrCompare(struct qsockaddr* addr1, struct qsockaddr* addr2)
{
	if (!addr1 || !addr2)
		return -1;

	if (isSteamAddr(addr1) && isSteamAddr(addr2))
	{
		uint64_t id1 = decodeSteamId(addr1);
		uint64_t id2 = decodeSteamId(addr2);

		if (id1 == 0 || id2 == 0)
			return -1;

		return (id1 == id2) ? 0 : -1;
	}

	return -1;
}

int GNS_SetSocketPort(struct qsockaddr* addr, int port)
{
	return 0;
}

static void EscapeSpaces(const char* in, char* out, size_t outSize)
{
	size_t w = 0;

	if (!in || !out || outSize == 0)
	{
		if (out && outSize) out[0] = 0;
		return;
	}

	for (size_t r = 0; in[r] && w + 1 < outSize; r++)
	{
		char c = in[r];
		if (c == ' ')
			c = '_';
		out[w++] = c;
	}

	out[w] = 0;
}

void GNS_MaybeUpdateLobby(void)
{
	if (gns_acceptsocket == INVALID_SOCKET) return;
	if (!sv.active) return;
	if (!sv_public.value) return;
	if (!steamserver.value) return;

	const double now = Sys_DoubleTime();

	const int clients = net_activeconnections;
	const int maxc = svs.maxclients;

	const char* rawMap =
		(sv.name && *sv.name) ? sv.name : "";

	const char* rawName =
		(hostname.string && *hostname.string) ? hostname.string : "UNNAMED";

	char map[128];
	char serverName[256];

	EscapeSpaces(rawMap, map, sizeof(map));
	EscapeSpaces(rawName, serverName, sizeof(serverName));

	qboolean changed =
		clients != gns_last_clients ||
		maxc != gns_last_maxclients ||
		q_strcasecmp(map, gns_last_map) != 0 ||
		q_strcasecmp(serverName, gns_last_hostname) != 0;

	qboolean time_due = now >= gns_next_update_time;

	if (!changed && !time_due)
		return;

	gns_last_clients = clients;
	gns_last_maxclients = maxc;
	q_strlcpy(gns_last_map, map, sizeof(gns_last_map));
	q_strlcpy(gns_last_hostname, serverName, sizeof(gns_last_hostname));

	gns_next_update_time = now + 10.0;

	Pipe_Write(
		"lobby_update %s %s %d %d",
		serverName,
		map,
		clients,
		maxc
	);
}

