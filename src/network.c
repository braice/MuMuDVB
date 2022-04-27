/*
 * udp.h fonction in order to send a multicast stream
 * mumudvb - UDP-ize a DVB transport stream.
 *
 * (C) 2004-2013 Brice DUBOST
 * (C) Dave Chapman <dave@dchapman.com> 2001, 2002.
 *
 *  The latest version can be found at http://mumudvb.net
 *
 * Copyright notice:
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 675 Mass Ave, Cambridge, MA 02139, USA.
 *
 */

/**@file
 * @brief Networking functions
 */

#define _CRT_SECURE_NO_WARNINGS

#include "network.h"
#include "errors.h"
#include <string.h>
#include <errno.h>
#include <fcntl.h>
#include "log.h"
#ifndef _WIN32
#include <net/if.h>
#include <unistd.h>
#else
#define close(sock) closesocket(sock)
#endif

static char *log_module="Network: ";

int is_multicast(struct sockaddr_storage *sa)
{
    int rv = -1;
    struct sockaddr_in *addr4;
    struct sockaddr_in6 *addr6;

    switch (sa->ss_family) {
        case AF_INET:
            addr4 = (struct sockaddr_in *)sa;
            rv = IN_MULTICAST(ntohl(addr4->sin_addr.s_addr));
            break;

        case AF_INET6:
            addr6 = (struct sockaddr_in6 *)sa;
            rv = IN6_IS_ADDR_MULTICAST(&addr6->sin6_addr);
            break;

        default:
            break;
    }

    return rv;
}

/**@brief Send data
 * just send the data over the socket fd
 */
void sendudp(int fd, struct sockaddr_in *sSockAddr, unsigned char *data, int len)
{
    int ret;
    ret = sendto(fd, (const char *)data, len, 0, (const struct sockaddr *)sSockAddr, sizeof(struct sockaddr_in));
    if (ret < 0)
        log_message(log_module, MSG_WARN, "sendto failed : %s\n", strerror(errno));
}

/**@brief Send data
 * just send the data over the socket fd
 */
void sendudp6(int fd, struct sockaddr_in6 *sSockAddr, unsigned char *data, int len)
{
    int ret;
    ret = sendto(fd, (const char *)data, len, 0, (const struct sockaddr *)sSockAddr, sizeof(struct sockaddr_in6));
    if (ret < 0)
        log_message(log_module, MSG_WARN, "sendto failed : %s\n", strerror(errno));
}

/** @brief create a sender socket.
 *
 * Create a socket for sending data, the socket is multicast, udp, with the options REUSE_ADDR et MULTICAST_LOOP set to 1
 */
