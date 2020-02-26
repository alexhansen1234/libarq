#ifndef NETUTIL_H
  #include <stdint.h>

  #define NETUTIL_H

  #define BACKLOG_DEFAULT 100

  typedef enum
  {
    SERVER_MODE=1,
    CLIENT_MODE
  } tunserv_mode_t;

  int open_server_socket( const char * ip_addr, uint16_t port, int backlog );
  int open_server_socket_by_name( const char * if_name, uint16_t port, int backlog );
  int open_client_socket( const char * dest_ip_addr, uint16_t dest_port );
  int accept_server_conn( int server_fd );

#endif /* NETUTIL_H */
