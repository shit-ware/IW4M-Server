// ==========================================================
// IW4M project
// 
// Component: clientdll
// Sub-component: steam_api
// Purpose: Internet Protocol version 6 support for IW4.
//
// Initial author: NTAuthority
// Started: 2011-06-08
// ==========================================================
/*
#include "StdInc.h"
#include <winsock2.h>
#include <ws2tcpip.h>
#include <time.h>

unsigned int crc24(unsigned char *octets, size_t len)
{
	unsigned int crc = 0xb704ce;
	int i;

	while (len--)
	{
		crc ^= (*octets++) << 16;
		for (i = 0; i < 8; i++)
		{
			crc <<= 1;
			if (crc & 0x1000000)
			{
				crc ^= 0x1864cfb;
			}
		}
	}
	return crc & 0xffffff;
}

#define IP6_TABLE_SIZE 32768

typedef struct  
{
	unsigned char ip[16];
	unsigned int hash;
	time_t lastSeen;
} ip6addr_t;

static ip6addr_t ip6table[IP6_TABLE_SIZE];

bool IP6Hash_IsIP6(unsigned char* ip)
{
	return (ip[3] == 254);
}

unsigned int IP6Hash_Find(unsigned char* addr)
{
	//unsigned int hash = jenkins_one_at_a_time_hash((char*)addr, 16);
	unsigned int hash = crc24(addr, 16);

	for (int i = 0; i < IP6_TABLE_SIZE; i++)
	{
		if (ip6table[i].hash == hash)
		{
			time(&ip6table[i].lastSeen);
			return hash;
		}
	}

	return 0xFFFFFFFF;
}

unsigned int IP6Hash_Add(unsigned char* addr)
{
	time_t expirationTime;
	time(&expirationTime);
	expirationTime -= 300000;

	for (int i = 0; i < IP6_TABLE_SIZE; i++)
	{
		if (ip6table[i].hash == 0 || ip6table[i].lastSeen < expirationTime)
		{
			memcpy(&ip6table[i].ip, addr, 16);
			ip6table[i].hash = crc24(addr, 16);
			//ip6table[i].hash = jenkins_one_at_a_time_hash((char*)addr, 16);
			time(&ip6table[i].lastSeen);

			return ip6table[i].hash;
		}
	}

	Com_Error(1, "IP6Hash_Add: no free IPs");
	return 0;
}

unsigned int IP6Hash_6to4(unsigned char* ip6)
{
	unsigned int address = IP6Hash_Find(ip6);

	if (address == 0xFFFFFFFF)
	{
		address = IP6Hash_Add(ip6);
	}

	return (address | (254 << 24));
}

unsigned char* IP6Hash_4to6(unsigned int ip4)
{
	if ((ip4 >> 24) != 254)
	{
		Com_Error(1, "IP6Hash_4to6: not an IP6Hash IP");
	}

	/*unsigned int index = (ip4 & 0xFFFFFF);

	if (ip6table[index].hash)
	{
		return ip6table[index].ip;
	}*/

	unsigned int hash = (ip4 & 0xFFFFFF);

	for (int i = 0; i < IP6_TABLE_SIZE; i++)
	{
		if (ip6table[i].hash == hash)
		{
			return ip6table[i].ip;
		}
	}

	Com_Error(1, "IP6Hash_4to6: no such IP");
	return 0;
}

#define sa_family_t		ADDRESS_FAMILY
#define EAGAIN			WSAEWOULDBLOCK
#define EADDRNOTAVAIL	WSAEADDRNOTAVAIL
#define EAFNOSUPPORT	WSAEAFNOSUPPORT
#define ECONNRESET		WSAECONNRESET
#define socketError		WSAGetLastError( )


#define qtrue true
#define qfalse false
typedef int qboolean;

#define PORT_ANY -1
#define MAX_STRING_CHARS 1024

static SOCKET*	ip_socket = (SOCKET*)0x64A3008;
static SOCKET	ip6_socket = INVALID_SOCKET;

char *NET_ErrorString( void )
{
	static char netError[8192];
	FormatMessage(FORMAT_MESSAGE_FROM_SYSTEM, NULL, socketError, 0, netError, sizeof(netError), NULL);

	return netError;
}

