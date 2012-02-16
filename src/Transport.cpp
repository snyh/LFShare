#include <sstream>
#include "FInfo.hpp"
#include "NetDriver.hpp"

using namespace std;
using namespace boost;

//Chunk用来包装一块数据(当前默认大小64KB)，可以在网络上进行安全的传输
//可以通过Chunk来组装完整的文件。
//Chunk本身不动态分配内存,因此Chunk体积较小,大概占用40byte的内存(32位系统)，
struct Chunk {
	Chunk(int index, Hash file_hash, size_t size, const char* data) {
		this->index = index;
		this->size = size;
		this->data = data;
		this->chunk_hash_ = hash_data(data, size);
		this->file_hash = file_hash;
	}
	Chunk(int index, Hash file_hash, size_t size, const char* data, Hash hash);
	Hash file_hash;
	Hash chunk_hash_;
	int index;
	const char* data;
	int size;
};

class NativeFile {
public:
  NativeFile(string path);
  ~NativeFile();
  void write(long begin, const char* data, size_t s);
  void read(long begin, char* data, size_t s);
private:
  iostreams::mapped_file file_;
};


Transport::Transport()
{
  //注册相关消息
  ndriver_.register_plugin(NetDriver::INFO, "FILEINFO", [&](const char*data, size_t s){
					 infos_.add_info("local", FInfo::from_net(data, s));
					 });
  ndriver_.register_plugin(NetDriver::DATA, "CHUNKS", [&](const char*data, size_t s){
					 write_chunk(Chunk::from_net(data, s));
					 });
  ndriver_.register_plugin(NetDriver::INFO, "REQUIRECHUNKS", [&](const char*data, size_t s){
					 });
  //注册定时器以便统计速度
  ndriver_.start_timer(1000, bind(&Transport::record_speed, this));
}

void Transport::write_chunk(const Chunk& c)
{
  payload_.globa++;
  if (c.valid()) {
	  auto it = local_bill_.find(c.file_hash);
	  if (it!= local_bill_.end() && it->second.test(c.index)) {
		  it->second.reset(c.index);

		  auto f_it= downloading_.find(c.file_hash);
		  if (f_it != downloading_.end()) {
			  auto& file = f_it->second;
			  file.write(c.index*FInfo::chunksize, c.data, c.size);
			  payload_.files[c.file_hash]++;
		  }
	  }
  }
}

//每隔一秒调用一次record_speed函数
void Transport::record_speed()
{
  //更新上一秒记录下的数据
  payload_ = payload_tmp_;
  //清空payload_tmp_以便下次重新计算
  payload_tmp_ = Payload();
  ndriver_.start_timer(1000, bind(&Transport::record_speed, this));
}

void Transport::begin_receive(Hash h)
{
}

void Transport::stop_receive(Hash h)
{
}
void Transport::add_local_file(Hash h)
{
}
Payload Transport::get_payload()
{
}

void Transport::on_receive_info()
{
}

void Transport::on_receive_data()
{
}

