#include "arq.h"

void stop_arq(Arq * this)
{
  this->running = ARQ_STOPPED;
  pthread_join(this->work_threads[0], NULL);
  pthread_join(this->work_threads[1], NULL);
  pthread_join(this->work_threads[2], NULL);
  pthread_join(this->work_threads[3], NULL);
  return;
}
