#ifndef __NetDriver_HPP__
#define __NetDriver_HPP__
#include <boost/asio.hpp>
#include <functional>
#include <string>
#include <array>
#include <map>
#include <queue>

class NetBuf {
public:
	void add_val(const std::string& s) {
		tmp_.push_back(s);
		asio_.push_back(boost::asio::buffer(tmp_[tmp_.size()-1]));
	}
	template<typename T>
	  void add_val(T t, size_t s) {
		  tmp_.push_back(std::string((char*)t, s));
		  asio_.push_back(boost::asio::buffer(tmp_[tmp_.size()-1]));
	  }
	void add_ref(const char*data, size_t s) {
		asio_.push_back(boost::asio::buffer(data, s));
	}
	std::vector<boost::asio::const_buffer>& to_asio_buf() { return asio_; }
private:
	std::vector<boost::asio::const_buffer> asio_;
	std::vector<std::string> tmp_;
};
typedef std::shared_ptr<NetBuf> NetBufPtr;

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
  enum PluginType {INFO, DATA};
  typedef std::function<void(const char*, size_t)> ReceiveFunction;
  typedef shared_ptr<deadline_timer> TimerPtr;

  NetDriver();
  void run();
  void stop();

  void register_plugin(const PluginType, const std::string& key, ReceiveFunction cb);

  void info_send(NetBufPtr buffer);
  void data_send(NetBufPtr buffer);

  void start_timer(int s, std::function<void()> cb, TimerPtr timer=TimerPtr(new Timer));
  void on_idle(std::function<void()> cb);

private:
  typedef std::multimap<std::string, ReceiveFunction> CBS;
  void handle_receive(CBS& cbs, const char* data, size_t s);

  void receive_info(const boost::system::error_code& ec, size_t byte_transferred);
  void receive_data(const boost::system::error_code& ec, size_t byte_transferred);

private:
  boost::asio::io_service io_service_;

  boost::asio::ip::udp::socket sinfo_;
  boost::asio::ip::udp::socket sdata_;

  boost::asio::ip::udp::socket ssend_;

  std::array<char, 1024> ibuf_;
  std::array<char, 4+16+65536> dbuf_;

  CBS cb_info_;
  CBS cb_data_;


  HandlerPriorityQueue queue_;
};

#endif
