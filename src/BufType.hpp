#ifndef __BUFTYPE_HPP_
#define __BUFTYPE_HPP_
#include "pre.hpp"
#include "config.hpp"

class SendBuf {
public:
	void add_val(const std::string& s) {
		tmp_.push_back(s);
		asio_.push_back(boost::asio::buffer(tmp_[tmp_.size()-1]));
	}
	void add_val(void* t, size_t s) {
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
typedef std::shared_ptr<SendBuf> SendBufPtr;

#define RECVBUFLEN (16+16+16+4+4+CHUNK_SIZE)  
typedef std::array<char, RECVBUFLEN> RecvBuf;
typedef std::shared_ptr<RecvBuf> RecvBufPtr;

#endif
