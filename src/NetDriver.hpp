#ifndef __NetDriver_HPP__
#define __NetDriver_HPP__
#include "pre.hpp"
#include "BufType.hpp"

class HandlerPriorityQueue {
public:
  	void add(int priority, std::function<void()> function) {
		handlers_.push(QueuedHandler(priority, function));
	}
	void execute_all() {
		while (!handlers_.empty()) {
			auto handler = handlers_.top();
			handler.execute();
			handlers_.pop();
		}
	}

	template<typename Handler>
	class WrappedHandler {
	public:
	  WrappedHandler(HandlerPriorityQueue& q, int p, Handler h)
		: queue_(q), priority_(p), handler_(h) {
	  }
	  template<typename... Args>
		void operator()(Args... args) {
			handler_(args...);
		}
	  HandlerPriorityQueue& queue_;
	  int priority_;
	  Handler handler_;
	};

	template <typename Handler>
	  WrappedHandler<Handler> wrap(int priority, Handler handler) {
		  return WrappedHandler<Handler>(*this, priority, handler);
	  }

private:
	class QueuedHandler {
	public:
	  QueuedHandler(int p, std::function<void()> f)
		: priority_(p), function_(f) {
		}
	  void execute() {
		  function_();
	  }
	  friend bool operator< (const QueuedHandler& a, const QueuedHandler& b) {
		  return a.priority_ < b.priority_;
	  }
	private:
	  int priority_;
	  std::function<void()> function_;
	};
	std::priority_queue<QueuedHandler> handlers_;
};
template <typename Function, typename Handler>
void asio_handler_invoke(Function f, HandlerPriorityQueue::WrappedHandler<Handler>* h)
{
  h->queue_.add(h->priority_, f);
}

class NetDriver {
public:
  typedef std::function<void(const char*, size_t)> RecvCmdFun;
  typedef std::function<void(RecvBufPtr, size_t)> RecvDataFun;
  typedef std::shared_ptr<boost::asio::deadline_timer> TimerPtr;

  NetDriver();
  void run();
  void stop();

  void register_cmd_plugin(const std::string& key, RecvCmdFun cb) {
	  assert(key.size() < 16);
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
  boost::asio::ip::udp::socket sdata_;

  boost::asio::ip::udp::socket ssend_;

  std::array<char, 10240> ibuf_;
  RecvBufPtr dbuf_;


  std::map<std::string, RecvCmdFun> cbs_cmd_;;
  RecvDataFun cb_data_;

  HandlerPriorityQueue queue_;
};

#endif
