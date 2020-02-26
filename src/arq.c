#include <stdlib.h>
#include <assert.h>
#include <errno.h>
#include <unistd.h>
#include <fcntl.h>
#include <sys/types.h>
#include <sys/socket.h>
#include "arq.h"

Arq init_arq(
  int input_fd,
  int output_fd,
  size_t tx_window_len,
  size_t rx_window_len,
  void * (* tx_handler)(void *),
  void * (* tx_driver)(void *),
  void * (* rx_handler)(void *),
  void * (* rx_driver)(void *),
  void (* pkt_encoder)(void *, size_t, void *),
  void (* pkt_enc_destruct)(void *),
  void (* pkt_decoder)(void *, size_t, size_t, void *),
  void (* pkt_dec_destruct)(void *)
)
{
  int flags;
  Arq ret;

  assert(input_fd >= 0);
  assert(output_fd >= 0);
  assert(tx_window_len > 0);
  assert(rx_window_len > 0);
  assert(tx_handler != NULL);
  assert(tx_driver != NULL);
  assert(rx_handler != NULL);
  assert(rx_driver != NULL);
  assert(pkt_encoder != NULL);
  assert(pkt_decoder != NULL);
  assert(pkt_enc_destruct != NULL);
  assert(pkt_dec_destruct != NULL);

  ret.input_fd = input_fd;
  ret.output_fd = output_fd;
  ret.tx_window_len = tx_window_len;
  ret.rx_window_len = rx_window_len;
  ret.tx_handler = tx_handler;
  ret.tx_driver = tx_driver;
  ret.rx_handler = rx_handler;
  ret.rx_driver = rx_driver;
  ret.pkt_encoder = pkt_encoder;
  ret.pkt_decoder = pkt_decoder;
  ret.pkt_enc_destruct = pkt_enc_destruct;
  ret.pkt_dec_destruct = pkt_dec_destruct;

  ret.tx_pkt_buffer = (struct packet *)malloc(sizeof(struct packet) * tx_window_len);
  assert(ret.tx_pkt_buffer != NULL);

  for(int i=0; i < ret.tx_window_len; ++i)
  {
    ret.tx_pkt_buffer[i].data = NULL;
    ret.tx_pkt_buffer[i].pkt_len = 0;
    ret.tx_pkt_buffer[i].ref = 0;
  }

  ret.rx_pkt_buffer = (struct packet *)malloc(sizeof(struct packet) * rx_window_len);
  assert(ret.rx_pkt_buffer != NULL);

  for(int i=0; i < ret.rx_window_len; ++i)
  {
    ret.rx_pkt_buffer[i].data = NULL;
    ret.rx_pkt_buffer[i].pkt_len = 0;
    ret.rx_pkt_buffer[i].ref = 0;
  }

  #if __linux__
  // Use unix sockets for communication between threads
  for(int i=0; i < sizeof(ret.sockpair)/sizeof(ret.sockpair[0]); ++i)
  {
    int err;
    if((err=socketpair(AF_UNIX, SOCK_SEQPACKET | SOCK_NONBLOCK, 0, ret.sockpair[i])))
      perror("socketpair");
    assert(err == 0);
  }
  #elif __APPLE__
  // Use mach message queues for communication between threads
    #error "IPC on APPLE not implemented."
  #endif

  for(int i=0; i < sizeof(ret.work_threads)/sizeof(ret.work_threads[0]); ++i)
    ret.work_threads[i] = 0;

  ret.tx_pkt_buffer_ptr = 0;
  ret.rx_pkt_buffer_ptr = 0;

  flags = fcntl(input_fd, F_GETFL, 0);
  assert(fcntl(input_fd, F_SETFL, flags | O_NONBLOCK) == 0);

  assert(pthread_mutex_init(&ret.tx_pkt_buffer_mutex, NULL) == 0);
  assert(pthread_mutex_init(&ret.rx_pkt_buffer_mutex, NULL) == 0);

  return ret;
}

void delete_arq(Arq * arq)
{
  for(int i=0; i < arq->tx_window_len; ++i)
    if(arq->tx_pkt_buffer[i].data != NULL) free(arq->tx_pkt_buffer[i].data);
  free(arq->tx_pkt_buffer);

  for(int i=0; i < arq->rx_window_len; ++i)
    if(arq->rx_pkt_buffer[i].data != NULL) free(arq->rx_pkt_buffer[i].data);
  free(arq->rx_pkt_buffer);

  for(int i=0; i < 4; ++i)
  {
    close(arq->sockpair[i][0]);
    close(arq->sockpair[i][1]);
  }
}