static void NetadrToSockadr( netadr_t *a, struct sockaddr *s ) {
	if( a->type == NA_BROADCAST ) {
		((struct sockaddr_in *)s)->sin_family = AF_INET;
		((struct sockaddr_in *)s)->sin_port = a->port;
		((struct sockaddr_in *)s)->sin_addr.s_addr = INADDR_BROADCAST;
	}
	else if( a->type == NA_IP ) {
		if (IP6Hash_IsIP6(a->ip))
		{
			unsigned char* ip6 = IP6Hash_4to6(*(unsigned int*)(&a->ip));

			((struct sockaddr_in6 *)s)->sin6_family = AF_INET6;
			((struct sockaddr_in6 *)s)->sin6_addr = * ((struct in6_addr *) ip6);
			((struct sockaddr_in6 *)s)->sin6_port = a->port;
		}
		else
		{
			((struct sockaddr_in *)s)->sin_family = AF_INET;
			((struct sockaddr_in *)s)->sin_addr.s_addr = *(int *)&a->ip;
			((struct sockaddr_in *)s)->sin_port = a->port;
		}
	}
}


static void SockadrToNetadr( struct sockaddr *s, netadr_t *a ) {
	if (s->sa_family == AF_INET) {
		a->type = NA_IP;
		*(int *)&a->ip = ((struct sockaddr_in *)s)->sin_addr.s_addr;
		a->port = ((struct sockaddr_in *)s)->sin_port;
	}
	else if(s->sa_family == AF_INET6)
	{
		unsigned int ip = IP6Hash_6to4((unsigned char*)(&((struct sockaddr_in6 *)s)->sin6_addr));

		a->type = NA_IP;
		memcpy(a->ip, &ip, sizeof(a->ip));
		a->port = ((struct sockaddr_in6 *)s)->sin6_port;
	}
}


static struct addrinfo *SearchAddrInfo(struct addrinfo *hints, sa_family_t family)
{
	while(hints)
	{
		if(hints->ai_family == family)
			return hints;

		hints = hints->ai_next;
	}

	return NULL;
}

/*
=============
Sys_StringToSockaddr
=============
*/
static qboolean Sys_StringToSockaddr(const char *s, struct sockaddr *sadr, int sadr_len, sa_family_t family)
{
	struct addrinfo hints, *res = NULL, *search;
	int retval;

	memset(sadr, '\0', sizeof(*sadr));
	memset(&hints, '\0', sizeof(hints));

	hints.ai_family = family;

	retval = getaddrinfo(s, NULL, NULL, &res); //

	if(!retval)
	{
		if(family == AF_UNSPEC)
		{
			// Decide here and now which protocol family to use
			/*if((net_enabled->integer & NET_ENABLEV6) && (net_enabled->integer & NET_PRIOV6))
				search = SearchAddrInfo(res, AF_INET6);
			else*/
				search = SearchAddrInfo(res, AF_INET);

			if(!search)
			{
				/*if((net_enabled->integer & NET_ENABLEV6) &&
					(net_enabled->integer & NET_PRIOV6) &&
					(net_enabled->integer & NET_ENABLEV4))
					search = SearchAddrInfo(res, AF_INET);
				else if(net_enabled->integer & NET_ENABLEV6)*/
					search = SearchAddrInfo(res, AF_INET6);
			}
		}
		else
			search = SearchAddrInfo(res, family);

		if(search)
		{
			if((int)res->ai_addrlen > sadr_len)
				res->ai_addrlen = sadr_len;

			memcpy(sadr, res->ai_addr, res->ai_addrlen);
			freeaddrinfo(res);

			return qtrue;
		}
		else
			Com_Printf(0, "Sys_StringToSockaddr: Error resolving %s: No address of required type found.\n", s);
	}
	else
		Com_Printf(0, "Sys_StringToSockaddr: Error resolving %s: %s\n", s, gai_strerror(retval));

	if(res)
		freeaddrinfo(res);

	return qfalse;
}


/*
=============
Sys_SockaddrToString
=============
*/
static void Sys_SockaddrToString(char *dest, int destlen, struct sockaddr *input, int inputlen)
{
	getnameinfo(input, inputlen, dest, destlen, NULL, 0, NI_NUMERICHOST);
}

