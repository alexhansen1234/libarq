#include "gateway.h"

int set_gateway( const char * device, const char * gateway )
{
  int err = 0;
  int sockfd = -1;
  struct rtentry route;
  struct sockaddr_in gateway_ip;

  sockfd = socket(AF_INET, SOCK_DGRAM, IPPROTO_IP);

  if( sockfd == -1 )
  {
    perror("socket");
    return -1;
  }

  memset( &route, 0, sizeof(route) );

  gateway_ip.sin_family = AF_INET;
  gateway_ip.sin_addr.s_addr = inet_addr(gateway);
  gateway_ip.sin_port = 0;

  ( (struct sockaddr_in *)&route.rt_dst )->sin_family = AF_INET;
  ( (struct sockaddr_in *)&route.rt_dst )->sin_addr.s_addr = 0;
  ( (struct sockaddr_in *)&route.rt_dst )->sin_port = 0;

  ( (struct sockaddr_in *)&route.rt_genmask )->sin_family = AF_INET;
  ( (struct sockaddr_in *)&route.rt_genmask )->sin_addr.s_addr = 0;
  ( (struct sockaddr_in *)&route.rt_genmask )->sin_port = 0;

  memcpy( (void *)&route.rt_gateway, &gateway_ip, sizeof(gateway_ip) );
  route.rt_flags = RTF_UP | RTF_GATEWAY | RTF_HOST;
  route.rt_metric = 0;

  if( (err = ioctl(sockfd, SIOCADDRT, &route)) != 0 )
  {
    perror("ioctl");
    return err;
  }

  return 0;
}
