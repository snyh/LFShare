#ifndef __NetDriver_HPP__
#define __NetDriver_HPP__
#include <boost/asio.hpp>
#include <functional>
#include <string>
#include <array>
#include <map>

class NetDriver {
public:
  enum PluginType {INFO, DATA};
  typedef std::function<void(const char*, size_t)> ReceiveFunction;

  NetDriver(int pinfo, int pdata);
  void run();
  void stop();

  void register_plugin(const PluginType, const std::string& key, ReceiveFunction cb);

  template<typename SequenceBuffer> 
	void data_send(SequenceBuffer buffer);
  template<typename SequenceBuffer> 
	void info_send(SequenceBuffer buffer);

  void start_timer(int seconds, std::function<void()> cb);

private:
  typedef std::multimap<std::string, ReceiveFunction> CBS;
  void NetDriver::handle_receive(CBS& cbs, const char* data, size_t s)

  void receive_info(const boost::system::error_code& ec,
					size_t byte_transferred);
  void receive_data(const boost::system::error_code& ec, 
					size_t byte_transferred);

private:
  boost::asio::io_service io_service_;

  boost::asio::ip::udp::socket sinfo_;
  int pinfo_;
  boost::asio::ip::udp::socket sdata_;
  int pdata_;

  boost::asio::ip::udp::socket ssend_;

  std::array<char, 1024> ibuf_;
  std::array<char, 4+16+65536> dbuf_;

  CBS cb_info_;
  CBS cb_data_;

  boost::asio::deadline_timer timter_;
};

#endif
