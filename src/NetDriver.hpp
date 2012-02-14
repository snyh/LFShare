#ifndef __NetDriver_HPP__
#define __NetDriver_HPP__
#include <boost/signals2/signal.hpp>
#include <boost/asio.hpp>
#include <array>

class NetDriver {
public:
  NetDriver(int pinfo, int pdata);
  void run();
  void stop();

  template<typename SequenceBuffer>
	void info_send(SequenceBuffer buffer) {
		this->ssend_.send_to(buffer, imulticast_);
	}

  template<typename SequenceBuffer>
	void data_send(SequenceBuffer buffer) {
		this->ssend_.send_to(buffer, dmulticast_);
	}

  //接受到数据时会触发此信号
  boost::signals2::signal<void(const char*, size_t)> data_receive;
  //接受到信息时会触发此信号
  boost::signals2::signal<void(const char*, size_t)> info_receive;

private:
  void receive_info(const boost::system::error_code& ec,
					size_t byte_transferred);
  void receive_data(const boost::system::error_code& ec, 
					size_t byte_transferred);

private:
  boost::asio::io_service io_service_;
  boost::asio::ip::udp::socket sinfo_;
  boost::asio::ip::udp::socket sdata_;
  boost::asio::ip::udp::socket ssend_;
  boost::asio::ip::udp::endpoint imulticast_;
  boost::asio::ip::udp::endpoint dmulticast_;

  std::array<char, 1024> ibuf_;
  std::array<char, 4+16+65536> dbuf_;
};


NetDriver& theND();
#endif
