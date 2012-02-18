#include <sstream>
#include "FInfo.hpp"
#include "Transport.hpp"
#include <boost/filesystem.hpp>

using namespace std;
using namespace boost;

class ByteReader {
public:
  ByteReader(const char* data, size_t s)
	: pos_(0), data_(data), max_size_(s) {}
  template<typename T>
	T read() {
		T t = *(T*)(data_+pos_);
		pos_ += sizeof(T);
		assert(pos_ <= max_size_);
		//cout << "T:" << t << " size:" << sizeof(T) << "\n";
		return t;
	}
  string str(int size) {
	  string str(data_+pos_, size);
	  pos_ += size;
	  assert(pos_ <= max_size_);
	  return str;
  }
  string str() {
	  size_t s = this->read<size_t>();
	  return str(s);
  }
  const char* data() { return data_ + pos_; }

private:
  int pos_;
  const char* data_;
  size_t max_size_;
};

string hash2str(Hash h);
FInfo info_from_net(const char* data, size_t s)
{
  /*
  千万不要这样来构造INFO, 因为求参顺序是不确定的.
  FInfo info(r.str(16), //file_hash
			 r.str(), //file_path
			 r.read<uint32_t>(), //chunknum
			 r.read<uint32_t>()); //lastchunksize
  */
  ByteReader r(data, s);
  FInfo info;
  info.hash = r.str(16);
  info.path = r.str();
  info.chunknum = r.read<uint32_t>();
  info.lastchunksize = r.read<uint32_t>();
  info.type = FInfo::Remote;
  return info;
}

NetBufPtr info_to_net(FInfo& info)
{
  NetBufPtr buf(new NetBuf);
  buf->add_val("FILEINFO\n");
  buf->add_val(info.hash);

  string name = boost::filesystem::path(info.path).filename().string();
  uint32_t s = name.size();
  buf->add_val(&s, sizeof(uint32_t));
  buf->add_val(name);

  buf->add_val(&(info.chunknum), sizeof(int));
  buf->add_val(&(info.lastchunksize), sizeof(size_t));
  return buf;
}

//Chunk用来包装一块数据(当前默认大小64KB)，可以在网络上进行安全的传输
//可以通过Chunk来组装完整的文件。
//Chunk本身不动态分配内存,因此Chunk体积较小,大概占用40byte的内存(32位系统)，
struct Chunk {
	Chunk(Hash fh, uint32_t i, uint32_t s, const char* d) 
	  : file_hash(fh),
	  chunk_hash(d, s),
	  index(i),
	  size(s),
	  data(d),
	  is_valid_(true) {
	  }
	Chunk(Hash fh, Hash h, uint32_t i, uint32_t s, const char* d) 
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

	bool valid() const { return is_valid_; }
	Hash file_hash;
	Hash chunk_hash;
	uint32_t index;
	uint32_t size;
	const char* data;
private:
	bool is_valid_;
};

Chunk chunk_from_net(const char* data, size_t s)
{
  ByteReader r(data, s);
  Chunk c(r.str(16), //file_hash
		  r.str(16), //chunk_hash
		  r.read<uint32_t>(), //index
		  r.read<uint32_t>(), //size
		  r.data()); //data pointer
  return c;
}
NetBufPtr chunk_to_net(Chunk& c)
{
  NetBufPtr buf(new NetBuf);
  buf->add_val("CHUNK\n");
  buf->add_val(c.file_hash);
  buf->add_val(c.chunk_hash);
  buf->add_val(&(c.index), sizeof(uint32_t));
  buf->add_val(&(c.size), sizeof(uint32_t));
  buf->add_ref(c.data, c.size);
  return buf;
}

struct Bill {
	Hash hash;
	boost::dynamic_bitset<> bits;
};

Bill bill_from_net(const char* data, size_t s)
{
  ByteReader r(data, s);
  Bill bill;
  bill.hash = r.str(16);
  bill.bits = boost::dynamic_bitset<>(r.str());
  return bill;
}