int makesocket(char *szAddr, unsigned short port, int TTL, char *iface, struct sockaddr_in *sSockAddr)
{
    int iRet, iLoop = 1;
    struct sockaddr_in sin;
    int iReuse = 1;
    int iSocket = socket(AF_INET, SOCK_DGRAM, 0);

	if (iSocket < 0) {
        log_message(log_module, MSG_WARN, "makesocket() socket() failed : %s\n", strerror(errno));
        set_interrupted(ERROR_NETWORK << 8);
        return -1;
    }

    sSockAddr->sin_family = sin.sin_family = AF_INET;
    sSockAddr->sin_port = sin.sin_port = htons(port);
    iRet = inet_pton(AF_INET, szAddr, &sSockAddr->sin_addr);
    if (iRet == 0) {
        log_message(log_module, MSG_ERROR, "inet_pton failed : %s\n", strerror(errno));
        set_interrupted(ERROR_NETWORK << 8);
        close(iSocket);
        return -1;
    }

    iRet = setsockopt(iSocket, SOL_SOCKET, SO_REUSEADDR, (const char *)&iReuse, sizeof(int));
    if (iRet < 0) {
        log_message(log_module, MSG_ERROR, "setsockopt SO_REUSEADDR failed : %s\n", strerror(errno));
        set_interrupted(ERROR_NETWORK << 8);
        close(iSocket);
        return -1;
    }

    iRet = setsockopt(iSocket, IPPROTO_IP, IP_MULTICAST_TTL, (const char *)&TTL, sizeof(int));
    if (iRet < 0) {
        log_message(log_module, MSG_ERROR, "setsockopt IP_MULTICAST_TTL failed.  multicast in kernel? error : %s \n", strerror(errno));
        set_interrupted(ERROR_NETWORK << 8);
        close(iSocket);
        return -1;
    }

    iRet = setsockopt(iSocket, IPPROTO_IP, IP_MULTICAST_LOOP, (const char *)&iLoop, sizeof(int));
    if (iRet < 0) {
        log_message(log_module, MSG_ERROR, "setsockopt IP_MULTICAST_LOOP failed.  multicast in kernel? error : %s\n", strerror(errno));
        set_interrupted(ERROR_NETWORK << 8);
        close(iSocket);
        return -1;
    }

    if (strlen(iface)) {
        int iface_index;
        iface_index = if_nametoindex(iface);
        if (iface_index) {
            log_message(log_module, MSG_DEBUG, "Setting IPv4 multicast iface to %s, index %d", iface, iface_index);
#ifdef _WIN32
            struct ip_mreq iface_struct;
#else
            struct ip_mreqn iface_struct;
#endif
            iface_struct.imr_multiaddr.s_addr = INADDR_ANY;
#ifndef _WIN32
            iface_struct.imr_address.s_addr = INADDR_ANY;
            iface_struct.imr_ifindex = iface_index;
#endif
            iRet = setsockopt(iSocket, IPPROTO_IP, IP_MULTICAST_IF, (const char *)&iface_struct, sizeof(iface_struct));
            if (iRet < 0) {
                log_message(log_module, MSG_ERROR, "setsockopt IP_MULTICAST_IF failed.  multicast in kernel? error : %s \n", strerror(errno));
                set_interrupted(ERROR_NETWORK << 8);
                close(iSocket);
                return -1;
            }
        } else
            log_message(log_module, MSG_ERROR, "Setting IPV4 multicast interface: Interface %s does not exist", iface);
    }

    return iSocket;
}

/** @brief create an IPv6 sender socket.
 *
 * Create a socket for sending data, the socket is multicast, udp, with the options REUSE_ADDR et MULTICAST_LOOP set to 1
 */
int makesocket6(char *szAddr, unsigned short port, int TTL, char *iface, struct sockaddr_in6 *sSockAddr)
{
    int iRet;
    int iReuse = 1;
    struct sockaddr_in6 sin;

    int iSocket = socket(AF_INET6, SOCK_DGRAM, IPPROTO_UDP);

    if (iSocket < 0) {
        log_message(log_module, MSG_WARN, "socket() failed : %s\n", strerror(errno));
        set_interrupted(ERROR_NETWORK << 8);
        return -1;
    }

    sSockAddr->sin6_family = sin.sin6_family = AF_INET6;
    sSockAddr->sin6_port = sin.sin6_port = htons(port);
    iRet = inet_pton(AF_INET6, szAddr, &sSockAddr->sin6_addr);
    if (iRet != 1) {
        log_message(log_module, MSG_ERROR, "inet_pton failed : %s\n", strerror(errno));
        set_interrupted(ERROR_NETWORK << 8);
        close(iSocket);
        return -1;
    }

    iRet = setsockopt(iSocket, SOL_SOCKET, SO_REUSEADDR, (const char *)&iReuse, sizeof(int));
    if (iRet < 0) {
        log_message(log_module, MSG_ERROR, "setsockopt SO_REUSEADDR failed : %s\n", strerror(errno));
        set_interrupted(ERROR_NETWORK << 8);
        close(iSocket);
        return -1;
    }
    iRet = setsockopt(iSocket, IPPROTO_IPV6, IPV6_MULTICAST_HOPS, (const char *)&TTL, sizeof(int));
    if (iRet < 0) {
        log_message(log_module, MSG_ERROR, "setsockopt IPV6_MULTICAST_HOPS failed.  multicast in kernel? error : %s \n", strerror(errno));
        set_interrupted(ERROR_NETWORK << 8);
        close(iSocket);
        return -1;
    }
    if (strlen(iface)) {
        int iface_index;
        iface_index = if_nametoindex(iface);
        if (iface_index) {
            log_message(log_module, MSG_DEBUG, "Setting IPv6 multicast iface to %s, index %d", iface, iface_index);
            iRet = setsockopt(iSocket, IPPROTO_IPV6, IPV6_MULTICAST_IF, (const char *)&iface_index, sizeof(int));
            if (iRet < 0) {
                log_message(log_module, MSG_ERROR, "setsockopt IPV6_MULTICAST_IF failed.  multicast in kernel? error : %s \n", strerror(errno));
                set_interrupted(ERROR_NETWORK << 8);
                close(iSocket);
                return -1;
            }
        } else
            log_message(log_module, MSG_ERROR, "Setting IPv6 multicast interface: Interface %s does not exist", iface);

    }

    return iSocket;
}

