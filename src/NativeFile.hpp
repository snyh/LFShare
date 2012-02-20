#ifndef _NATIVEFILE_HPP_
#define _NATIVEFILE_HPP_
#include <map>
#include <list>
#include <string>
#include "FInfo.hpp"
#include <boost/iostreams/device/mapped_file.hpp>

typedef std::string Hash;

class NativeFileManager {
public:
	NativeFileManager(int n): current_(nullptr), max_num_(n) {}
	void new_file(const FInfo&);
	void write(const Hash& h, long begin, const char* data, size_t s);
	void read(const Hash& h, long begin, char* data, size_t s);
private:
	void set_current_file(const Hash& h);
	void push_hot(const Hash& h);
	std::list<std::pair<Hash, boost::iostreams::mapped_file> > hot_;
	std::map<Hash, FInfo> files_;
	char* current_;
	int max_num_;
};
#endif

