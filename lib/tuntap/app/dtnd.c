#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>
#include <stdarg.h>
#include <getopt.h>
#include <string.h>
#include <fcntl.h>
#include <unistd.h>
#include <pthread.h>
#include <sys/types.h>
#include <sys/wait.h>
#include <arpa/inet.h>
#include <sys/socket.h>
#include <netinet/tcp.h>
#include <netinet/in.h>

#include "server_client.h"
#include "tuntap.h"

static int dtnd_do_exit = 0;
static int threads_exit = 0;
static FILE * err_file = NULL;

static char * ownEid = NULL;
static char * destEid = NULL;

#define typename(x) _Generic((x),                                                 \
        _Bool: "_Bool",                  unsigned char: "unsigned char",          \
         char: "char",                     signed char: "signed char",            \
    short int: "short int",         unsigned short int: "unsigned short int",     \
          int: "int",                     unsigned int: "unsigned int",           \
     long int: "long int",           unsigned long int: "unsigned long int",      \
long long int: "long long int", unsigned long long int: "unsigned long long int", \
        float: "float",                         double: "double",                 \
  long double: "long double",                   char *: "pointer to char",        \
       void *: "pointer to void",                int *: "pointer to int",         \
       char **: "char **",                     char **: "const char **",          \
      default: "other")

int child_routine(int * stdin_pipe, int * stdout_pipe, int * stderr_pipe, const char * path, char * const arg[]);
int parent_routine(int * stdin_pipe, int * stdout_pipe, int * stderr_pipe, pid_t child_pid, int sockfd);
void * io_routine( void * args );

void usage(char ** argv)
{
  printf("usage: %s -i <interface-name> -s <source-ipn> -d <destination-ipn>\n", argv[0]);
  printf("\n");
  printf("-h  [--help]    - Prints this help info\n");
  printf("-i  [--ifname]  - Interface %s will attach to\n", argv[0]);
  printf("-p  [--port]    - Port %s will listen on\n", argv[0]);
  printf("-s  [--source]  - Source IPN, eg. ipn:150.1\n");
  printf("-d  [--dest]    - Destination IPN, eg. ipn:149.1\n");
  printf("\n");
}

void print_error( const char * fmt, ... )
{
  va_list ap;
  va_start(ap, fmt);

  if(!err_file)
    vfprintf( err_file, fmt, ap );
  else
    vfprintf( stderr, fmt, ap );
}

void print_debug( const char * fmt, ... )
{
  va_list ap;
  va_start(ap, fmt);

  if(debug_file)
    vfprintf( debug_file, fmt, ap );

}

int do_fork_exec(int sockfd)
{
  pid_t child_pid = -1;

  int stdin_pipe[2];
  int stdout_pipe[2];
  int stderr_pipe[2];

  if( pipe(stdin_pipe) || pipe(stdout_pipe) || pipe(stderr_pipe) )
  {
    print_error("pipe(): %s\n", strerror(ERRNO));
    return -1;
  }

  if( (child_pid = fork()) )
  {
    // In parent, do parent stuff
    parent_routine( stdin_pipe, stdout_pipe, stderr_pipe, child_pid, sockfd );
  }
  else if( child_pid == 0 )
  {
    // In child, do child stuff
    const char * path = "/usr/bin/bpchat2";
    char * args[] = {"bpchat2", "-i", ownEid, "-o", destEid, "--raw", NULL};
    child_routine( stdin_pipe, stdout_pipe, stderr_pipe, path, args );
  }
  else
  {
    printf_error("fork(): %s\n", strerror(ERRNO));
    return 1;
  }

  return 0;
}

int child_routine( int * stdin_pipe, int * stdout_pipe, int * stderr_pipe, const char * path, char * const argv[] )
{
  if( stdin_pipe != NULL )
  {
    dup2( stdin_pipe[0], STDIN_FILENO );
    close( stdin_pipe[0] );
    close( stdin_pipe[1] );
  }

  if( stdout_pipe != NULL )
  {
    dup2( stdout_pipe[1], STDOUT_FILENO );
    close( stdout_pipe[0] );
    close( stdout_pipe[1] );
  }

  if( stderr_pipe != NULL )
  {
    dup2( stderr_pipe[1], STDERR_FILENO );
    close( stderr_pipe[0] );
    close( stderr_pipe[1] );
  }

  if( path != NULL )
  {
    execv( path, argv );
  }

  // Child has failed if it reaches here
  print_error("child_routine(): failed to exec\n");

  exit(EXIT_FAILURE);
}

