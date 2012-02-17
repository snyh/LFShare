#ifndef _TRANSPOR_HPP__
#define _TRANSPOR_HPP__
#include <string>
#include <map>
#include <boost/iostreams/device/mapped_file.hpp>
#include <boost/dynamic_bitset.hpp>
#include "NetDriver.hpp"

typedef std::string Hash;

struct Payload {
	int global;
	std::map<Hash, int> files;
};
class NativeFile {
public:
  NativeFile(std::string path);
  ~NativeFile();
  void write(long begin, const char* data, size_t s);
  void read(long begin, char* data, size_t s);
private:
  boost::iostreams::mapped_file file_;
};

class NetDriver;
class FInfoManager;
class Bill;
class Chunk;

class Transport {
public:
  	Transport(FInfoManager& info_manager);
	void start_receive(const Hash& file_hash);
	void stop_receive(const Hash& file_hash);
	void add_local_file(const Hash& file_hash, const std::string& path);
	void del_local_file(const Hash& file_hash);
	void add_completed_file(const Hash& h);
	void del_completed_file(const Hash& h);
	Payload get_payload();

	void run();

private:
	void send_chunks();
	void send_bill();

	void handle_chunk(const Chunk& c);
	void handle_bill(const Bill& b);
	void record_speed();

	Payload payload_;
	Payload payload_tmp_;

	//记录整个网络中缺少的文件块并且本地拥有的文件块
	std::map<Hash, boost::dynamic_bitset<> > global_bill_;

	//记录正在下载的文件所缺文件块
	std::map<Hash, boost::dynamic_bitset<> > local_bill_;

	//正在下载中的文件列表
	std::map<Hash, NativeFile> downloading_;
	//本地文件或已经完成下载的文件列表
	std::map<Hash, NativeFile> completed_;

	FInfoManager& info_manager_;
	NetDriver ndriver_;
};

#endif