/** @brief create a receiver socket, i.e. join the multicast group.
 *@todo document
 */
int
makeclientsocket (char *szAddr, unsigned short port, int TTL, char *iface, struct sockaddr_in *sSockAddr)
{
	int socket;
#ifdef _WIN32
	struct ip_mreq blub;
#else
	struct ip_mreqn blub;
#endif
	struct sockaddr_in sin;
	memset(&sin, 0, sizeof sin);
	uint32_t tempaddr;
	socket = makesocket (szAddr, port, TTL, iface, sSockAddr);
	if(socket<0)
		return 0;
	sin.sin_family = AF_INET;
	sin.sin_port = htons (port);
	inet_pton(AF_INET, szAddr, &sin.sin_addr.s_addr);
	if (bind (socket, (struct sockaddr *) &sin, sizeof (sin)))
	{
		log_message( log_module,  MSG_ERROR, "bind failed : %s\n", strerror(errno));
		set_interrupted(ERROR_NETWORK<<8);
	}
	inet_pton(AF_INET, szAddr, &tempaddr);
	if ((ntohl(tempaddr) >> 28) == 0xe)
	{
		inet_pton(AF_INET, szAddr, &blub.imr_multiaddr.s_addr);
#ifndef _WIN32
		if(strlen(iface))
			blub.imr_ifindex=if_nametoindex(iface);
		else
			blub.imr_ifindex = 0;
#endif
        if (setsockopt(socket, IPPROTO_IP, IP_ADD_MEMBERSHIP, (const char *)&blub, sizeof(blub)))
		{
			log_message( log_module,  MSG_ERROR, "setsockopt IP_ADD_MEMBERSHIP ipv4 failed (multicast kernel?) : %s\n", strerror(errno));
			set_interrupted(ERROR_NETWORK<<8);
		}
	}
	return socket;
}

/** @brief create a receiver socket, i.e. join the multicast group.
 *@todo document
 */
int
makeclientsocket6 (char *szAddr, unsigned short port, int TTL, char *iface,
		struct sockaddr_in6 *sSockAddr)
{
	int socket = makesocket6 (szAddr, port, TTL, iface, sSockAddr);
	if (socket < 0)
		return -1;
	struct ipv6_mreq blub;
	struct sockaddr_in6 sin;
	memset(&sin, 0, sizeof sin);
	int iRet;
	sin.sin6_family = AF_INET6;
	sin.sin6_port = htons (port);
	iRet=inet_pton (AF_INET6, szAddr,&sin.sin6_addr);
	if (iRet != 1)
	{
		log_message( log_module,  MSG_ERROR,"inet_pton failed : %s\n", strerror(errno));
		close(socket);
		set_interrupted(ERROR_NETWORK<<8);
		return 0;
	}

	if (bind (socket, (struct sockaddr *) &sin, sizeof (sin)))
	{
		log_message( log_module,  MSG_ERROR, "bind failed : %s\n", strerror(errno));
		close(socket);
		set_interrupted(ERROR_NETWORK<<8);
		return 0;
	}

	//join the group
    iRet = inet_pton(AF_INET6, szAddr, &blub.ipv6mr_multiaddr);
	if (iRet != 1)
	{
		log_message( log_module,  MSG_ERROR,"inet_pton failed : %s\n", strerror(errno));
		set_interrupted(ERROR_NETWORK<<8);
		close(socket);
		return -1;
	}

	if(strlen(iface))
		blub.ipv6mr_interface=if_nametoindex(iface);
	else
		blub.ipv6mr_interface = 0;
    if (setsockopt(socket, IPPROTO_IPV6, IPV6_JOIN_GROUP, (const char *)&blub, sizeof(blub)))
	{
		log_message( log_module,  MSG_ERROR, "setsockopt IPV6_JOIN_GROUP (ipv6) failed (multicast kernel?) : %s\n", strerror(errno));
		set_interrupted(ERROR_NETWORK<<8);
	}
	return socket;
}

