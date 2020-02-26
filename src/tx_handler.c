#include "arq.h"

void * insert_packet(Arq * arq, char * buffer, size_t len);
void transmit_packet(Arq * arq);
void handle_ack(Arq * arq);
void handle_retransmits(Arq * arq);

void * tx_handler(void * args)
{
  ssize_t readlen;
  char buffer[4*1024];
  Arq * arq = ARQ(args);

  while(arq->running == ARQ_RUNNING)
  {
    // fill up tx buffer to window size, unless...
    if( arq->tx_pkt_buffer_ptr < arq->tx_window_len )
    {
      readlen = read(arq->input_fd, buffer, sizeof(buffer));
      if(readlen > 0)
      {
        printf("%.*s\n", (int)readlen, buffer);
        insert_packet(arq, buffer, readlen);
        transmit_packet(arq);
      }
    }

    // there is no data to transmit currently, otherwise, do...
    if(readlen < 0)
    {
      handle_ack(arq);
      handle_retransmits(arq);
    }
  }

  return NULL;
}

void * insert_packet(Arq * arq, char * buffer, size_t len)
{
  return NULL;

}

void transmit_packet(Arq * arq)
{

  return;
}
void handle_ack(Arq * arq)
{
  return;
}
void handle_retransmits(Arq * arq)
{
  return;
}
