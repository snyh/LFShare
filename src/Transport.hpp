#ifndef _TRANSPOR_HPP__
#define _TRANSPOR_HPP__
#include <string>
#include <map>

typedef std::string Hash;

struct Payload {
	int global;
	std::map<Hash, int> files;
};

class Transport {
public:
	void begin_receive(const Hash& file_hash);
	void stop_receive(const Hash& file_hash);
	void add_local_file(const Hash& file_hash, const std::string& path);
	void del_local_file(const Hash& file_hash);
	const Payload& get_payload();

private:
	void on_receive_msg();
	void on_receive_data();
	Payload payload_;
	Payload payload_tmp_;

	//记录整个网络中还缺少的文件块包括本地暂时没有的文件块
	std::map<Hash, boost::dynamic_bitset<> > global_bill_;

	//记录正在下载的文件所缺文件块
	std::map<Hash, boost::dynamci_bitset<> > local_bill_;

	std::map<Hash, NativeFile> downloading_;
};

#endif
