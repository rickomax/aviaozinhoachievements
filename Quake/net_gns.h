#pragma once
sys_socket_t GNS_Init(void);

void GNS_Shutdown(void);

sys_socket_t GNS_Listen(qboolean state);

sys_socket_t GNS_OpenSocket(int port);

int GNS_CloseSocket(sys_socket_t socketid);

int GNS_Connect(sys_socket_t socketid, struct qsockaddr* addr);

sys_socket_t GNS_CheckNewConnections(void);

int GNS_Read(sys_socket_t socketid, byte* buf, int len, struct qsockaddr* addr);

int GNS_Write(sys_socket_t socketid, byte* buf, int len, struct qsockaddr* addr);

int GNS_Broadcast(sys_socket_t socketid, byte* buf, int len);

int GNS_GetAddresses(qhostaddr_t* addresses, int maxaddresses);

const char* GNS_AddrToString(struct qsockaddr* addr, qboolean masked);

int GNS_StringToAddr(const char* string, struct qsockaddr* addr);

int GNS_GetSocketPort(struct qsockaddr* addr);

int GNS_GetSocketAddr(sys_socket_t socketid, struct qsockaddr* addr);

int GNS_GetNameFromAddr(struct qsockaddr* addr, char* name);

int GNS_GetAddrFromName(const char* name, struct qsockaddr* addr);

int GNS_AddrCompare(struct qsockaddr* addr1, struct qsockaddr* addr2);

int GNS_SetSocketPort(struct qsockaddr* addr, int port);

void GNS_MaybeUpdateLobby(void);