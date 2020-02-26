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
#include <getopt.h>
#include <pthread.h>

#include "netutil.h"

int open_client_socket( const char * dest_ip_addr, uint16_t dest_port )
{
  int sockfd=0, status;
  struct sockaddr_in server_addr;
  uint32_t dest_addr;

  if( dest_ip_addr == NULL )
  {
    fprintf(stderr, "dest_ip_addr is null\n");
    goto err_cleanup;
  }
  else
  {
    status = inet_pton( AF_INET, dest_ip_addr, &dest_addr);
  }

  if( status != 1 )
  {
    perror("inet_pton");
    goto err_cleanup;
  }

  sockfd = socket( AF_INET, SOCK_STREAM, 0 );

  if( sockfd <= 0 )
  {
    perror("socket");
    goto err_cleanup;
  }

  server_addr.sin_family = AF_INET;
  server_addr.sin_addr.s_addr = dest_addr;
  server_addr.sin_port = htons( dest_port );

  status = connect( sockfd, (struct sockaddr *)&server_addr, sizeof(server_addr) );

  if( status )
  {
    perror("connect");
    goto err_cleanup;
  }

  return sockfd;

err_cleanup:
  if( sockfd > 0 )
    close(sockfd);
  return -1;
}

// Puts server in listen state
int open_server_socket( const char * ip_addr, uint16_t port, int backlog)
{
  int sockfd=0, status=0, opt;
  struct sockaddr_in addr;
  uint32_t inaddr;

  if( ip_addr == NULL )
  {
    inaddr = htonl(INADDR_ANY);
  }
  else
  {
    status = inet_pton( AF_INET, ip_addr, &inaddr);
  }

  if( status != 1 )
  {
    perror("inet_pton");
    goto err_cleanup;
  }

  if( backlog <= 0 )
  {
    backlog = BACKLOG_DEFAULT;
  }

  sockfd = socket( AF_INET, SOCK_STREAM, 0 );

  if( sockfd <= 0 )
  {
    perror("socket");
    goto err_cleanup;
  }

  status = setsockopt( sockfd, SOL_SOCKET, SO_REUSEADDR | SO_REUSEPORT, &opt, sizeof(opt) );

  if( status )
  {
    perror("setsockopt");
    goto err_cleanup;
  }

  addr.sin_family = AF_INET;
  addr.sin_addr.s_addr = inaddr;
  addr.sin_port = htons( port );

  status = bind( sockfd, (struct sockaddr *)&addr, sizeof(addr) );

  if( status )
  {
    perror("bind");
    goto err_cleanup;
  }

  status = listen( sockfd, BACKLOG_DEFAULT );

  if( status )
  {
    perror("listen");
    goto err_cleanup;
  }

  return sockfd;

err_cleanup:
  if( sockfd > 0 )
    close(sockfd);
  return -1;
}

// Puts server in listen state
int open_server_socket_by_name( const char * if_name, uint16_t port, int backlog )
{
  int sockfd=0, status=0, opt;
  struct ifreq ifr;
  struct sockaddr_in addr;

  if( if_name == NULL )
  {
    goto err_cleanup;
  }

  if( backlog <= 0 )
  {
    backlog = BACKLOG_DEFAULT;
  }

  sockfd = socket( AF_INET, SOCK_STREAM, 0 );

  if( sockfd <= 0 )
  {
    perror("socket");
    goto err_cleanup;
  }

  ifr.ifr_addr.sa_family = AF_INET;
  strncpy(ifr.ifr_name, if_name, IFNAMSIZ-1);
  status = ioctl( sockfd, SIOCGIFADDR, &ifr );

  if( status )
  {
    perror("ioctl");
    goto err_cleanup;
  }

  status = setsockopt( sockfd, SOL_SOCKET, SO_REUSEADDR | SO_REUSEPORT, &opt, sizeof(opt) );

  if( status )
  {
    perror("setsockopt");
    goto err_cleanup;
  }

  addr.sin_family = AF_INET;
  addr.sin_addr.s_addr = ((struct sockaddr_in *)&ifr.ifr_addr)->sin_addr.s_addr;
  addr.sin_port = htons( port );

  status = bind( sockfd, (struct sockaddr *)&addr, sizeof(addr) );

  if( status )
  {
    perror("bind");
    goto err_cleanup;
  }

  status = listen( sockfd, BACKLOG_DEFAULT );

  if( status )
  {
    perror("listen");
    goto err_cleanup;
  }

  return sockfd;

err_cleanup:
  if( sockfd > 0 )
    close(sockfd);
  return -1;
}

// Accepts incoming server connection for bidirectional communication
int accept_server_conn( int server_fd )
{
  int session_fd = 0;

  session_fd = accept( server_fd, 0, 0 );

  if( session_fd < 0 )
  {
    perror("accept");
    goto err_cleanup;
  }

  return session_fd;

err_cleanup:
  if( session_fd > 0 )
    close(session_fd);

  return -1;
}
