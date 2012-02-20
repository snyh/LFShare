#include <sstream>
#include "FInfo.hpp"
#include "Transport.hpp"
#include <boost/filesystem.hpp>
#include <functional>

using namespace std;
namespace pl = std::placeholders;

#include "transport_help.icc"

Transport::Transport(FInfoManager& info_manager)
	:native_(5),
	info_manager_(info_manager),
	ndriver_()
{
  //注册相关消息
  ndriver_.register_plugin(NetDriver::INFO, "FILEINFO", [&](const char*data, size_t s){
						   handle_info(info_from_net(data, s));
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

  info_manager_.on_new_info.connect(std::bind(&Transport::cb_new_info, this, pl::_1));
  info_manager_.on_del_info.connect(bind(&Transport::cb_del_info, this, pl::_1));
}

void Transport::cb_new_info(const FInfo& f)
{
  if (f.type == FInfo::Local) {
	complete_.insert(f.hash);
	native_.new_file(f);

	//向网络发送此文件信息
	ndriver_.info_send(info_to_net(f));
  }
}

void Transport::cb_del_info(const Hash& h)
{
  complete_.erase(h);
  incomplete_.erase(h);
}

void Transport::run() 
{
  ndriver_.run(); 
}

void Transport::handle_info(const FInfo& info)
{
  try {
	  info_manager_.add_info(info);
	  cout << "发现新文件:"<<info.path << endl;
	  on_new_file(info);
  } catch (InfoExists&) {
	  cout << "文件已存在:" <<  info.path << endl;
  }
}

void Transport::send_bill()
{
  //TODO:一次是否发送太多文件块请求？
  for (auto& t: local_bill_) {
	  Bill bill{t.first, t.second};
	  //cout << "开始发送:" << hash2str(t.first) << endl;
	  cout << "--------:" << t.second << endl;
	  ndriver_.info_send(bill_to_net(bill));
  }
}
void Transport::handle_bill(const Bill& b)
{
  //如果所请求文件本地拥有完整拷贝则全部添加到global_bill中
  if (complete_.count(b.hash)) {
	  global_bill_[b.hash] |= b.bits;
	  //发送网络需要的文件块, 优先级非常低
	  ndriver_.add_task(0, bind(&Transport::send_chunks, this));
  }

  //如果所请求文件在下载队列中
  if (incomplete_.count(b.hash)) {
	  //tmp保存的是b.bits所缺少而本地拥有的文件块位。
	  auto tmp = ~local_bill_[b.hash] & b.bits;
	  cout << "所请求块: " << b.bits << endl;
	  cout << "本地拥有: " << ~local_bill_[b.hash] << endl;
	  cout << "综合可发送: " << tmp << endl;
	  //只有拥有对方所缺少的文件块则进行标记
	  if (tmp.any()) {
		  global_bill_[b.hash] |= tmp;
		  //并在空闲时发送文件块, 优先级非常低
		  ndriver_.add_task(0, bind(&Transport::send_chunks, this));
	  }
  }
}


void Transport::handle_chunk(const Chunk& c)
{
  if (c.valid()) {
	  payload_.global++;

	  //如果是下载队列中的文件
	  if (incomplete_.count(c.file_hash)) {

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

			  //继续发送Bill以便其他节点可以马上继续发送文件块
			  ndriver_.add_task(100, bind(&Transport::send_bill, this));
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
	  send_chunk_helper(chunks, bill.first, bill.second, complete_, native_, global_bill_);
	  if (chunks.size() > 5)
		break;
	  send_chunk_helper(chunks, bill.first, bill.second, incomplete_, native_, global_bill_);
  }

  //发送文件块数据到网络
  for (auto& c: chunks) {
	  ndriver_.data_send(chunk_to_net(c));
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
  auto it = local_bill_.find(h);
  if (it == local_bill_.end()) {
	  //新开始下载的文件
	  FInfo info = info_manager_.find(h);
	  native_.new_file(info);
	  incomplete_.insert(h);
	  boost::dynamic_bitset<> bits(info.chunknum);
	  bits.flip();
	  local_bill_.insert(make_pair(h, bits));
  } else {
	  //继续下载的文件
	  incomplete_.insert(h);
  }
  send_bill();
}
void Transport::stop_receive(const Hash& h)
{
  incomplete_.erase(h);
  //TODO bill?
}


Payload Transport::payload()
{
  return payload_;
}

void Transport::check_complete(const Hash& h)
{

  FInfo info = info_manager_.find(h);

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
