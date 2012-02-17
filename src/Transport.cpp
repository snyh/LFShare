#include <sstream>
#include <boost/iostreams/device/mapped_file.hpp>
#include "FInfo.hpp"
#include "NetDriver.hpp"
#include "Transport.hpp"

using namespace std;
using namespace boost;

//Chunk用来包装一块数据(当前默认大小64KB)，可以在网络上进行安全的传输
//可以通过Chunk来组装完整的文件。
//Chunk本身不动态分配内存,因此Chunk体积较小,大概占用40byte的内存(32位系统)，
struct Chunk {
	Chunk(Hash fh, int i, size_t s, const char* d) 
	  : file_hash(fh),
	  	chunk_hash(d, s),
		index(i),
		size(s),
		data(d),
		is_valid_(true) {
	}
	Chunk(Hash fh, Hash h, int i, size_t s, const char* d) 
	  : file_hash(fh),
	  	chunk_hash(h),
		index(i),
		size(s),
		data(d) {
	  if (chunk_hash == hash_data(data, size))
		is_valid_ = true;
	  else
		is_valid_ = false;
	}

	bool valid() { return is_valid_; }
	Hash file_hash;
	Hash chunk_hash;
	int index;
	int size;
	const char* data;
private:
	bool is_valid_;
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

void Transport::run() 
{
  ndriver_.run(); 
}

class ByteReader {
public:
	ByteReader(const char* data, size_t s)
	  : pos_(0), data_(data), max_size_(s) {}
	template<typename T>
	  T read() {
		  T t = *(T*)(data+pos_);
		  pos_ += sizeof(T);
		  return t;
	  }
	string str(int size) {
		string str(data_+pos_, size);
		pos_ += size;
		//if (pos_ > max_size_)
		return str;
	}
	string str() {
		int s = this.read<int>();
		return str(s);
	}
	const char* data() { return data_ + pos_; }

private:
	int pos_;
	const char* data_;
	size_t max_size_;
};

FInfo info_from_net(const char* data, size_t s)
{
  FInfo info;
  ByteReader r(data, s);
  info.path = r.str();
  info.hash = r.str(16);
  info.chunknum = r.read<int>();
  info.lastchunksize = r.read<size_t>();
}
Chunk chunk_from_net(const char* data, size_t s)
{
  Chunk c;
  ByteReader r(data, s);
  c.file_hash = r.str(16);
  c.chunk_hash = r.str(16);
  c.index = r.read<int>();
  c.size = r.read<size_t>();
  c.data = r.data();
  return c;
}
Bill bill_from_net(const char* data, size_t s)
{
}

Transport::Transport(FInfoManager& info_manager)
	 :info_manager_(info_manager),
	 ndriver_()
{
  //注册相关消息
  ndriver_.register_plugin(NetDriver::INFO, "FILEINFO", [&](const char*data, size_t s){
						   info_manager_.add_info("local", from_net<FInfo>(data, s));
						   });
  ndriver_.register_plugin(NetDriver::DATA, "CHUNK", [&](const char*data, size_t s){
						   handle_chunk(chunk_from_net(data, s));
						   });
  ndriver_.register_plugin(NetDriver::INFO, "REQUIRECHUNKS", [&](const char*data, size_t s){
						   record_chunk(bill_from_net(data, s));
						   });
  //注册定时器以便统计速度
  ndriver_.start_timer(1000, bind(&Transport::record_speed, this));
  ndriver_.on_idle(bind(&Transport::send_chunk, this));
}

void Transport::record_chunk(Bill& b)
{
  //如果所请求文件本地拥有完整拷贝则全部添加到global_bill中
  auto it = completed_.find(b.hash);
  if (it != completed_.end())
	global_bill_[b.hash] |= b.bits;

  //如果所请求文件在下载队列中，则统计拥有的文件块并添加到gloabl_bill中
  it = downloading_.find(b.hash);
  if (it != downloading_.end())
	global_bill_[b.hash] |= (!local_bill_[b.hash] & b.bits);
}

void Transport::handle_chunk(const Chunk& c)
{
  if (c.valid()) {
	  payload_.globa++;

	  //如果是下载队列中的文件
	  auto f_it= downloading_.find(c.file_hash);
	  if (f_it != downloading_.end()) {

		  //并且是此文件所缺少的文件块
		  auto it = local_bill_.find(c.file_hash);
		  if (it != local_bill_.end() && it->second[c.index]) {
			  //就取消文件块缺失标记
			  it->second[c.index] = false;
			  //并写入文件
			  auto& file = f_it->second;
			  file.write(c.index*FInfo::chunksize, c.data, c.size);

			  //同时记录此文件有效获得一次文件块以便统计下载速度
			  payload_.files[c.file_hash]++;
		  } 
	  }

	  //如果是全局缺少的文件块则取消文件块缺失标记
	  it = global_bill_.find(c.file_hash);
	  if (it != global_bill_.end() && it->seoncd[c.index]) {
		  it->second[c.index] = false;
	  }
  }
}

void send_chunk_helper(vector<Chunk>& chunks,
					   Hash& h,
					   boost::dynamic_bitset<>& bits;
					   map<Hash, NativeFile>& files,
					   map<Hash, boost::dynamic_bitset<> >& bill
					  )
{
  auto it = files.find(h);
  if (it != files.end()) {
	  int length = bits.size();
	  int i;
	  for (i=0; i<length; i++) {
		  if (bits[i]) {
			  size_t size = (i == length) ? : FInfo::chunksize;
			  char* data;
			  it->second.read(i*FInfo::chunksize, data, size);

			  chunks.push_back(Chunk(it->first, i, data, size));
			  if (chunks.size() > 5)
				return;
		  }
	  }
	  //去除无效bills(所有文件块全班拥有的文件不应该出现在bill中)
	  if (i == length) {
		  bill.erase(h);
	  }
  }
}

void Transport::send_chunk()
{
  //生成不多于5个文件块
  vector<Chunk> chunks;
  for (auto bill : global_bill_) {
	  send_chunk_helper(chunks, bill.first, bill.second, completed_);
	  if (chunks.size() > 5)
		break;
	  send_chunk_helper(chunks, bill.first, bill.second, downloading_);
  }

  //发送文件块数据到网络
  for (auto& c: chunks) {
	  NetBufPtr buf(new NetBuf);
	  buf.add_val("CHUNK");
	  buf.add_val(c.file_hash);
	  buf.add_val(c.hash);
	  buf.add_val(&(c.index), sizeof(int));
	  buf.add_val(&(c.size), sizeof(size_t));
	  buf.add_ref(c.data, c.size);
	  ndriver_.data_send(buf);
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


void Transport::start_receive(Hash h)
{
  //如果当前文件不在dowloading列表中则添加之
  if (downloading_.find(h) == downloading_.end()) {
	  auto info = info_manager_.find(h);
	  downloading_.insert(make_pair(h, NativeFile(info.path)));
  }
}
void Transport::stop_receive(Hash h)
{
  downloading_.erase(h);
}

void Transport::add_completed_file(Hash h)
{
  if (completed_.find(h) == completed_.end()) {
	  auto info = info_manager_.find(h);
	  completed_.insert(make_pair(h, NativeFile(info.path)));
  }
}

void Transport::del_completed_file(Hash h)
{
  downloading_.erase(h);
}

Payload Transport::get_payload()
{
  return payload_;
}
