#ifndef __CONNECTOR_HPP__
#define __CONNECTOR_HPP__

#include <boost/signals2/signal.hpp>
#include <functional>
#include <string>
#include <map>

#include "NetDriver.hpp"

struct Plugin {
	static const size_t KeyMaxSize = 16;
	enum Type {INFO, DATA};
	std::string key;
	Type type;
	std::function<void(const char*, size_t)> on_receive;
};


class Connector {
public:
  class SendBuffer {
  public:
	typedef std::vector<boost::asio::const_buffer> SequenceType;
	void add(const char* d, size_t s) {
		buf_.push_back(boost::asio::buffer(d, s));
	}
	SequenceType& value() {return buf_;}
  private:
	SequenceType buf_;
  };

public:
  typedef std::function<void(const char*, size_t)> ReceiveFunction;
  Connector();
  void data_receive(const char*, size_t);
  void info_receive(const char*, size_t);
  void register_plugin(Plugin& p);

  boost::signals2::signal<void(SendBuffer&)> data_send;
  boost::signals2::signal<void(SendBuffer&)> info_send;

  std::multimap<std::string, ReceiveFunction> cb_info_map_;
private:
  std::multimap<std::string, ReceiveFunction> cb_data_map_;
};

inline
Connector& theConnector()
{
  static Connector c;
  return c;
}
#endif