int makeUDPclientsocket(char *szAddr, unsigned short port)
{
    int rv, sock, opt;
    struct addrinfo hints = { 0, };
    struct addrinfo *result = NULL;
    char cPort[6] = { 0, };

    /* prepare hints for conversion */
    hints.ai_family = AF_UNSPEC;
    hints.ai_socktype = SOCK_DGRAM;
    hints.ai_protocol = IPPROTO_UDP; /* UDP socket */
    hints.ai_flags = AI_NUMERICHOST | AI_NUMERICSERV;
    snprintf(cPort, sizeof(cPort), "%d", port);

    rv = getaddrinfo(szAddr, cPort, &hints, &result);
    if (rv != 0) {
        log_message(log_module, MSG_ERROR, "getaddrinfo failed : %s\n", strerror(errno));
        return -1;
    }

    /* use the first result from getaddrinfo as its numeric, should be the only one */
    sock = socket(result->ai_family, result->ai_socktype, result->ai_protocol);
    if (sock < 0) {
        log_message(log_module, MSG_ERROR, "socket() failed.\n");
        rv = -1;
        goto cleanup;
    }

    /* set socket options */
    opt = 1;
    rv = setsockopt(sock, SOL_SOCKET, SO_REUSEADDR, (const char *)&opt, sizeof(int));
    if (rv < 0) {
        log_message(log_module, MSG_ERROR, "setsockopt SO_REUSEADDR failed : %s\n", strerror(errno));
        rv = -1;
        goto cleanup;
    }

    opt = 0x80000;
    rv = setsockopt(sock, SOL_SOCKET, SO_RCVBUF, (void *)&opt, sizeof(int));
    if (rv < 0) {
        log_message(log_module, MSG_ERROR, "setsockopt SO_RCVBUF failed : %s\n", strerror(errno));
        rv = -1;
        goto cleanup;
    }

    /* bind to the socket */
    if (bind(sock, result->ai_addr, result->ai_addrlen)) {
        log_message(log_module, MSG_ERROR, "bind failed : %s\n", strerror(errno));
        rv = -1;
        goto cleanup;
    }

    /* Try join multicast group */
    if (is_multicast((struct sockaddr_storage *)result->ai_addr)) {
        struct ip_mreq v4req;
        struct ipv6_mreq v6req;
        size_t mlen = 0;
        const char *req = NULL;
        bool v6 = false;

        switch (result->ai_family) {
            case AF_INET:
                v4req.imr_multiaddr.s_addr = ((struct in_addr *)result->ai_addr)->s_addr;
                v4req.imr_interface.s_addr = INADDR_ANY;
                req = (const char *)&v4req;
                mlen = sizeof(v4req);
                break;
            case AF_INET6:
                memcpy(&v6req.ipv6mr_multiaddr, result->ai_addr, sizeof(v6req.ipv6mr_multiaddr));
                v6req.ipv6mr_interface = 0;
                req = (const char *)&v6req;
                mlen = sizeof(v6req);
                v6 = true;
                break;
            default:
                break;
        }
        if (setsockopt(sock, v6 ? IPPROTO_IPV6 : IPPROTO_IP, v6 ? IPV6_JOIN_GROUP : IP_ADD_MEMBERSHIP, req, mlen))
            log_message(log_module, MSG_ERROR, "setsockopt IP_ADD_MEMBERSHIP/IPV6_JOIN_GROUP failed : %s\n", strerror(errno));
    }

    rv = 0;

cleanup:
    freeaddrinfo(result);
    if (rv < 0) {
        if (sock > 0)
            close(sock);
        sock = -1;
    }

    return sock;
}

/** @brief create a TCP receiver socket.
 *
 * Create a socket for waiting the HTTP connection, on either IPv4 or IPv6 interface
 */
