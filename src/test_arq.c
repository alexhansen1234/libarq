#include <string.h>
#include <unistd.h>
#include <fcntl.h>
#include "arq.h"
#include "test_arq.h"

#define TEST(x,y) do \
{ \
  int ret = x; \
  printf("[%s]: %s == %d\n", x==y?"PASS":"FAIL", #x, y); \
} while(0);

void do_tests(Arq * arq)
{
  TEST(do_test_sockets(arq),0);
  return;
}

int do_test_sockets(Arq * arq)
{
  const char teststring[] = "testing";
  char testbuffer1[1024];
  char testbuffer2[1024];

  for(int i=0; i < 4; ++i)
  {
    write(arq->sockpair[0][1], teststring, strlen(teststring)+1);
    write(arq->sockpair[0][1], teststring, strlen(teststring)+1);
    testbuffer1[read(arq->sockpair[0][0], testbuffer1, sizeof(testbuffer1))] = 0;
    testbuffer2[read(arq->sockpair[0][0], testbuffer2, sizeof(testbuffer2))] = 0;
    if(strncmp(testbuffer1, teststring, sizeof(testbuffer1)) != 0)
      return 1;
    if(strncmp(testbuffer2, teststring, sizeof(testbuffer2)) != 0)
      return 1;
  }
  return 0;
}
