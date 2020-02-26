#ifndef GATEWAY_H
  #define GATEWAY_H

  #include <stdio.h>
  #include <stdlib.h>
  #include <string.h>
  #include <sys/ioctl.h>
  #include <sys/types.h>
  #include <sys/socket.h>
  #include <net/route.h>
  #include <arpa/inet.h>

  int set_gateway(const char *, const char *);

#endif
