/*
 network.c : Network stuff

    Copyright (C) 1999-2000 Timo Sirainen

    This program is free software; you can redistribute it and/or modify
    it under the terms of the GNU General Public License as published by
    the Free Software Foundation; either version 2 of the License, or
    (at your option) any later version.

    This program is distributed in the hope that it will be useful,
    but WITHOUT ANY WARRANTY; without even the implied warranty of
    MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
    GNU General Public License for more details.

    You should have received a copy of the GNU General Public License
    along with this program; if not, write to the Free Software
    Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA  02111-1307  USA
*/

#include "module.h"
#include "network.h"
#include "net-internal.h"

#ifndef INADDR_NONE
#  define INADDR_NONE INADDR_BROADCAST
#endif

union sockaddr_union {
	struct sockaddr sa;
	struct sockaddr_in sin;
#ifdef HAVE_IPV6
	struct sockaddr_in6 sin6;
#endif
};

#ifdef HAVE_IPV6
#  define SIZEOF_SOCKADDR(so) ((so).sa.sa_family == AF_INET6 ? \
	sizeof(so.sin6) : sizeof(so.sin))
#else
#  define SIZEOF_SOCKADDR(so) (sizeof(so.sin))
#endif

#ifdef WIN32
#  define g_io_channel_new(handle) g_io_channel_win32_new_stream_socket(handle)
#else
#  define g_io_channel_new(handle) g_io_channel_unix_new(handle)
#endif

/* Cygwin need this, don't know others.. */
/*#define BLOCKING_SOCKETS 1*/

int net_ip_compare(IPADDR *ip1, IPADDR *ip2)
{
	if (ip1->family != ip2->family)
		return 0;

#ifdef HAVE_IPV6
	if (ip1->family == AF_INET6)
		return memcmp(&ip1->addr, &ip2->addr, sizeof(ip1->addr)) == 0;
#endif

	return memcmp(&ip1->addr, &ip2->addr, 4) == 0;
}


/* copy IP to sockaddr */
#ifdef G_CAN_INLINE
G_INLINE_FUNC
#else
static
#endif
void sin_set_ip(union sockaddr_union *so, const IPADDR *ip)
{
	if (ip == NULL) {
#ifdef HAVE_IPV6
		so->sin6.sin6_family = AF_INET6;
		so->sin6.sin6_addr = in6addr_any;
#else
		so->sin.sin_family = AF_INET;
		so->sin.sin_addr.s_addr = INADDR_ANY;
#endif
		return;
	}

	so->sin.sin_family = ip->family;
#ifdef HAVE_IPV6
	if (ip->family == AF_INET6)
		memcpy(&so->sin6.sin6_addr, &ip->addr, sizeof(ip->addr.ip6));
	else
#endif
		memcpy(&so->sin.sin_addr, &ip->addr, 4);
}

void sin_get_ip(const union sockaddr_union *so, IPADDR *ip)
{
	ip->family = so->sin.sin_family;

#ifdef HAVE_IPV6
	if (ip->family == AF_INET6)
		memcpy(&ip->addr, &so->sin6.sin6_addr, sizeof(ip->addr.ip6));
	else
#endif
		memcpy(&ip->addr, &so->sin.sin_addr, 4);
}

#ifdef G_CAN_INLINE
G_INLINE_FUNC
#else
static
#endif
void sin_set_port(union sockaddr_union *so, int port)
{
#ifdef HAVE_IPV6
	if (so->sin.sin_family == AF_INET6)
                so->sin6.sin6_port = htons(port);
	else
#endif
		so->sin.sin_port = htons((unsigned short)port);
}

#ifdef G_CAN_INLINE
G_INLINE_FUNC
#else
static
#endif
int sin_get_port(union sockaddr_union *so)
{
#ifdef HAVE_IPV6
	if (so->sin.sin_family == AF_INET6)
		return ntohs(so->sin6.sin6_port);
#endif
	return ntohs(so->sin.sin_port);
}

