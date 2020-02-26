#ifndef PACKET_HPP
  #define PACKET_HPP

#include <cstdint>
#include <vector>
#include <string>
#include <complex>

template <typename input_type, typename encoded_type>
class PacketBase
{
public:
  virtual ~PacketBase() {}
  virtual std::string toString() = 0;
  virtual void encode(input_type & data) = 0;

  int  set_send_id(int send_id);
  int  get_send_id(void);
  int  set_seq_num(int seq_num);
  int  get_seq_num(void);

protected:
  int seq_num;
  int send_id;
  encoded_type payload;
};

template <typename input_type, typename encoded_type>
int PacketBase<input_type, encoded_type>::set_send_id(int send_id)
{
  this->send_id = send_id;
  return this->send_id;
}

template <typename input_type, typename encoded_type>
int PacketBase<input_type, encoded_type>::get_send_id(void)
{
  return this->send_id;
}

template <typename input_type, typename encoded_type>
int PacketBase<input_type, encoded_type>::set_seq_num(int seq_num)
{
  this->seq_num = seq_num;
  return this->seq_num;
}

template <typename input_type, typename encoded_type>
int PacketBase<input_type, encoded_type>::get_seq_num(void)
{
  return this->seq_num;
}

template <typename input_type, typename encoded_type>
class Packet : PacketBase<input_type, encoded_type>
{
public:
    Packet(int seq_num, int send_id, encoded_type & data);
    ~Packet() {}
    std::string toString();
};

template <typename input_type, typename encoded_type>
Packet<input_type, encoded_type>::Packet(int seq_num, int send_id, input_type & data)
{
  this->seq_num = seq_num;
  this->send_id = send_id;
  this->payload = this->encode(data);
}

template <typename input_type, typename encoded_type>
void Packet<input_type, encoded_type>::encode(input_type & data)
{

}

template <typename input_type, typename encoded_type>
std::string Packet<input_type, encoded_type>::toString()
{
  std::string output = "";
  output += "Type: Packet\n";
  output += "Seq Num:\t" + std::to_string(this->get_seq_num()) + "\n";
  output += "Send ID:\t" + std::to_string(this->get_send_id()) + "\n";
  output += "Payload:\n\t";
  output += "\n";
  return output;
}




#endif
