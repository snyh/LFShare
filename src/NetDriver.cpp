#include "NetDriver.hpp"
#include <boost/date_time/posix_time/posix_time.hpp>

using namespace boost::asio;
using namespace std;
namespace pl = std::placeholders;

void NetDriver::run()
{
  io_service_.run();
}

void NetDriver::probe_local_ip()
{
  array<char, 1> buf;
  ip::udp::socket tmp(io_service_, ip::udp::v4());
  tmp.bind(ip::udp::endpoint());
  int port = tmp.local_endpoint().port();
  ip::udp::endpoint p;

  tmp.async_receive_from(buffer(buf), p,
						 [&](const boost::system::error_code& e, size_t s) {
						 local_ip_ = p.address();
						 });
  ssend_.send_to(buffer(buf), ip::udp::endpoint(ip::address_v4::broadcast(), port));
  io_service_.run();
  io_service_.reset();
}

NetDriver::NetDriver()
	:scmd_(io_service_, ip::udp::endpoint(ip::udp::v4(), UDP_CMD_PORT)),
	sdata_(io_service_, ip::udp::endpoint(ip::udp::v4(), UDP_DATA_PORT)),
	ssend_(io_service_, ip::udp::v4()),
	strand_(io_service_)
{ 
  // win(xp)下默认BufSize太小,8K. 
  socket_base::receive_buffer_size recv_buf(65536*4);
  sdata_.set_option(recv_buf);
  scmd_.set_option(recv_buf);

  socket_base::send_buffer_size send_buf(65536*4);
  ssend_.set_option(send_buf);
  ssend_.set_option(socket_base::broadcast(true));

  probe_local_ip();
  this->scmd_.async_receive_from(buffer(ibuf_), ep_cmd_,
								 strand_.wrap(
							bind(&NetDriver::handle_receive_cmd, this, pl::_1, pl::_2)));

  dbuf_ = RecvBufPtr(new RecvBuf);
  this->sdata_.async_receive_from(buffer(*dbuf_), ep_data_,
								  strand_.wrap(
							 bind(&NetDriver::handle_receive_data, this, pl::_1, pl::_2)));


}


void NetDriver::cmd_send(SendBufPtr buffer)
{
  static ip::udp::endpoint imulticast(ip::address_v4::broadcast(), UDP_CMD_PORT);
  ssend_.send_to(buffer->to_asio_buf(), imulticast);
}

void NetDriver::data_send(SendBufPtr buffer) 
{
  static ip::udp::endpoint dmulticast(ip::address_v4::broadcast(), UDP_DATA_PORT);
  ssend_.send_to(buffer->to_asio_buf(), dmulticast);
}

void NetDriver::handle_receive_cmd(const boost::system::error_code& ec, size_t byte_transferred)
{
  if (!ec && byte_transferred > 0 ) {
	  //发送cmd_receive信号以便业务模块可以处理具体数据
	  if (ep_cmd_.address() != local_ip_)
		call_cmd_plugin(byte_transferred);

	  //继续监听
	  this->scmd_.async_receive_from(buffer(ibuf_), ep_cmd_,
									 strand_.wrap(
								 bind(&NetDriver::handle_receive_cmd, this, pl::_1, pl::_2)));

  } else {
	  cerr << "!Error:" << boost::system::system_error(ec).what() << endl;
  }
}
void NetDriver::call_cmd_plugin(size_t s)
{
  NetMSG key = static_cast<NetMSG>(ibuf_.data()[0]);
  auto it = cbs_cmd_.find(key);
  if (it != cbs_cmd_.end())
	it->second(ibuf_.data()+1, s-1);
}

void NetDriver::handle_receive_data(const boost::system::error_code& ec, size_t byte_transferred)
{
  if (!ec && byte_transferred > 0) {
	  if (ep_data_.address() != local_ip_)
		cb_data_(dbuf_, byte_transferred);

	  dbuf_ = RecvBufPtr(new RecvBuf);
	  this->sdata_.async_receive_from(buffer(*dbuf_), ep_data_,
									  strand_.wrap(
		bind(&NetDriver::handle_receive_data,this, pl::_1, pl::_2))
									  );
  } else {
	  cerr << "!Error:" << boost::system::system_error(ec).what() << endl;
  }
}

void NetDriver::timer_helper(TimerPtr timer, int s, std::function<void()> cb)
{
  timer->expires_from_now(boost::posix_time::seconds(s));
  timer->async_wait([=](const boost::system::error_code&){
								cb();
								timer_helper(timer, s, cb);
								});
}
void NetDriver::start_timer(int s, std::function<void()> cb)
{
  TimerPtr timer(new deadline_timer(io_service_));
  timer_helper(timer, s, cb);
}

void NetDriver::add_task(int priority, std::function<void()> cb)
{
  //空闲进程的优先级最低
  io_service_.post(cb);
}
