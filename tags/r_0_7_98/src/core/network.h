#ifndef __NETWORK_H
#define __NETWORK_H

#include <sys/types.h>
#ifndef WIN32
#  include <sys/socket.h>
#  include <netinet/in.h>
#endif

#ifndef AF_INET6
#  ifdef PF_INET6
#    define AF_INET6 PF_INET6
#  else
#    define AF_INET6 10
#  endif
#endif

struct _IPADDR {
	unsigned short family;
	union {
#ifdef HAVE_IPV6
		struct in6_addr ip6;
#else
		struct in_addr ip;
#endif
	} addr;
};

/* maxmimum string length of IP address */
#ifdef HAVE_IPV6
#  define MAX_IP_LEN INET6_ADDRSTRLEN
#else
#  define MAX_IP_LEN 20
#endif

#define is_ipv6_addr(ip) ((ip)->family != AF_INET)

/* returns 1 if IPADDRs are the same */
int net_ip_compare(IPADDR *ip1, IPADDR *ip2);

/* Connect to socket */
GIOChannel *net_connect(const char *addr, int port, IPADDR *my_ip);
/* Connect to socket with ip address */
GIOChannel *net_connect_ip(IPADDR *ip, int port, IPADDR *my_ip);
/* Disconnect socket */
void net_disconnect(GIOChannel *handle);
/* Try to let the other side close the connection, if it still isn't
   disconnected after certain amount of time, close it ourself */
void net_disconnect_later(GIOChannel *handle);

/* Listen for connections on a socket */
GIOChannel *net_listen(IPADDR *my_ip, int *port);
/* Accept a connection on a socket */
GIOChannel *net_accept(GIOChannel *handle, IPADDR *addr, int *port);

/* Read data from socket, return number of bytes read, -1 = error */
int net_receive(GIOChannel *handle, char *buf, int len);
/* Transmit data, return number of bytes sent, -1 = error */
int net_transmit(GIOChannel *handle, const char *data, int len);

/* Get IP address for host. family specifies if we should prefer to
   IPv4 or IPv6 address (0 = default, AF_INET or AF_INET6).
   returns 0 = ok, others = error code for net_gethosterror() */
int net_gethostbyname(const char *addr, IPADDR *ip, int family);
/* Set the default address family to use with host resolving
   (AF_INET or AF_INET6) */
void net_host_resolver_set_default_family(unsigned short family);
/* Get name for host, *name should be g_free()'d unless it's NULL.
   Return values are the same as with net_gethostbyname() */
int net_gethostbyaddr(IPADDR *ip, char **name);
/* get error of net_gethostname() */
const char *net_gethosterror(int error);
/* return TRUE if host lookup failed because it didn't exist (ie. not
   some error with name server) */
int net_hosterror_notfound(int error);

/* Get socket address/port */
int net_getsockname(GIOChannel *handle, IPADDR *addr, int *port);

/* IPADDR -> char* translation. `host' must be at least MAX_IP_LEN bytes */
int net_ip2host(IPADDR *ip, char *host);
/* char* -> IPADDR translation. */
int net_host2ip(const char *host, IPADDR *ip);

/* Get socket error */
int net_geterror(GIOChannel *handle);

/* Get name of TCP service */
char *net_getservbyport(int port);

int is_ipv4_address(const char *host);
int is_ipv6_address(const char *host);

#endif
