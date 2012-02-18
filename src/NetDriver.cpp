#include "NetDriver.hpp"
#include <functional>
#include <boost/date_time/posix_time/posix_time.hpp>

using namespace boost::asio;
using namespace std;
namespace pl = std::placeholders;
const int PINFO = 18068;
const int PDATA = 18069;

void NetDriver::run()
{
  while (io_service_.run_one()) {
	  while (io_service_.poll_one())
		;
	  queue_.execute_all();
  }
}

NetDriver::NetDriver()
	:sinfo_(io_service_, ip::udp::endpoint(ip::udp::v4(), PINFO)),
	sdata_(io_service_, ip::udp::endpoint(ip::udp::v4(), PDATA)),
	ssend_(io_service_, ip::udp::v4())
{ 
  this->sinfo_.async_receive(buffer(ibuf_), queue_.wrap(100, bind(&NetDriver::receive_info, this, pl::_1, pl::_2)));
  this->sdata_.async_receive(buffer(dbuf_), queue_.wrap(100, bind(&NetDriver::receive_data, this, pl::_1, pl::_2)));

  ssend_.set_option(socket_base::broadcast(true));
}

void NetDriver::register_plugin(const PluginType type, const std::string& key, ReceiveFunction cb)
{
  if (key.size() > 16) {
	  cerr << "Warning: KeySize must smaller than 16\n";
  }
  if (type == INFO)
	this->cb_info_.insert(make_pair(key, cb));
  else if (type == DATA)
	this->cb_data_.insert(make_pair(key, cb));
}

void NetDriver::info_send(NetBufPtr buffer)
{
  static ip::udp::endpoint imulticast(ip::address::from_string("255.255.255.255"), PINFO);
  io_service_.post(queue_.wrap(20, [=](){ssend_.send_to(buffer->to_asio_buf(), imulticast);}));
}

void NetDriver::data_send(NetBufPtr buffer) 
{
  static boost::asio::ip::udp::endpoint dmulticast(ip::address::from_string("255.255.255.255"), PDATA);
  io_service_.post(queue_.wrap(20, [=](){ssend_.send_to(buffer->to_asio_buf(), dmulticast);}));
}

void NetDriver::receive_info(const boost::system::error_code& ec, size_t byte_transferred)
{
  if (!ec && byte_transferred > 0) {
	  //发送info_receive信号以便业务模块可以处理具体数据
	  handle_receive(cb_info_, ibuf_.data(), byte_transferred);

	  //继续监听
	  this->sinfo_.async_receive(buffer(ibuf_),
								 queue_.wrap(100, bind(&NetDriver::receive_info, this, pl::_1, pl::_2)));

  } else {
	  cerr << "Error:" << boost::system::system_error(ec).what();
  }
}

void NetDriver::receive_data(const boost::system::error_code& ec, size_t byte_transferred)
{
  if (!ec && byte_transferred > 0) {
	  //发送data_receive信号以便业务模块可以处理具体数据
	  handle_receive(cb_data_, dbuf_.data(), byte_transferred);

	  //继续监听
	  this->sdata_.async_receive(buffer(dbuf_), 
								 queue_.wrap(100, bind(&NetDriver::receive_data, this, pl::_1, pl::_2)));

  } else {
	  cerr << "Error:" << boost::system::system_error(ec).what();
  }
}

void NetDriver::handle_receive(CBS& cbs, const char* data, size_t s)
{
  static const size_t KeyMaxSize = 16;

  int pos = -1;
  pos = find(data, data+KeyMaxSize, '\n') - data;
  if (pos <= 0) 
	return;
  string key(data, pos);

  for(auto it = cbs.find(key); it != cbs.end(); it++) {
	  it->second(data+pos+1, s-pos-1);
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
