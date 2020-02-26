#ifndef TUNTAP_H
#define TUNTAP_H

#include <stdint.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <linux/if.h>
#include <linux/if_tun.h>

int tun_alloc( char * dev, int flags, const char * ip4_addr_str, uint8_t * hw_addr, int mtu );
int set_ip( struct ifreq * ifr, int sock, uint32_t ip4 );
int set_mac( const char * if_name, int sock, uint8_t * hw_addr );
int set_mtu( const char * if_name, int sock, int mtu_val );
void hexdump( char * in, size_t len );
int set_if_up( char * ifname, short flags );
int set_if_down( char * ifname, short flags);
int set_if_flags( char * ifname, short flags);

#endif /* TUNTAP_H */
