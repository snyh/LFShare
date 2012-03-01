#ifndef _NATIVEFILE_HPP_
#define _NATIVEFILE_HPP_
#include "pre.hpp"
#include <boost/iostreams/device/mapped_file.hpp>
#include "BufType.hpp"
#include "CoreStruct.hpp"

class NativeFileManager {
public:
	NativeFileManager(int n);
	void run();
	void new_file(const FInfo&);
	void close(const Hash& h);

	void async_write(const Hash& h, long begin, const char* data, size_t s, std::function<void()> cb);
	void write(const Hash& h, long begin, const char* data, size_t s);
	char* read(const Hash& h, long begin);
private:
	void set_current_file(const Hash& h);
	void push_hot(const Hash& h);
	std::list<std::pair<Hash, boost::iostreams::mapped_file> > hot_;
	std::map<Hash, FInfo> files_;
	char* current_;
	int max_num_;
	boost::asio::io_service io_;
	boost::asio::io_service::work io_work_;
};
#endif