int parent_routine( int * stdin_pipe, int * stdout_pipe, int * stderr_pipe, int child_pid, int sockfd )
{
  pid_t wait_return = 0;
  int child_status;
  int threads_joined = 0;
  pthread_t io_threads[2];

  int child_stdin = -1;
  int child_stdout = -1;
  int child_stderr = -1;

  int io_args1[2];
  int io_args2[2];

  char stderr_buffer[1024];
  ssize_t readlen;

  if( stdin_pipe != NULL )
  {
    close(stdin_pipe[0]);
    child_stdin = stdin_pipe[1];
  }

  if( stdout_pipe != NULL )
  {
    close(stdout_pipe[1]);
    child_stdout = stdout_pipe[0];
  }

  if( stderr_pipe != NULL )
  {
    close(stderr_pipe[1]);
    child_stderr = stderr_pipe[0];
  }

  io_args1[0] = sockfd;
  io_args1[1] = child_stdin;

  pthread_create( &io_threads[0], NULL, &io_routine, (void *)io_args1 );

  io_args2[0] = child_stdout;
  io_args2[1] = sockfd;

  pthread_create( &io_threads[1], NULL, &io_routine, (void *)io_args2 );

  while( wait_return != child_pid )
  {
    wait_return = waitpid( child_pid, &child_status, WNOHANG );

    if( !threads_joined && pthread_tryjoin_np( io_threads[0], NULL ) == 0 )
    {
      print_debug("thread 0 returned, cancelling thread 1\n");
      pthread_cancel( io_threads[1] );
      pthread_join( io_threads[1], NULL );
      print_debug("threads 0 & 1 joined\n");
      threads_joined = 1;
    }

    if( !threads_joined && pthread_tryjoin_np( io_threads[1], NULL ) == 0 )
    {
      print_debug("thread 1 returned, cancelling thread 0\n");
      pthread_cancel( io_threads[0] );
      pthread_join( io_threads[0], NULL );
      print_debug("threads 1 & 0 joined\n");
      threads_joined = 1;
    }

    if( threads_joined == 1 )
    {
      kill( child_pid, SIGKILL );
      break;
    }

    // readlen = read( child_stderr, stderr_buffer, sizeof(stderr_buffer)-1 );
    // if( readlen > 0 )
    // {
    //   stderr_buffer[readlen] = '\0';
    //   printf("child_stderr: %s\n", stderr_buffer);
    // }
  }

  print_debug("child returned with status %d\n", WEXITSTATUS(child_status));

  if( threads_joined == 0 )
  {
    pthread_cancel( io_threads[0] );
    pthread_cancel( io_threads[1] );

    pthread_join( io_threads[0], NULL );
    pthread_join( io_threads[1], NULL );
  }

  close( stdin_pipe[1] );
  close( stdout_pipe[0] );
  close( stderr_pipe[0] );
  shutdown( sockfd, 2 );

  print_debug("closed all open file descriptors, parent returning\n");

  return 0;
}