NetBufPtr bill_to_net(Bill& b)
{
  NetBufPtr buf(new NetBuf);
  buf->add_val("BILL\n");
  buf->add_val(b.hash);

  string bits;
  to_string(b.bits, bits);
  size_t s = bits.size();
  buf->add_val(&(s), sizeof(size_t));
  buf->add_val(bits);
  return buf;
}

Transport::Transport(FInfoManager& info_manager)
	:native_(5),
	info_manager_(info_manager),
	ndriver_()
{
  //注册相关消息
  ndriver_.register_plugin(NetDriver::INFO, "FILEINFO", [&](const char*data, size_t s){
						   try {
						   FInfo info = info_from_net(data, s);
						   info_manager_.add_info(info);
						   on_new_file(info);
						   } catch (...) {}
						   });
  ndriver_.register_plugin(NetDriver::DATA, "CHUNK", [&](const char*data, size_t s){
						   handle_chunk(chunk_from_net(data, s));
						   });
  ndriver_.register_plugin(NetDriver::INFO, "BILL", [&](const char*data, size_t s){
						   handle_bill(bill_from_net(data, s));
						   });
  //注册定时器以便统计速度
  ndriver_.start_timer(1, bind(&Transport::record_speed, this));
  //每5秒发送一次本机所需要的文件块
  ndriver_.start_timer(5, bind(&Transport::send_bill, this));

  //发送网络需要的文件块, 优先级非常低
  ndriver_.add_task(0, bind(&Transport::send_chunks, this));
}

void Transport::run() 
{
  ndriver_.run(); 
}

void Transport::handle_bill(const Bill& b)
{
  //如果所请求文件本地拥有完整拷贝则全部添加到global_bill中
  if (completed_.count(b.hash))
	global_bill_[b.hash] |= b.bits;

  //如果所请求文件在下载队列中，则统计拥有的文件块并添加到gloabl_bill中
  if (downloading_.count(b.hash)) 
	global_bill_[b.hash] |= (~local_bill_[b.hash] & b.bits);
}

void Transport::handle_chunk(const Chunk& c)
{
  if (c.valid()) {
	  payload_.global++;

	  //如果是下载队列中的文件
	  if (downloading_.count(c.file_hash)) {

		  //并且是此文件所缺少的文件块
		  auto it = local_bill_.find(c.file_hash);
		  if (it != local_bill_.end() && it->second[c.index]) {
			  //就取消文件块缺失标记
			  it->second[c.index] = false;
			  //并写入文件
			  native_.write(c.file_hash, c.index*FInfo::chunksize, c.data, c.size);

			  //检查是否已经完成
			  check_complete(c.file_hash);

			  //同时记录此文件有效获得一次文件块以便统计下载速度
			  payload_.files[c.file_hash]++;
		  } 
	  }

	  //如果是全局缺少的文件块则取消文件块缺失标记
	  auto it = global_bill_.find(c.file_hash);
	  if (it != global_bill_.end() && it->second[c.index]){
		  it->second[c.index] = false;
	  }
  }
}