/*
=============
Sys_StringToAdr
=============
*/
qboolean Sys_StringToAdr( const char *s, netadr_t *a ) {
	struct sockaddr_storage sadr;
	//->sa_family_t fam;

	if( !Sys_StringToSockaddr(s, (struct sockaddr *) &sadr, sizeof(sadr), AF_UNSPEC ) ) {
		return qfalse;
	}

	SockadrToNetadr( (struct sockaddr *) &sadr, a );
	return qtrue;
}


bool NET_StringToAdr_c(const char* s, netadr_t* a)
{
	bool r;
	char base[MAX_STRING_CHARS], *search;
	char* port = NULL;

	if (!strcmp(s, "localhost"))
	{
		memset(a, 0, sizeof(*a));
		a->type = NA_LOOPBACK;
		return true;
	}

	// look for a port number
	strncpy(base, s, sizeof(base));

	if (*base == '[')
	{
		// This is an ipv6 address, handle it specially.
		search = strchr(base, ']');
		if (search)
		{
			*search = '\0';
			search++;

			if(*search == ':')
				port = search + 1;
		}

		search = base + 1;
	}
	else
	{
		port = strchr(base, ':');

		if (port)
		{
			*port = '\0';
			port++;
		}

		search = base;
	}

	r = Sys_StringToAdr(search, a) == 1;

	if (!r)
	{
		a->type = NA_BAD;
		return false;
	}

	if (port)
	{
		a->port = BigShort((short)atoi(port));
	}
	else
	{
		a->port = BigShort(28960);
	}

	return true;
}

bool NET_CompareBaseAdrSigned_c(netadr_t* a, netadr_t* b)
{
	if (a->type != b->type)
		return qfalse;

	if (a->type == NA_LOOPBACK)
		return qtrue;

	if (a->type == NA_IP)
	{
		if(!memcmp(a->ip, b->ip, sizeof(a->ip)))
			return qtrue;

		return qfalse;
	}

	/*if (a->type == NA_IP6)
	{
		if(!memcmp(a->ip6, b->ip6, sizeof(a->ip6)))
			return qtrue;

		return qfalse;
	}*/

	Com_Printf(0, "NET_CompareBaseAdrSigned: bad address type\n");
	return qfalse;
}

bool NET_CompareAdrSigned_c(netadr_t* a, netadr_t* b)
{
	if(!NET_CompareBaseAdrSigned_c(a, b))
		return qfalse;

	if (a->type == NA_IP/* || a->type == NA_IP6*/)
	{
		if (a->port == b->port)
			return qtrue;
	}
	else
		return qtrue;

	return qfalse;
}

const char	*NET_AdrToString_c (netadr_t a)
{
	static	char s[64];

	if (a.type == NA_LOOPBACK) {
		snprintf (s, sizeof(s), "loopback");
	} else if (a.type == NA_BOT) {
		snprintf (s, sizeof(s), "bot");
	}
	else if (a.type == NA_IP)
	{
		struct sockaddr_storage sadr;

		memset(&sadr, 0, sizeof(sadr));
		NetadrToSockadr(&a, (struct sockaddr *) &sadr);
		Sys_SockaddrToString(s, sizeof(s), (struct sockaddr *) &sadr, sizeof(sadr));
	}

	return s;
}

#ifdef _DEBUG
int	recvfromCount;
#endif

