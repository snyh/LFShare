#include "NetDriver.hpp"
#include <boost/date_time/posix_time/posix_time.hpp>

using namespace boost::asio;
using namespace std;
namespace pl = std::placeholders;

void NetDriver::run()
{
  while (io_service_.run_one()) {
	  while (io_service_.poll_one())
		;
	  queue_.execute_all();
  }
}

NetDriver::NetDriver()
	:scmd_(io_service_, ip::udp::endpoint(ip::udp::v4(), UDP_CMD_PORT)),
	sdata_(io_service_, ip::udp::endpoint(ip::udp::v4(), UDP_DATA_PORT)),
	ssend_(io_service_, ip::udp::v4())
{ 
  this->scmd_.async_receive(buffer(ibuf_), queue_.wrap(100, bind(&NetDriver::handle_receive_cmd, this, pl::_1, pl::_2)));

  dbuf_ = RecvBufPtr(new RecvBuf);
  this->sdata_.async_receive(buffer(*dbuf_), queue_.wrap(100, bind(&NetDriver::handle_receive_data, this, pl::_1, pl::_2)));

  ssend_.set_option(socket_base::broadcast(true));
}


void NetDriver::cmd_send(SendBufPtr buffer)
{
  static ip::udp::endpoint imulticast(ip::address::from_string("255.255.255.255"), UDP_CMD_PORT);
  io_service_.post(queue_.wrap(20, [=](){ssend_.send_to(buffer->to_asio_buf(), imulticast);}));
}

void NetDriver::data_send(SendBufPtr buffer) 
{
  static boost::asio::ip::udp::endpoint dmulticast(ip::address::from_string("255.255.255.255"), UDP_DATA_PORT);
  io_service_.post(queue_.wrap(20, [=](){ssend_.send_to(buffer->to_asio_buf(), dmulticast);}));
}

void NetDriver::handle_receive_cmd(const boost::system::error_code& ec, size_t byte_transferred)
{
  if (!ec && byte_transferred > 0) {
	  //发送cmd_receive信号以便业务模块可以处理具体数据
	  call_cmd_plugin(byte_transferred);

	  //继续监听
	  this->scmd_.async_receive(buffer(ibuf_),
								 queue_.wrap(100, bind(&NetDriver::handle_receive_cmd, this, pl::_1, pl::_2)));

  } else {
	  cerr << "Error:" << boost::system::system_error(ec).what();
  }
}
void NetDriver::call_cmd_plugin(size_t s)
{
  static const size_t KeyMaxSize = 16;

  int pos = -1;
  char* data = ibuf_.data();
  pos = find(data, data+KeyMaxSize, '\n') - data;
  if (pos <= 0) 
	return;
  string key(data, pos);

  auto it = cbs_cmd_.find(key);
  if (it != cbs_cmd_.end())
	it->second(data+pos+1, s-pos-1);
}

void NetDriver::handle_receive_data(const boost::system::error_code& ec, size_t byte_transferred)
{
  if (!ec && byte_transferred > 0) {
	  cb_data_(dbuf_, byte_transferred);

	  dbuf_ = RecvBufPtr(new RecvBuf);
	  this->sdata_.async_receive(buffer(*dbuf_),
		queue_.wrap(100, bind(&NetDriver::handle_receive_data,this, pl::_1, pl::_2)));
  } else {
	  cerr << "Error:" << boost::system::system_error(ec).what();
  }
}

void NetDriver::timer_helper(TimerPtr timer, int s, std::function<void()> cb)
{
  timer->expires_from_now(boost::posix_time::seconds(s));
  timer->async_wait(queue_.wrap(50, [=](boost::system::error_code&){
								cb();
								timer_helper(timer, s, cb);
								})
				   );
}
void NetDriver::start_timer(int s, std::function<void()> cb)
{
  TimerPtr timer(new deadline_timer(io_service_));
  timer_helper(timer, s, cb);
}

void NetDriver::add_task(int priority, std::function<void()> cb)
{
  //空闲进程的优先级最低
  io_service_.post(queue_.wrap(priority, cb));
}
