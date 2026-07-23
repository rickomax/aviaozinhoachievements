#pragma once
#ifdef __cplusplus
extern "C" {
#endif
extern cvar_t steamserver;

#define GNS_MAX_SOCKETS 9999999
#define GNS_PREFIX "steam-conn|"

qboolean validSteamId(const char* address);
qboolean isSteamAddr(const struct qsockaddr* addr);
uint64_t decodeSteamId(const struct qsockaddr* qaddr);
void encodeSteamId(struct qsockaddr* qaddr, uint64_t steamId);
int tryEncodeSteamId(const char* string, struct qsockaddr* addr);

//void gns_clientremoved(struct qsockaddr* addr);
int gns_init(void);
void gns_shutdown(void);
int gns_socket(int domain, int type, int protocol);
int gns_bind(int s, const struct sockaddr* addr, int addrlen);
int gns_sendto(int s, const void* buf, int len, int flags, const struct sockaddr* to, int tolen);
int gns_recvfrom(int s, void* buf, int len, int flags, struct sockaddr* from, int* fromlen);
int gns_closesocket(int s);
int gns_ioctlsocket(int s, long cmd, u_long* argp);
int gns_setsockopt(int socket, int level, int option_name, const void* option_value, socklen_t option_len);
#ifdef __cplusplus
}
#endif