/* Connect to socket */
GIOChannel *net_connect(const char *addr, int port, IPADDR *my_ip)
{
	IPADDR ip;

	g_return_val_if_fail(addr != NULL, NULL);

	if (net_gethostbyname(addr, &ip) == -1)
		return NULL;

	return net_connect_ip(&ip, port, my_ip);
}

/* Connect to socket with ip address */
GIOChannel *net_connect_ip(IPADDR *ip, int port, IPADDR *my_ip)
{
	union sockaddr_union so;
	int handle, ret, opt = 1;

	/* create the socket */
	memset(&so, 0, sizeof(so));
        so.sin.sin_family = ip->family;
	handle = socket(ip->family, SOCK_STREAM, 0);

	if (handle == -1)
		return NULL;

	/* set socket options */
#ifndef WIN32
	fcntl(handle, F_SETFL, O_NONBLOCK);
#endif
	setsockopt(handle, SOL_SOCKET, SO_REUSEADDR,
		   (char *) &opt, sizeof(opt));
	setsockopt(handle, SOL_SOCKET, SO_KEEPALIVE,
		   (char *) &opt, sizeof(opt));

	/* set our own address, ignore if bind() fails */
	if (my_ip != NULL) {
		sin_set_ip(&so, my_ip);
		bind(handle, &so.sa, SIZEOF_SOCKADDR(so));
	}

	/* connect */
	sin_set_ip(&so, ip);
	sin_set_port(&so, port);
	ret = connect(handle, &so.sa, SIZEOF_SOCKADDR(so));

#ifndef WIN32
	if (ret < 0 && errno != EINPROGRESS) {
#else
	if (ret < 0 && WSAGetLastError() != WSAEWOULDBLOCK) {
#endif
		close(handle);
		return NULL;
	}

	return g_io_channel_new(handle);
}

/* Disconnect socket */
void net_disconnect(GIOChannel *handle)
{
	g_return_if_fail(handle != NULL);

	g_io_channel_close(handle);
	g_io_channel_unref(handle);
}

/* Listen for connections on a socket. if `my_ip' is NULL, listen in any
   address. */
GIOChannel *net_listen(IPADDR *my_ip, int *port)
{
	union sockaddr_union so;
	int ret, handle, opt = 1;
	socklen_t len;

	g_return_val_if_fail(port != NULL, NULL);

	memset(&so, 0, sizeof(so));
	sin_set_port(&so, *port);
	sin_set_ip(&so, my_ip);

	/* create the socket */
	handle = socket(so.sin.sin_family, SOCK_STREAM, 0);
	if (handle == -1)
		return NULL;

	/* set socket options */
#ifndef WIN32
	fcntl(handle, F_SETFL, O_NONBLOCK);
#endif
	setsockopt(handle, SOL_SOCKET, SO_REUSEADDR,
		   (char *) &opt, sizeof(opt));
	setsockopt(handle, SOL_SOCKET, SO_KEEPALIVE,
		   (char *) &opt, sizeof(opt));

	/* specify the address/port we want to listen in */
	ret = bind(handle, &so.sa, SIZEOF_SOCKADDR(so));
	if (ret >= 0) {
		/* get the actual port we started listen */
		len = SIZEOF_SOCKADDR(so);
		ret = getsockname(handle, &so.sa, &len);
		if (ret >= 0) {
			*port = sin_get_port(&so);

			/* start listening */
			if (listen(handle, 1) >= 0)
                                return g_io_channel_new(handle);
		}

	}

        /* error */
	close(handle);
	return NULL;
}

/* Accept a connection on a socket */
GIOChannel *net_accept(GIOChannel *handle, IPADDR *addr, int *port)
{
	union sockaddr_union so;
	int ret;
	socklen_t addrlen;

	g_return_val_if_fail(handle != NULL, NULL);

	addrlen = sizeof(so);
	ret = accept(g_io_channel_unix_get_fd(handle), &so.sa, &addrlen);

	if (ret < 0)
		return NULL;

	if (addr != NULL) sin_get_ip(&so, addr);
	if (port != NULL) *port = sin_get_port(&so);

#ifndef WIN32
	fcntl(ret, F_SETFL, O_NONBLOCK);
#endif
	return g_io_channel_new(ret);
}

/* Read data from socket, return number of bytes read, -1 = error */
int net_receive(GIOChannel *handle, char *buf, int len)
{
        unsigned int ret;
	int err;

	g_return_val_if_fail(handle != NULL, -1);
	g_return_val_if_fail(buf != NULL, -1);

	err = g_io_channel_read(handle, buf, len, &ret);
	if (err == 0 && ret == 0)
		return -1; /* disconnected */

	if (err == G_IO_ERROR_AGAIN || (err != 0 && errno == EINTR))
		return 0; /* no bytes received */

	return err == 0 ? (int)ret : -1;
}

/* Transmit data, return number of bytes sent, -1 = error */
int net_transmit(GIOChannel *handle, const char *data, int len)
{
        unsigned int ret;
	int err;

	g_return_val_if_fail(handle != NULL, -1);
	g_return_val_if_fail(data != NULL, -1);

	err = g_io_channel_write(handle, (char *) data, len, &ret);
	if (err == G_IO_ERROR_AGAIN ||
	    (err != 0 && (errno == EINTR || errno == EPIPE)))
		return 0;

	return err == 0 ? (int)ret : -1;
}

/* Get socket address/port */
int net_getsockname(GIOChannel *handle, IPADDR *addr, int *port)
{
	union sockaddr_union so;
	socklen_t addrlen;

	g_return_val_if_fail(handle != NULL, -1);
	g_return_val_if_fail(addr != NULL, -1);

	addrlen = sizeof(so);
	if (getsockname(g_io_channel_unix_get_fd(handle),
			(struct sockaddr *) &so, &addrlen) == -1)
		return -1;

        sin_get_ip(&so, addr);
	if (port) *port = sin_get_port(&so);

	return 0;
}

/* Get IP address for host, returns 0 = ok,
   others = error code for net_gethosterror() */
int net_gethostbyname(const char *addr, IPADDR *ip)
{
#ifdef HAVE_IPV6
	union sockaddr_union *so;
	struct addrinfo req, *ai;
	char hbuf[NI_MAXHOST];
	int host_error;
#else
	struct hostent *hp;
#endif

	g_return_val_if_fail(addr != NULL, -1);

#ifdef HAVE_IPV6
	memset(ip, 0, sizeof(IPADDR));
	memset(&req, 0, sizeof(struct addrinfo));
	req.ai_socktype = SOCK_STREAM;

	/* save error to host_error for later use */
	host_error = getaddrinfo(addr, NULL, &req, &ai);
	if (host_error != 0)
		return host_error;

	if (getnameinfo(ai->ai_addr, ai->ai_addrlen, hbuf,
			sizeof(hbuf), NULL, 0, NI_NUMERICHOST))
		return 1;

	so = (union sockaddr_union *) ai->ai_addr;
        sin_get_ip(so, ip);
	freeaddrinfo(ai);
#else
	hp = gethostbyname(addr);
	if (hp == NULL) return h_errno;

	ip->family = AF_INET;
	memcpy(&ip->addr, hp->h_addr, 4);
#endif

	return 0;
}

/* Get name for host, *name should be g_free()'d unless it's NULL.
   Return values are the same as with net_gethostbyname() */
int net_gethostbyaddr(IPADDR *ip, char **name)
{
#ifdef HAVE_IPV6
	struct addrinfo req, *ai;
	int host_error;
#else
	struct hostent *hp;
#endif
	char ipname[MAX_IP_LEN];

	g_return_val_if_fail(ip != NULL, -1);
	g_return_val_if_fail(name != NULL, -1);

	net_ip2host(ip, ipname);

	*name = NULL;
#ifdef HAVE_IPV6
	memset(&req, 0, sizeof(struct addrinfo));
	req.ai_socktype = SOCK_STREAM;
	req.ai_flags = AI_CANONNAME;

	/* save error to host_error for later use */
	host_error = getaddrinfo(ipname, NULL, &req, &ai);
	if (host_error != 0)
		return host_error;
	*name = g_strdup(ai->ai_canonname);

	freeaddrinfo(ai);
#else
	hp = gethostbyaddr(ipname, strlen(ipname), AF_INET);
	if (hp == NULL) return -1;

	*name = g_strdup(hp->h_name);
#endif

	return 0;
}

int net_ip2host(IPADDR *ip, char *host)
{
#ifdef HAVE_IPV6
	if (!inet_ntop(ip->family, &ip->addr, host, MAX_IP_LEN))
		return -1;
#else
	unsigned long ip4;

	ip4 = ntohl(ip->addr.ip.s_addr);
	g_snprintf(host, MAX_IP_LEN, "%lu.%lu.%lu.%lu",
		   (ip4 & 0xff000000UL) >> 24,
		   (ip4 & 0x00ff0000) >> 16,
		   (ip4 & 0x0000ff00) >> 8,
		   (ip4 & 0x000000ff));
#endif
	return 0;
}

int net_host2ip(const char *host, IPADDR *ip)
{
	unsigned long addr;

#ifdef HAVE_IPV6
	if (strchr(host, ':') != NULL) {
		/* IPv6 */
		ip->family = AF_INET6;
		if (inet_pton(AF_INET6, host, &ip->addr) == 0)
			return -1;
	} else
#endif
	{
		/* IPv4 */
		ip->family = AF_INET;
#ifdef HAVE_INET_ATON
		if (inet_aton(host, &ip->addr.ip.s_addr) == 0)
			return -1;
#else
		addr = inet_addr(host);
		if (addr == INADDR_NONE)
			return -1;

		memcpy(&ip->addr, &addr, 4);
#endif
	}

	return 0;
}

/* Get socket error */
int net_geterror(GIOChannel *handle)
{
	int data;
	socklen_t len = sizeof(data);

	if (getsockopt(g_io_channel_unix_get_fd(handle),
		       SOL_SOCKET, SO_ERROR, (void *) &data, &len) == -1)
		return -1;

	return data;
}

/* get error of net_gethostname() */
const char *net_gethosterror(int error)
{
#ifdef HAVE_IPV6
	g_return_val_if_fail(error != 0, NULL);

	if (error == 1) {
		/* getnameinfo() failed ..
		   FIXME: does strerror return the right error message? */
		return g_strerror(errno);
	}

	return gai_strerror(error);
#else
	switch (error) {
	case HOST_NOT_FOUND:
		return _("Host not found");
	case NO_ADDRESS:
		return _("No IP address found for name");
	case NO_RECOVERY:
		return _("A non-recovable name server error occurred");
	case TRY_AGAIN:
		return _("A temporary error on an authoritative name server");
	}

	/* unknown error */
	return NULL;
#endif
}

/* return TRUE if host lookup failed because it didn't exist (ie. not
   some error with name server) */
int net_hosterror_notfound(int error)
{
#ifdef HAVE_IPV6
	return error != 1 && (error == EAI_NONAME || error == EAI_NODATA);
#else
	return error == HOST_NOT_FOUND || error == NO_ADDRESS;
#endif
}

/* Get name of TCP service */
char *net_getservbyport(int port)
{
	struct servent *entry;

	entry = getservbyport(htons((unsigned short) port), "tcp");
	return entry == NULL ? NULL : entry->s_name;
}

int is_ipv4_address(const char *host)
{
	while (*host != '\0') {
		if (*host != '.' && !isdigit(*host))
			return 0;
                host++;
	}

	return 1;
}

int is_ipv6_address(const char *host)
{
	while (*host != '\0') {
		if (*host != ':' && !isxdigit(*host))
			return 0;
                host++;
	}

	return 1;
}