qboolean Sys_GetPacket_c( netadr_t *net_from, msg_t *net_message ) {
	int 	ret;
	struct sockaddr_storage from;
	socklen_t	fromlen;
	int		err;

#ifdef _DEBUG
	recvfromCount++;		// performance check
#endif

	if(*ip_socket != INVALID_SOCKET)
	{
		fromlen = sizeof(from);
		ret = recvfrom( *ip_socket, net_message->data, net_message->maxsize, 0, (struct sockaddr *)&from, &fromlen );

		if (ret == SOCKET_ERROR)
		{
			err = socketError;

			if( err != EAGAIN && err != ECONNRESET )
				Com_Printf( 0, "NET_GetPacket: %s\n", NET_ErrorString() );
		}
		else
		{

			memset( ((struct sockaddr_in *)&from)->sin_zero, 0, 8 );

			SockadrToNetadr( (struct sockaddr *) &from, net_from );
			net_message->readcount = 0;

			if( ret == net_message->maxsize ) {
				Com_Printf( 0, "Oversize packet from %s\n", NET_AdrToString (*net_from) );
				return qfalse;
			}

			net_message->cursize = ret;
			return qtrue;
		}
	}

	if(ip6_socket != INVALID_SOCKET)
	{
		fromlen = sizeof(from);
		ret = recvfrom(ip6_socket, net_message->data, net_message->maxsize, 0, (struct sockaddr *)&from, &fromlen);

		if (ret == SOCKET_ERROR)
		{
			err = socketError;

			if( err != EAGAIN && err != ECONNRESET )
				Com_Printf( 0, "NET_GetPacket: %s\n", NET_ErrorString() );
		}
		else
		{
			SockadrToNetadr((struct sockaddr *) &from, net_from);
			net_message->readcount = 0;

			if(ret == net_message->maxsize)
			{
				Com_Printf( 0, "Oversize packet from %s\n", NET_AdrToString (*net_from) );
				return qfalse;
			}

			net_message->cursize = ret;
			return qtrue;
		}
	}

	return qfalse;
}

void Sys_SendPacket_c( int length, const char *data, netadr_t to ) {
	int				ret;
	struct sockaddr_storage	addr;

	if( to.type != NA_BROADCAST && to.type != NA_IP/* && to.type != NA_IP6*/ ) {
		Com_Error( 1, "Sys_SendPacket: bad address type" );
		return;
	}

	if((*ip_socket == INVALID_SOCKET && to.type == NA_IP) || (ip6_socket == INVALID_SOCKET && IP6Hash_IsIP6(to.ip)))
		return;

	memset(&addr, 0, sizeof(addr));
	NetadrToSockadr( &to, (struct sockaddr *) &addr );

	if(addr.ss_family == AF_INET)
		ret = sendto( *ip_socket, data, length, 0, (struct sockaddr *) &addr, sizeof(addr) );
	else
		ret = sendto( ip6_socket, data, length, 0, (struct sockaddr *) &addr, sizeof(addr) );

	if( ret == SOCKET_ERROR ) {
		int err = socketError;

		// wouldblock is silent
		if( err == EAGAIN ) {
			return;
		}

		// some PPP links do not allow broadcasts and return an error
		if( ( err == EADDRNOTAVAIL ) && ( ( to.type == NA_BROADCAST ) ) ) {
			return;
		}

		Com_Printf( 0, "Sys_SendPacket: %s\n", NET_ErrorString() );
	}
}

/*
====================
NET_IP6Socket
====================
*/
int NET_IP6Socket( char *net_interface, int port, int *err ) {
	SOCKET				newsocket;
	struct sockaddr_in6	address;
	qboolean			_true = qtrue;
	int					i = 1;

	memset(&address, 0, sizeof(address));

	*err = 0;

	if( net_interface ) {
		Com_Printf( 0, "Opening IP6 socket: [%s]:%i\n", net_interface, port );
	}
	else {
		Com_Printf( 0, "Opening IP6 socket: localhost:%i\n", port );
	}

	if( ( newsocket = socket( PF_INET6, SOCK_DGRAM, IPPROTO_UDP ) ) == INVALID_SOCKET ) {
		*err = socketError;
		Com_Printf( 0, "WARNING: NET_IP6Socket: socket: %s\n", NET_ErrorString() );
		return newsocket;
	}

	// make it non-blocking
	if( ioctlsocket( newsocket, FIONBIO, (u_long *)&_true ) == SOCKET_ERROR ) {
		Com_Printf( 0, "WARNING: NET_IP6Socket: ioctl FIONBIO: %s\n", NET_ErrorString() );
		*err = socketError;
		closesocket(newsocket);
		return INVALID_SOCKET;
	}

	// ipv4 addresses should not be allowed to connect via this socket.
	if(setsockopt(newsocket, IPPROTO_IPV6, IPV6_V6ONLY, (char *) &i, sizeof(i)) == SOCKET_ERROR)
	{
		// win32 systems don't seem to support this anyways.
		Com_Printf(0, "WARNING: NET_IP6Socket: setsockopt IPV6_V6ONLY: %s\n", NET_ErrorString());
	}

	// make it broadcast capable
	if( setsockopt( newsocket, SOL_SOCKET, SO_BROADCAST, (char *) &i, sizeof(i) ) == SOCKET_ERROR ) {
		Com_Printf( 0, "WARNING: NET_IP6Socket: setsockopt SO_BROADCAST: %s\n", NET_ErrorString() );
		//		return newsocket;
	}

	if( !net_interface || !net_interface[0] || !_stricmp(net_interface, "localhost") ) {
		address.sin6_family = AF_INET6;
		address.sin6_addr = in6addr_any;
	}
	else
	{
		if(!Sys_StringToSockaddr( net_interface, (struct sockaddr *)&address, sizeof(address), AF_INET6))
		{
			closesocket(newsocket);
			return INVALID_SOCKET;
		}
	}

	if( port == PORT_ANY ) {
		address.sin6_port = 0;
	}
	else {
		address.sin6_port = htons( (short)port );
	}

	if( bind( newsocket, (const sockaddr*)&address, sizeof(address) ) == SOCKET_ERROR ) {
		Com_Printf( 0, "WARNING: NET_IP6Socket: bind: %s\n", NET_ErrorString() );
		*err = socketError;
		closesocket( newsocket );
		return INVALID_SOCKET;
	}

	return newsocket;
}