int do_server(const char * if_name, uint16_t port_no, int backlog)
{
  int sockfd, session_fd;
  int optval;
  socklen_t optlen = sizeof(optval);

  print_debug("attempting open %s\n", if_name);

  sockfd = open_server_socket_by_name( if_name, port_no, backlog);

  if( sockfd < 0 )
  {
    print_debug("open_server_socket_by_name(): failed to open socket on %s\n", if_name);
    return -1;
  }

  optval = 1;

  if( setsockopt(sockfd, SOL_SOCKET, SO_KEEPALIVE, &optval, optlen) < 0 )
  {
    print_error("setsockopt(): %s\n", strerror(ERRNO));
    close(sockfd);
    return -1;
  }

  optval = 10;

  if( setsockopt(sockfd, IPPROTO_IP, TCP_KEEPCNT, &optval, optlen) < 0 )
  {
    print_error("setsockopt(): %s\n", strerror(ERRNO));
    close(sockfd);
    return -1;
  }

  optval = 10;

  if( setsockopt(sockfd, IPPROTO_IP, TCP_KEEPIDLE, &optval, optlen) < 0 )
  {
    print_error("setsockopt(): %s\n", strerror(ERRNO));
    close(sockfd);
    return -1;
  }

  optval = 10;

  if( setsockopt(sockfd, IPPROTO_IP, TCP_KEEPINTVL, &optval, optlen) < 0 )
  {
    print_error("setsockopt(): %s\n", strerror(ERRNO));
    close(sockfd);
    return -1;
  }

  print_debug("waiting for clients\n");

  while( dtnd_do_exit == 0 )
  {
    session_fd = accept_server_conn( sockfd );

    if( session_fd >= 0 )
      break;
  }

  print_debug("started client session\n");

  return session_fd;
}

void * io_routine( void * args )
{
  int * args_ptr = (int *)args;
  ssize_t readlen;
  char buffer[4096];

  while( 1 )
  {
    readlen = read( args_ptr[0], buffer, sizeof(buffer)-1 );
    if( readlen > 0 )
    {
      buffer[readlen] = '\0';
      // printf("read %d bytes from %d\n", readlen, args_ptr[0]);
      // printf("buffer[] = %s", buffer);
      readlen = write( args_ptr[1], buffer, readlen+1 );
      if( readlen < 0 )
      {
        print_error("write on closed fd\n");
        return NULL;
      }
      //printf("wrote %d bytes to %d\n", readlen+1, args_ptr[1]);
    }
    else if( readlen < 0 )
    {
      print_error("read on closed fd\n");
      return NULL;
    }
    else
    {
      print_debug("end of file\n");
      return NULL;
    }
  }

  return NULL;
}

int main(int argc, char ** argv)
{
  if( argc == 1 )
  {
    usage(argv);
    return 1;
  }

  int sockfd, session_fd;

  struct sockaddr_in client_addr;

  socklen_t client_len;

  int option_index, opt;

  char ifname[1024];

  uint16_t port = 1337;

  static struct option long_options[] =
  {
    {"ifname", required_argument, 0, 'i'},
    {"port", required_argument, 0, 'p'},
    {"source", required_argument, 0, 's'},
    {"dest", required_argument, 0, 'd'},
    {"verbose", no_argument, 0, 'v'},
    {"help", no_argument, 0, 'h'},
    {0,0,0,0}
  };

  while( (opt = getopt_long(argc, argv, "hi:p:s:d:v", long_options, &option_index)) != -1 )
  {
    switch( opt )
    {
      case 'h':
        usage(argv);
        return 1;

      case 's':
        ownEid = optarg;
        break;

      case 'd':
        destEid = optarg;
        break;

      case 'i':
        strncpy(ifname, optarg, sizeof(ifname));
        break;

      case 'p':
        port = atoi(optarg);
        break;

      case 'v':
        debug_file = stderr;

      case '?':
        break;

      default:
        printf("????? lol");
    }
  }

  sockfd = open_server_socket_by_name( ifname, port, 2000 );

  if( sockfd < 0 )
  {
    print_error("sockfd is invalid [%d]\n", sockfd);
    return 1;
  }

  while( dtnd_do_exit == 0 )
  {
    pid_t pid;

    print_debug("waiting for connection\n");

    session_fd = accept( sockfd, (struct sockaddr *)&client_addr, &client_len );

    if( session_fd < 0 )
    {
      print_error("session_fd is %d\n", session_fd);
      exit(EXIT_FAILURE);
    }

    print_debug("accepted connection from %s\n", inet_ntoa( client_addr.sin_addr ) );

    if( (pid = fork()) )
    {
      continue;
    }
    else if (pid == 0)
    {
      do_fork_exec( session_fd );
      break;
    }
    else
    {
      print_error("fork(): %s\n", strerror(ERRNO));
      exit(EXIT_FAILURE);
    }
  }

  return 0;
}
