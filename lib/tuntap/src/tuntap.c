#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>
#include <getopt.h>
#include <unistd.h>
#include <fcntl.h>
#include <string.h>
#include <signal.h>
#include <sys/ioctl.h>
#include <sys/socket.h>
#include <linux/if.h>
#include <linux/if_tun.h>
#include <arpa/inet.h>
#include <net/if_arp.h>

#include "tuntap.h"

/*
 *
 *  Source code for a library to generate TUN/TAP interfaces on a GNU/Linux
 *  system.
 *
 *  Author: Alex Hansen
 *  email:  ajhansen@mail.usf.edu
 *
 */

// MAC address is 6 8-bit numbers
#define HW_ETHERNET_ADDR_LEN 6

#define BACKLOG_DEFAULT 100

void hexdump( char * in, size_t len )
{
  int colwidth = 16;
  for(int i=0; i < len / colwidth; ++i)
  {
    for(int j=0; j < colwidth; ++j)
    {
      if( i*colwidth + j > len )
        break;
      printf("%02x ", in[i*colwidth+j]);
    }
    printf("\n");
  }
  printf("\n");
}

/*
 *  char * dev should be something like
 *  "tun%d" or "tap%d"
 */
int tun_alloc( char * dev, int flags, const char * ip4_addr_str, uint8_t * hw_addr, int mtu )
{
  struct ifreq ifr;
  int fd, sock, err;
  uint32_t ip4_netaddr;
  char * clonedev = "/dev/net/tun";

  /*
   * Valid unicast ethernet address must start with an
   * even valued byte.
   */
  //uint8_t hw_addr[] = {0x10, 0x20, 0x30, 0x40, 0x50, 0x60};

  /*
   * Convert from presentation form of IP address (string)
   * to network order (unsigned 32-bit integer)
   */
  inet_pton( AF_INET, ip4_addr_str, &ip4_netaddr );
  // printf("%08x\n", ip4_netaddr);

  /*
   *  Open /dev/net/tun to clone it
   */
  fd = open(clonedev, O_RDWR);

  if( fd < 0 )
  {
    return fd;
  }

  bzero( &ifr, sizeof(ifr) );

  /*
   * Flags: IFF_TUN   - TUN device (no Ethernet headers)
   *        IFF_TAP   - TAP device
   *
   *        IFF_NO_PI - Do not provide packet information
   */

  ifr.ifr_flags = flags;

  if( *dev )
  {
    strncpy( ifr.ifr_name, dev, IFNAMSIZ );
  }

  err = ioctl(fd, TUNSETIFF, (void *) &ifr);

  if( err < 0 )
  {
    close(fd);
    return err;
  }


  strcpy( dev, ifr.ifr_name );

  sock = socket( AF_INET, SOCK_DGRAM, 0 );

  if( sock < 0 )
  {
    fprintf(stderr, "%s: failed to open socket\n", __func__);
    close(fd);
    return 1;
  }

  if( set_ip( &ifr, sock, ip4_netaddr ) )
  {
    close(fd);
    close(sock);
    return 1;
  }

  if( hw_addr != NULL )
  {
    err = set_mac( dev, sock, hw_addr );
  }

  if( mtu != 0 )
  {
    set_mtu( dev, sock, mtu );
  }

  if( err )
  {
    close(fd);
    close(sock);
    return 1;
  }

  return fd;
}

int set_ip(struct ifreq * ifr, int sock, uint32_t ip4)
{
  struct sockaddr_in addr;

  memset( &addr, 0 , sizeof(addr) );

  addr.sin_addr.s_addr = ip4;

  addr.sin_family = AF_INET;

  memcpy( &ifr->ifr_addr, &addr, sizeof(struct sockaddr) );

  if( ioctl( sock, SIOCSIFADDR, ifr ) < 0 )
  {
    fprintf(stderr, "%s: SIOCSIFADDR error\n", __func__);
    close(sock);
    return -1;
  }

  return 0;
}

int set_mac(const char * if_name, int sock, uint8_t * hw_addr)
{
  struct ifreq ifr;
  int status;

  if( hw_addr == NULL )
  {
    fprintf(stderr, "%s: hw_addr is null pointer\n", __func__);
    return -1;
  }

  if( if_name == NULL )
  {
    fprintf(stderr, "%s: if_name is null pointer\n", __func__);
    return -1;
  }

  memset( &ifr, 0, sizeof(struct ifreq) );

  strncpy(ifr.ifr_name, if_name, IFNAMSIZ);

  for( int i=0; i < HW_ETHERNET_ADDR_LEN; ++i )
  {
    ifr.ifr_hwaddr.sa_data[i] = hw_addr[i];
  }

  ifr.ifr_hwaddr.sa_family = ARPHRD_ETHER;

  status = ioctl(sock, SIOCSIFHWADDR, &ifr);

  if( status )
  {
    perror("SIOCSIFHWADDR");
    return -1;
  }

  return 0;
}

int set_mtu( const char * if_name, int fd, int mtu_val )
{
  int status;
  struct ifreq ifr;

  ifr.ifr_addr.sa_family = AF_INET;

  strncpy( ifr.ifr_name, if_name, sizeof( ifr.ifr_name) );

  ifr.ifr_mtu = mtu_val;

  status = ioctl(fd, SIOCSIFMTU, (caddr_t)&ifr);

  if( status )
  {
    perror("ioctl");
  }

  return 0;
}

int set_if_up( char * ifname, short flags )
{
  return set_if_flags(ifname, flags | IFF_UP);
}

int set_if_down( char * ifname, short flags)
{
  return set_if_flags(ifname, flags & ~IFF_UP);
}

int set_if_flags( char * ifname, short flags )
{
  int sockfd;
  struct ifreq ifr;

  sockfd = socket(AF_INET, SOCK_DGRAM, 0);

  if( sockfd < 0 )
  {
    perror("socket");
    return -1;
  }

  memset(&ifr, 0, sizeof ifr);

  strncpy(ifr.ifr_name, ifname, IFNAMSIZ);

  ifr.ifr_flags |= flags;

  if( ioctl(sockfd, SIOCSIFFLAGS, &ifr) )
  {
    perror("ioctl");
    return -1;
  }

  return 0;
}
