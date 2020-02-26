#include <iostream>
#include <complex>
#include "packet.hpp"

int main(int argc, char ** argv)
{
  std::vector<uint8_t> data = {0, 1, 2, 3, 4, 5, 6, 7};
  Packet<uint8_t,std::vector<std::complex<float>>> pkt(0,0,data);
  std::cout << pkt.toString() << "\n";
  return 0;
}
