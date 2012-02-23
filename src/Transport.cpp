#include "Transport.hpp"
#include "FInfoManager.hpp"

using namespace std;
namespace pl = std::placeholders;


Transport::Transport(FInfoManager& info_manager)
	:native_(5),
	info_manager_(info_manager),
	ndriver_()
{
  //注册相关消息
  ndriver_.register_cmd_plugin("FILEINFO", 
								[&](const char*data, size_t s){
								try { handle_info(info_from_net(data, s)); }
								catch (IllegalData&) {assert(!"IllegalData");}
								});
  ndriver_.register_cmd_plugin("BILL",
								[&](const char*data, size_t s){
								try {handle_bill(bill_from_net(data, s));}
								catch(IllegalData&) {assert(!"IllegalData");}
								});

  ndriver_.register_data_plugin([&](RecvBufPtr buf, size_t s){handle_chunk(buf, s);});

  //注册定时器以便统计速度
  ndriver_.start_timer(1, std::bind(&Transport::record_speed, this));
  //每5秒发送一次本机所需要的文件块(在收到需要的chunk时也会立即发送余下需要的
  //bill到网络中)
  ndriver_.start_timer(5, std::bind(&Transport::send_all_bill, this));

  info_manager_.on_new_info.connect(std::bind(&Transport::cb_new_info, this, pl::_1));
  info_manager_.on_del_info.connect(bind(&Transport::cb_del_info, this, pl::_1));
}