void send_chunk_helper(vector<Chunk>& chunks,
					   const Hash& h,
					   boost::dynamic_bitset<>& bits,
					   set<Hash>& files,
					   NativeFileManager& native,
					   map<Hash, boost::dynamic_bitset<> >& bill
					  )
{
  if (files.count(h)) {
	  int length = bits.size();
	  int i;
	  for (i=0; i<length; i++) {
		  if (bits[i]) {
			  size_t size = (i == length) ? : FInfo::chunksize;
			  char* data;
			  native.read(h, i*FInfo::chunksize, data, size);

			  chunks.push_back(Chunk(h, i, size, data));
			  bits[i] = false; //标记已经待发送的文件块
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

void Transport::send_chunks()
{
  //生成不多于5个文件块
  vector<Chunk> chunks;
  for (auto& bill : global_bill_) {
	  send_chunk_helper(chunks, bill.first, bill.second, completed_, native_, global_bill_);
	  if (chunks.size() > 5)
		break;
	  send_chunk_helper(chunks, bill.first, bill.second, downloading_, native_, global_bill_);
  }

  //发送文件块数据到网络
  for (auto& c: chunks) {
	  ndriver_.data_send(chunk_to_net(c));
  }
  ndriver_.add_task(0, bind(&Transport::send_chunks, this));
}

void Transport::send_bill()
{
  for (auto& t: local_bill_) {
	  Bill bill{t.first, t.second};
	  ndriver_.info_send(bill_to_net(bill));
  }
}

//每隔一秒调用一次record_speed函数
void Transport::record_speed()
{
  //更新上一秒记录下的数据
  payload_ = payload_tmp_;
  //清空payload_tmp_以便下次重新计算
  payload_tmp_ = Payload();
}


void Transport::start_receive(const Hash& h)
{
  //如果当前文件不在downloading列表中则添加之
  if (downloading_.count(h) == 0) {
	  auto info = info_manager_.find(h, FInfo::Downloading);
	  native_.new_file(info);
	  downloading_.insert(h);
  }
}
void Transport::stop_receive(const Hash& h)
{
  downloading_.erase(h);
}

void Transport::add_completed_file(const Hash& h)
{
  //如果所添加文件不在已完成列表中则添加
  if (completed_.count(h) == 0) {
	  auto info = info_manager_.find(h, FInfo::Local);
	  native_.new_file(info);
	  completed_.insert(h);

	  //向网络发送此文件信息
	  ndriver_.info_send(info_to_net(info));
  }
}

void Transport::del_completed_file(const Hash& h)
{
  downloading_.erase(h);
}

Payload Transport::get_payload()
{
  return payload_;
}

void Transport::check_complete(const Hash& h)
{

  FInfo info = info_manager_.find(h, FInfo::Downloading);

  //如果所属文件块全部获得则移动文件信息从downloading到local去
  auto bits = local_bill_.find(h)->second;
  if (bits.none()) {
	  local_bill_.erase(h);
	  info_manager_.del_info(h);

	  info.type = FInfo::Local;
	  info_manager_.add_info(info);
  }
  //发送信号告知上层模块有新文件块已写入文件
  double progress = (info.chunknum-bits.count()) / double(info.chunknum);
  on_new_chunk(info.hash, progress);
}


void NativeFileManager::new_file(const FInfo& info)
{
  files_.insert(make_pair(info.hash, info));
}

void NativeFileManager::push_hot(const Hash& h)
{
  using namespace boost::iostreams;
  FInfo& info = files_.find(h)->second;

  if (info.type == FInfo::Local) {
	  hot_.push_front(make_pair(info.hash, mapped_file(info.path, mapped_file::readonly)));
  } else if (info.type == FInfo::Remote) {
	  mapped_file_params param;
	  param.path = info.path;
	  param.flags = mapped_file::readwrite;
	  param.new_file_size = (info.chunknum-1) * FInfo::chunksize + info.lastchunksize;

	  hot_.push_front(make_pair(info.hash, mapped_file(param)));
  } else if (info.type == FInfo::Downloading) {
	  hot_.push_front(make_pair(info.hash, mapped_file(info.path, mapped_file::readwrite)));
  }
}

void NativeFileManager::set_current_file(const Hash& h)
{
  //如果已经在hot列表最前端则直接返回
  if (hot_.front().first == h)
	return;
  //如果hot列表未满则直接添加
  if (hot_.size() < max_num_) {
	  push_hot(h);
	  return;
  }

  //如果此文件在hot列表中则将其位置提前，以便下次可以直接命中
  auto hot_it = hot_.begin();
  for (; hot_it != hot_.end(); hot_it++) {
	  if (hot_it->first == h) {
		  hot_.erase(hot_it);
		  push_hot(h);
	  }
  }

  //如果此文件不在hot列表则将其加入hot中，并将hot中最后的文件关闭
  if (hot_it == hot_.end()) {
	  push_hot(h);
	  hot_.back().second.close();
	  hot_.pop_back();
  }

  //设置当前数据流指向热度最高的文件
  current_= hot_.front().second.data();
}

void NativeFileManager::write(const Hash& h, long begin, const char* src, size_t s)
{
  set_current_file(h);
  memcpy(current_+begin, src, s);
}

void NativeFileManager::read(const Hash& h, long begin, char* dest, size_t s)
{
  set_current_file(h);
  memcpy(dest, current_+begin, s);
}
