#include <stdio.h>
#include <stdlib.h>
#include <signal.h>
#include "arq.h"
#include "test_arq.h"

int * running;

void install_sigint_handler(void);

int main(int argc, char ** argv)
{
  Arq arq = init_arq(
    fileno(stdin),
    fileno(stdout),
    1,
    1,
    tx_handler,
    tx_driver,
    rx_handler,
    rx_driver,
    pkt_encoder,
    pkt_enc_destruct,
    pkt_decoder,
    pkt_dec_destruct
  );

  running = &(arq.running);

  install_sigint_handler();

  do_tests(&arq);

  start_arq(&arq);

  while(ARQ_RUNNING == *running)
    continue;

  stop_arq(&arq);

  delete_arq(&arq);

  return 0;
}

static void sigint_handler(int signum)
{
  *running = ARQ_STOPPED;
  printf("\n");
  printf("Cntl-C Caught, exiting.\n");
}

void install_sigint_handler(void)
{
  struct sigaction sa;
  sa.sa_handler = sigint_handler;
  sigemptyset(&sa.sa_mask);
  sa.sa_flags = SA_RESTART;
  assert(sigaction(SIGINT, &sa, NULL) == 0);
  return;
}