void Transport::cb_new_info(const FInfo& f)
{
  if (f.status == FInfo::Local) {
	  complete_.insert(f.hash);
	  native_.new_file(f);

	  //向网络发送此文件信息
	  ndriver_.cmd_send(info_to_net(f));
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
void Transport::native_run() 
{
  native_.run();
}


/***************************接受信息函数**********************/

void Transport::handle_info(const FInfo& info)
{
  try {
	  info_manager_.add_info(info);
	  on_new_file(info);
  } catch (InfoExists&) {
	  cout << "文件已存在:" <<  info.path << endl;
  }
}


void Transport::handle_bill(const Bill& b)
{
  FInfo info = info_manager_.find(b.hash);
  uint32_t bit_offset = b.region*BLOCK_LEN;
  uint32_t last_index = info.chunknum - 1;
  assert(b.region <= info.chunknum/BLOCK_LEN);

  // 如果所属region是最后一个则有效长度为chunknum%BLOCK_LEN, 否则BLOCK_LEN为
  uint8_t len = (info.chunknum/BLOCK_LEN == b.region) ? (info.chunknum%BLOCK_LEN) : BLOCK_LEN;

  if (complete_.count(b.hash)) { 		//如果所请求文件本地拥有完整拷贝
	  for (uint8_t i=0; i<len; i++) { //则全部发送
		  uint32_t index = bit_offset+i; 
		  uint16_t chunk_size = (index == last_index) ? info.lastchunksize : CHUNK_SIZE;
		  send_chunk(b.hash, index, chunk_size);
	  }
  } else if (incomplete_.count(b.hash)) {//如果所请求文件在下载队列中
	  cout << "HUHU1" << endl;
	  cout << "b.regin: " << b.region << "num: " << local_bill_.at(b.hash).size() << endl;
	  auto& bits = local_bill_.at(b.hash).at(b.region);
	  cout << "HUHU2" << endl;

	  for (uint8_t i=0; i<len; i++) {
		  uint32_t index = bit_offset+i; 
		  if (!bits[i]) {			//且本地拥有某文件块
			  uint16_t chunk_size = (index == last_index) ? info.lastchunksize : CHUNK_SIZE;
			  send_chunk(b.hash, index, chunk_size);  //则发送
		  }
	  }
	  cout << "HUHU3" << endl;
  }
}

void Transport::handle_chunk(RecvBufPtr buf, size_t s)
{
  try {
	  //尝试转换数据为Chunk
	  char* data = buf->data()+6; // 6 是 CHUNK\n 的长度
	  Chunk c = chunk_from_net(data, s);
	  //TODO: 检查c.index是否在info.chunknum之内

	  if (incomplete_.count(c.file_hash)) {	 //如果是下载队列中的文件
		  //全局速度+1
		  payload_.global++;

		  uint16_t region = c.index / BLOCK_LEN;
		  uint8_t offset = c.index % BLOCK_LEN;

		  //并且是此文件所缺少的文件块
		  auto it = local_bill_.find(c.file_hash);
		  if (it != local_bill_.end() && it->second[region][offset]) {
			  //就取消文件块缺失标记
			  it->second[region][offset] = false;
			  //并写入文件
			  native_.async_write(c.file_hash, 
								  c.index*CHUNK_SIZE,
								  c.data, 
								  c.size,
								  [=](){buf;});

			  //检查是否已经完成
			  check_complete(c.file_hash);

			  //同时记录此文件有效获得一次文件块以便统计此文件的下载速度
			  payload_.files[c.file_hash]++;

			  //继续发送Bill以便其他节点可以马上继续发送文件块
			  ndriver_.add_task(90, std::bind(&Transport::send_bill, this, std::cref(c.file_hash)));
		  } 
	  }
  } catch(IllegalData&) {
	  assert(!"IllegalData");
	  return;
  }
}

/***************************发送信息函数**********************/

void Transport::send_bill(const Hash& h)
{
  int counter = 0;
  vector<bitset<BLOCK_LEN>>& bits = local_bill_[h];
  int num_region = bits.size();
  for (int i=0; i<num_region; i++) {
	  if (bits[i].any()) {
		  Bill bill;
		  bill.hash = h;
		  bill.region = i;
		  bill.bits = bits[i].to_ulong();
		  ndriver_.cmd_send(bill_to_net(bill));
		  if (counter++ > 10)
			break;
	  }
  }
}

void Transport::send_all_bill()
{
  // 如果下载队列为空则直接返回
  if (incomplete_.empty())
	return;

  // local_bill.size() >= incomplete_.size()  因此使用local_bill作为外层循环
  for (auto& t: local_bill_) {
	  if (incomplete_.count(t.first) != 0) { //如果此file在下载队列中则继续
		  send_bill(t.first);
	  }
  }
}

void Transport::send_chunk(const Hash& fh, uint32_t index, uint16_t size)
{
  char* data;
  native_.async_read(fh, index*CHUNK_SIZE, data, size,
					 [=](){ ndriver_.data_send(chunk_to_net(Chunk(fh, index, size, data)));}
					);
}


/**********************其他函数*****************/

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
	  //新开始下载的文件将信息提供给native_
	  FInfo info = info_manager_.find(h);
	  native_.new_file(info);

	  // 生成local_bill信息
	  vector<std::bitset<BLOCK_LEN>> vb;
	  uint16_t num_region = info.chunknum / BLOCK_LEN + 1;
	  const uintmax_t mask = -1;
	  for (int i=0; i<num_region-1; i++) {
		  std::bitset<BLOCK_LEN> b(mask);
		  vb.push_back(b);
	  }
	  uint8_t last_len = info.chunknum % BLOCK_LEN;
	  std::bitset<BLOCK_LEN> b(mask & mask << (BLOCK_LEN-last_len));
	  vb.push_back(b);
	  cout << "num region: " << vb.size() << endl;
	  cout << "chunknum: " << info.chunknum << endl;

	  local_bill_.insert(make_pair(h, vb));
  }

  // 加入下载列表
  incomplete_.insert(h);

  // 向网络发送请求文件块消息
  send_bill(h);
}

void Transport::stop_receive(const Hash& h)
{
  incomplete_.erase(h);
}


Payload Transport::payload()
{
  return payload_;
}

void Transport::check_complete(const Hash& h)
{

  FInfo info = info_manager_.find(h);

  uint32_t count = 0;
  //如果所属文件块全部获得则移动文件信息从downloading到local去
  auto bits = local_bill_.find(h)->second;
  for (auto& i : bits) {
	  count += i.count();
  }
  if (count == 0) {
	  local_bill_.erase(h);
	  info_manager_.del_info(h);

	  info.status = FInfo::Local;
	  info_manager_.add_info(info);
  }
  //发送信号告知上层模块有新文件块已写入文件
  double progress = (info.chunknum-count) / double(info.chunknum);
  on_new_chunk(info.hash, progress);
}


string Transport::get_chunk_info(const Hash& h)
{
  auto it = local_bill_.find(h);
  if (it != local_bill_.end()) {
	  string r;
	  for (auto& i : it->second) {
		  r.append(i.to_string());
	  }
	  return r;
  }

  throw InfoNotFound();
}
