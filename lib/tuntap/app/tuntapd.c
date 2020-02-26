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

#include "tuntap.h"
#include "netutil.h"
#include "printutil.h"

#define BACKLOG_DEFAULT 100
#define MTU_DEFAULT 576

int io_main_routine(int,int);
void * io_thread_routine( void * args );

static int do_exit = 0;

void usage(char ** argv)
{
  printf("usage: %s -i <ip-addr> -p <port> -t <tun/tap-ip> -m <mode>\n", argv[0]);
  printf("\n");
  printf("-i [--ipaddr]\tIP ddress of the network interface that %s attaches to\n", argv[0]);
  printf("-p [--port]\tPort to use on the network interface\n");
  printf("-t [--tap_ip]\nDesired IP address of tap interface that is created\n");
  printf("-m [--mode]\nOptions are \"server\" or \"client\". Server will accept incoming connections\
  on the interface specified by the -i flag, while client will attempt to connect to a server listening\
  on the interface specified by the -i flag.\n");
  printf("\n");

  exit(1);
}

static void sigint_handler(int sig, siginfo_t * siginfo, void * context)
{
  fprintf(stderr, "Caught Ctrl-C, exiting\n");
  do_exit = 1;
}

int main(int argc, char ** argv)
{
  struct sigaction act;

  bzero( &act, sizeof(act) );
  act.sa_sigaction = sigint_handler;
  sigemptyset( &act.sa_mask );
  act.sa_flags = SA_SIGINFO;
  sigaction( SIGINT, &act, NULL );

  int status = 0;

  int opt, option_index;

  tunserv_mode_t mode=0;

  uint16_t port = 0;

  char ipaddr[1024];

  char tap_ip_addr[1024];

  uint8_t hw_addr[] = {0x10, 0x20, 0x30, 0x40, 0x50, 0x60};

  int mtu_val = MTU_DEFAULT;

  static struct option long_options[] =
  {
    {"mode", required_argument, 0, 'm'},
    {"ipaddr", required_argument, 0, 'i'},
    {"port", required_argument, 0, 'p'},
    {"tap_ip", required_argument, 0, 't'},
    {"hw_addr", required_argument, 0, 'h'},
    {"mtu", required_argument, 0, 'u'},
    {0,0,0,0}
  };

  while( (opt = getopt_long(argc, argv, "i:p:m:t:h:u:", long_options, &option_index)) != -1 )
  {
    switch( opt )
    {
      case 'm':
        if( strncmp(optarg, "server", strlen("server")) == 0 )
          mode = SERVER_MODE;
        if( strncmp(optarg, "client", strlen("client")) == 0 )
          mode = CLIENT_MODE;
        printf("mode: %d\n", mode);
        break;

      case 'i':
        strncpy(ipaddr, optarg, sizeof(ipaddr));
        printf("ipaddr: %s\n", ipaddr);
        break;

      case 'p':
        port = atoi(optarg);
        printf("port: %d\n", port);
        break;

      case 't':
        strncpy( tap_ip_addr, optarg, sizeof(tap_ip_addr) );
        printf("tap ip: %s\n", tap_ip_addr);
        break;

      case 'h':
        printf("%s\n", optarg);

        sscanf
        (
          optarg,
          "%02hhu:%02hhu:%02hhu:%02hhu:%02hhu:%02hhu",
          &hw_addr[0],
          &hw_addr[1],
          &hw_addr[2],
          &hw_addr[3],
          &hw_addr[4],
          &hw_addr[5]
        );
        printf
        (
          "tap hw addr: %02hhu:%02hhu:%02hhu:%02hhu:%02hhu:%02hhu\n",
          hw_addr[0],
          hw_addr[1],
          hw_addr[2],
          hw_addr[3],
          hw_addr[4],
          hw_addr[5]
        );
        break;

      case 'u':
        mtu_val = atoi( optarg );
        printf("mtu=%d\n", mtu_val);
        break;

      case '?':
        break;

      default:
        printf("????? lol");

    }

  }

// Open tap
  char tap_name[1024] = "tap%d";
  int tap_fd = tun_alloc( tap_name, IFF_TAP | IFF_NO_PI , tap_ip_addr, hw_addr, mtu_val );
  int flags = fcntl(tap_fd, F_GETFL, 0);
  flags |= O_NONBLOCK;
  fcntl(tap_fd, F_SETFL, flags);
//

  int sockfd = 0, clientfd = 0;

  switch( mode )
  {
    case SERVER_MODE:
      sockfd = open_server_socket( ipaddr, port, 2000);

      printf("server sockfd opened [%d]\n", sockfd);

      clientfd = accept_server_conn( sockfd );

      if( clientfd <= 0 )
      {
        fprintf(stderr, "clientfd invalid\n");
        status = EXIT_FAILURE;
        goto cleanup;
      }

      //print_debug("accepted client %s\n", );

      // flags = fcntl( clientfd, F_GETFL, 0 );
      // flags |= O_NONBLOCK;
      // fcntl( clientfd, F_SETFL, flags);

      io_main_routine( clientfd, tap_fd );

      break;

    case CLIENT_MODE:

      sockfd = open_client_socket( ipaddr, port );

      if(sockfd < 0)
      {
        fprintf(stderr, "failed to open client socket\n");
        status = EXIT_FAILURE;
        goto cleanup;
      }
      // flags = fcntl(sockfd, F_GETFL, 0);
      // flags |= O_NONBLOCK;
      // fcntl(sockfd, F_SETFL, flags);

      io_main_routine( tap_fd, sockfd );

      break;

    default:
      printf("done broke\n");
      break;
  }

cleanup:
  if( sockfd > 0 )
    close(sockfd);
  if( clientfd > 0 )
    close(clientfd);

  return status;
}