dvar_t* net_ip6;
dvar_t* net_port6;

void NET_OpenIP6()
{
	int i;
	int err;
	int port6;

	net_ip6 = (dvar_t*)Dvar_RegisterString("net_ip6", "localhost", DVAR_FLAG_LATCHED, "Network IP6 Address");
	net_port6 = Dvar_RegisterInt("net_port6", 28960, 0, 65535, DVAR_FLAG_LATCHED, "Network IP6 port");

	port6 = net_port6->current.integer;
	for( i = 0 ; i < 10 ; i++ )
	{
		ip6_socket = NET_IP6Socket(net_ip6->current.string, port6 + i, &err);
		if (ip6_socket != INVALID_SOCKET)
		{
			//Cvar_SetValue( "net_port6", port6 + i );
			break;
		}
		else
		{
			if(err == EAFNOSUPPORT)
				break;
		}
	}

	if(ip6_socket == INVALID_SOCKET)
		Com_Printf( 0, "WARNING: Couldn't bind to a v6 ip address.\n");
}

void __declspec(naked) NET_OpenIP6Stub()
{
	__asm
	{
		call NET_OpenIP6
		mov eax, 64D900h
		jmp eax
	}
}

StompHook netStringToAdrHook;
DWORD netStringToAdrHookLoc = 0x409010;

StompHook netCompareBaseAdrSignedHook;
DWORD netCompareBaseAdrSignedHookLoc = 0x457740;

StompHook netCompareAdrSignedHook;
DWORD netCompareAdrSignedHookLoc = 0x60FA30;

StompHook netAdrToStringHook;
DWORD netAdrToStringHookLoc = 0x469880;

StompHook netGetPacketHook;
DWORD netGetPacketHookLoc = 0x4ECFF0;

StompHook netSendPacketHook;
DWORD netSendPacketHookLoc = 0x48F500;

StompHook netOpenIPHook;
DWORD netOpenIPHookLoc = 0x4FD4D4;

void PatchMW2_ProtocolSix()
{
	netStringToAdrHook.initialize(netStringToAdrHookLoc, NET_StringToAdr_c);
	netStringToAdrHook.installHook();

	netCompareBaseAdrSignedHook.initialize(netCompareBaseAdrSignedHookLoc, NET_CompareBaseAdrSigned_c);
	netCompareBaseAdrSignedHook.installHook();

	netCompareAdrSignedHook.initialize(netCompareAdrSignedHookLoc, NET_CompareAdrSigned_c);
	netCompareAdrSignedHook.installHook();

	netAdrToStringHook.initialize(netAdrToStringHookLoc, NET_AdrToString_c);
	netAdrToStringHook.installHook();

	netGetPacketHook.initialize(netGetPacketHookLoc, Sys_GetPacket_c);
	netGetPacketHook.installHook();

	netSendPacketHook.initialize(netSendPacketHookLoc, Sys_SendPacket_c);
	netSendPacketHook.installHook();

	netOpenIPHook.initialize(netOpenIPHookLoc, NET_OpenIP6Stub);
	netOpenIPHook.installHook();

	// TODO (if existent): Sys_IsLanAddress

}