int makeTCPclientsocket(char *szAddr, unsigned short port)
{
	int iRet, iLoop = 1;
	struct addrinfo hints = { 0, };
	struct addrinfo *result = NULL;
	char cPort[6] = { 0, };

	/* prepare hints for conversion */
	hints.ai_family = AF_UNSPEC;
	hints.ai_socktype = SOCK_STREAM;
	hints.ai_protocol = IPPROTO_TCP; /* TCP socket */
	hints.ai_flags = AI_NUMERICHOST | AI_NUMERICSERV;
	snprintf(cPort, sizeof(cPort), "%d", port);

	iRet = getaddrinfo(szAddr, cPort, &hints, &result);
	if (iRet != 0) {
		log_message(log_module, MSG_ERROR, "getaddrinfo failed : %s\n", strerror(errno));
		return -1;
	}

	/* use the first result from getaddrinfo as its numeric, should be the only one */
	int iSocket = socket(result->ai_family, result->ai_socktype, result->ai_protocol);
	if (iSocket < 0)
	{
		log_message( log_module,  MSG_ERROR, "socket() failed.\n");
		freeaddrinfo(result);
		return -1;
	}

	iRet = setsockopt(iSocket, SOL_SOCKET, SO_REUSEADDR, (const char *)&iLoop, sizeof(int));
	if (iRet < 0) {
		log_message(log_module, MSG_ERROR, "setsockopt SO_REUSEADDR failed : %s\n", strerror(errno));
		freeaddrinfo(result);
		close(iSocket);
		return -1;
	}

	if (bind(iSocket, result->ai_addr, result->ai_addrlen)) {
		log_message( log_module,  MSG_ERROR, "bind failed : %s\n", strerror(errno));
		freeaddrinfo(result);
		close(iSocket);
		return -1;
	}

	/* free the result here */
	freeaddrinfo(result);

	iRet = listen(iSocket,10);
	if (iRet < 0)
	{
		log_message( log_module,  MSG_ERROR,"listen failed : %s\n",strerror(errno));
		close(iSocket);
		return -1;
	}

	//Now we set this socket to be non blocking because we poll it
	int flags;
#ifndef _WIN32
	flags = fcntl(iSocket, F_GETFL, 0);
	flags |= O_NONBLOCK;
	if (fcntl(iSocket, F_SETFL, flags) < 0)
	{
		log_message( log_module, MSG_ERROR,"Set non blocking failed : %s\n",strerror(errno));
		close(iSocket);
		return -1;
	}
#else
    u_long iMode = 0;
	flags = ioctlsocket(iSocket, FIONBIO, &iMode);
	if (flags != NO_ERROR) {
		log_message(log_module, MSG_ERROR, "Set non blocking failed : %s\n", strerror(errno));
		close(iSocket);
		return -1;
	}
#endif

	return iSocket;
}

int socket_to_string(int sock, char *pDest, size_t len)
{
    struct sockaddr_storage addr = { 0, };
    socklen_t l = sizeof(struct sockaddr_storage);
    int rv;

    if (!pDest)
        return -1;

    /* get sockaddr for socket */
    rv = getsockname(sock, (struct sockaddr *)&addr, &l);
    if (rv == 0)
        return sockaddr_to_string(&addr, pDest, len);

    log_message(log_module, MSG_ERROR, "getsockname failed : %s", strerror(errno));
    return rv;
}

int sockaddr_to_string(struct sockaddr_storage *sa, char *pDest, size_t len)
{
    char cPort[6] = { 0, };
    int rv;

    if (!pDest)
        return -1;

    /* turn into text string IP */
    rv = getnameinfo((struct sockaddr *)sa, sizeof(struct sockaddr_storage), pDest, len, cPort, sizeof(cPort), NI_NUMERICHOST | NI_NUMERICSERV);

    /* append port. pDest has enough space beacuse it's IPV6_CHAR_LEN size */
    if (rv == 0) {
        strcat(pDest, ":");
        strcat(pDest, cPort);
    }

    return rv;
}

int socket_to_string_port(int sock, char *pDestAddr, size_t destLen, char *pDestPort, size_t portLen)
{
    struct sockaddr_storage sa;
    socklen_t l = sizeof(struct sockaddr_storage);
    int rv;

    rv = getsockname(sock, (struct sockaddr *)&sa, &l);
    if (rv == 0)
        rv = getnameinfo((struct sockaddr *)&sa, sizeof(struct sockaddr_storage), pDestAddr, destLen, pDestPort, portLen, NI_NUMERICHOST | NI_NUMERICSERV);

    return rv;
}
