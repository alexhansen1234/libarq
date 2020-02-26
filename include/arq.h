#ifndef ARQ_H
#define ARQ_H

#include <stdio.h>
#include <stdint.h>
#include <unistd.h>
#include <stdatomic.h>
#include <pthread.h>
#include <assert.h>
#include <sys/types.h>
#include <sys/time.h>

#define ARQ(x) ((Arq *)x)

enum
{
  ARQ_TX_HANDLER,
  ARQ_TX_DRIVER,
  ARQ_RX_HANDLER,
  ARQ_RX_DRIVER
};

enum
{
  ARQ_SOCK_TXH_TXD,
  ARQ_SOCK_RXH_TXD,
  ARQ_SOCK_RXD_RXH,
  ARQ_SOCK_RXD_TXH
};

enum
{
  ARQ_STOPPED,
  ARQ_RUNNING
};

struct packet
{
  void * data;
  size_t pkt_len;
  int ref;
};

typedef struct arq
{
  void * (* tx_handler)(void * args);
  void * (* tx_driver)(void * args);
  void * (* rx_handler)(void * args);
  void * (* rx_driver)(void * args);

  void (* pkt_encoder)(void * input, size_t data_len, void * output);
  void (* pkt_enc_destruct)(void * input);

  void (* pkt_decoder)(void * input, size_t data_size, void * output);
  void (* pkt_dec_destruct)(void * input);

  pthread_mutex_t tx_pkt_buffer_mutex;
  struct packet * tx_pkt_buffer;
  size_t tx_window_len;
  int tx_pkt_buffer_ptr;
  int tx_pkt_buffer_ackd;

  pthread_mutex_t rx_pkt_buffer_mutex;
  struct packet * rx_pkt_buffer;
  size_t rx_window_len;
  int rx_pkt_buffer_ptr;

  pthread_t work_threads[4];

  int input_fd;
  int output_fd;

  int running;
  int sockpair[4][2];

} Arq;

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
  void (* pkt_decoder)(void *, size_t, void *),
  void (* pkt_dec_destruct)(void *)
);

void delete_arq(Arq * arq);
void start_arq(Arq * arq);
void stop_arq(Arq * arq);

extern void * tx_handler(void *);
extern void * tx_driver(void *);
extern void * rx_handler(void *);
extern void * rx_driver(void *);
extern void pkt_encoder(void * input, size_t input_size, void * output);
extern void pkt_enc_destruct(void *);
extern void pkt_decoder(void * input, size_t input_size, void * output);
extern void pkt_dec_destruct(void *);

#endif
