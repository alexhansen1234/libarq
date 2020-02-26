#include "arq.h"

void debug_arq(Arq * this);

void start_arq(Arq * this)
{
  printf("%s\n", __func__);
  this->running = ARQ_RUNNING;
  assert(pthread_create(&this->work_threads[ARQ_TX_HANDLER], NULL, this->tx_handler, this) == 0);
  assert(pthread_create(&this->work_threads[ARQ_TX_DRIVER], NULL, this->tx_driver, this) == 0);
  assert(pthread_create(&this->work_threads[ARQ_RX_HANDLER], NULL, this->rx_handler, this) == 0);
  assert(pthread_create(&this->work_threads[ARQ_RX_DRIVER], NULL, this->rx_driver, this) == 0);
  return;
}
