#include "NetDriver.hpp"
#include <functional>
#include <thread>
using namespace boost::asio;
using namespace std;
namespace pl = std::placeholders;

NetDriver::NetDriver(int pinfo, int pdata) 
	: sinfo_(io_service_, ip::udp::endpoint(ip::udp::v4(), pinfo)),
	  sdata_(io_service_, ip::udp::endpoint(ip::udp::v4(), pdata)),
	  ssend_(io_service_, ip::udp::v4()),
	  imulticast_(ip::address::from_string("255.255.255.255"), pinfo),
	  dmulticast_(ip::address::from_string("255.255.255.255"), pdata)
{ 
  this->sinfo_.async_receive(buffer(ibuf_), 
							 bind(&NetDriver::receive_info, 
								  this, pl::_1, pl::_2));
  this->sdata_.async_receive(buffer(dbuf_, 65536),
							 bind(&NetDriver::receive_data,
								  this, pl::_1, pl::_2));


  socket_base::broadcast option(true);
  ssend_.set_option(option);
}

void NetDriver::receive_info(const boost::system::error_code& ec, size_t byte_transferred)
{
  if (!ec && byte_transferred > 0) {
	  //发送info_receive信号以便业务模块可以处理具体数据
	  info_receive(ibuf_.data(), byte_transferred);

	  //继续监听
	  this->sinfo_.async_receive(buffer(ibuf_),
								 bind(&NetDriver::receive_info, this, _1, _2));

  } else {
	  cerr << "Error:" << boost::system::system_error(ec).what();
  }
}
void NetDriver::receive_data(const boost::system::error_code& ec, size_t byte_transferred)
{
  cerr << "Debug: receive_data  transferred size:" << byte_transferred << endl;
  if (!ec && byte_transferred > 0) {
	  //发送data_receive信号以便业务模块可以处理具体数据
	  data_receive(dbuf_.data(), byte_transferred);

	  //继续监听
	  this->sdata_.async_receive(buffer(dbuf_),
								 bind(&NetDriver::receive_data, this, _1, _2));

  } else {
	  cerr << "Error:" << boost::system::system_error(ec).what();
  }
}

void NetDriver::run()
{
  io_service_.run();
}
NetDriver& theND()
{
  static NetDriver nd(18068, 18069);
  return nd;
}