int io_main_routine( int in_fd, int out_fd )
{
  pthread_t io_threads[2];
  int args0[2];
  int args1[2];
  int status;

  args0[0] = in_fd;
  args0[1] = out_fd;

  fprintf(stderr, "Creating thread 0\n");
  status = pthread_create( &io_threads[0], NULL, io_thread_routine, (void *)args0 );

  if( status )
  {
    fprintf(stderr, "failed to create thread 0\n");
  }

  args1[0] = out_fd;
  args1[1] = in_fd;

  fprintf(stderr, "Creating thread 1\n");
  status = pthread_create( &io_threads[1], NULL, io_thread_routine, (void *)args1 );

  if( status )
  {
    fprintf(stderr, "failed to create thread 1\n");
  }

  status = pthread_join( io_threads[0], NULL );

  if( status )
  {
    fprintf(stderr, "error joining thread 0\n");
  }
  else
  {
    fprintf(stderr, "joined thread 0\n");
  }

  status = pthread_join( io_threads[1], NULL );

  if( status )
  {
    fprintf(stderr, "error joining thread 1\n");
  }
  else
  {
    fprintf(stderr, "joined thread 1\n");
  }

  return 0;
}

void * io_thread_routine( void * args )
{
  int readlen, writelen;
  char buffer[4096];

  int * args_ptr = (int *)args;
  int in_fd, out_fd;

  in_fd = args_ptr[0];
  out_fd = args_ptr[1];

  while( do_exit == 0 )
  {
    readlen = read(in_fd, buffer, sizeof(buffer));

    if( readlen > 0 )
    {
      printf("read %d bytes from tap\n", readlen);

      hexdump( (char *)buffer, readlen);

      writelen = write(out_fd, buffer, readlen);

      if( writelen != readlen )
      {
        fprintf(stderr, "writelen != readlen [%d]\n", __LINE__);
      }
      printf("wrote %d bytes to if\n", writelen);
    }
  }

  return NULL;
}
