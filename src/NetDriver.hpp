#ifndef __NetDriver_HPP__
#define __NetDriver_HPP__
#include "pre.hpp"
#include "BufType.hpp"
#include "CoreStruct.hpp"

class NetDriver {
public:
  typedef std::function<void(const char*, size_t)> RecvCmdFun;
  typedef std::function<void(RecvBufPtr, size_t)> RecvDataFun;
  typedef std::shared_ptr<boost::asio::deadline_timer> TimerPtr;

  NetDriver();
  void run();
  void stop();

  void register_cmd_plugin(NetMSG key, RecvCmdFun cb) {
	  this->cbs_cmd_.insert(make_pair(key, cb));
  }
  void register_data_plugin(RecvDataFun cb) {
	  cb_data_ = cb;
  }

  void cmd_send(SendBufPtr buffer);
  void data_send(SendBufPtr buffer);

  void start_timer(int s, std::function<void()> cb);
  void add_task(int priority, std::function<void()> cb);

private:
  void call_cmd_plugin(size_t s);

  void handle_receive_cmd(const boost::system::error_code& ec, size_t byte_transferred);
  void handle_receive_data(const boost::system::error_code& ec, size_t byte_transferred);

  void timer_helper(TimerPtr timer, int s, std::function<void()> cb);

private:
  boost::asio::io_service io_service_;

  boost::asio::ip::udp::socket scmd_;
  boost::asio::ip::udp::endpoint ep_cmd_;
  boost::asio::ip::udp::socket sdata_;
  boost::asio::ip::udp::endpoint ep_data_;

  /// 用来探测ssend_使用的IP地址，以便忽略该地址的信息
  void probe_local_ip();
  boost::asio::ip::address local_ip_;

  boost::asio::ip::udp::socket ssend_;

  std::array<char, 10240> ibuf_;
  RecvBufPtr dbuf_;


  std::map<NetMSG, RecvCmdFun> cbs_cmd_;;
  RecvDataFun cb_data_;

  boost::asio::io_service::strand strand_;
};

#endif
