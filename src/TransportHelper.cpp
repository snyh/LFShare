#include "Transport.hpp"
#include "tools.hpp"

using namespace std;
namespace pl = std::placeholders;
const int THRESHOLD = 64;
int num_send_chunk = 0; //测试使用的变量

vector<Bill> vec2bill(const vector<uint32_t>& vec)
{
  vector<Bill> bs;
  Bill bill;
  bitset<BLOCK_LEN> bit;
  int p_region = vec[0] / BLOCK_LEN;
  int region;
  int offset = 0;
  for(uint32_t i=0; i<vec.size(); i++){
	  region = vec[i] / BLOCK_LEN;
	  if (region != p_region) {
		  bill.region = p_region;
		  bill.bits = bit.to_ulong();
		  bs.push_back(bill);

		  p_region = region;
		  bit.reset();
	  }
	  offset = vec[i] % BLOCK_LEN;
	  bit[offset] = true;
  }
  if (region == p_region) {
	  bill.region = region;
	  bill.bits = bit.to_ulong();
	  bs.push_back(bill);
  }
  return bs;
}



RecvHelper::RecvHelper(Transport& t, const Hash& h, uint32_t max_i)
	:is_work_(false),
	is_stop_(false),
	hash_(h),
	bits_(max_i),
	pos_(bits_),
	tp_(t)
{
  // bits初始化时默认值为0
  bits_.flip();
}
RecvHelper::RecvHelper(const RecvHelper& old) 
	 : is_work_(old.is_work_),
	   is_stop_(old.is_stop_),
	   hash_(old.hash_),
	   bits_(old.bits_),
	   pos_(bits_),
	   tp_(old.tp_)
{
  // pos_需要使用当前bits_进行初始化,不能简单的引用之前的old.bits_
}

void RecvHelper::begin_send()
{
  tp_.send_sb(hash_);
  send_bill(pos_.init());
}

bool RecvHelper::send_bill(const vector<uint32_t>& vecs)
{
  // is_send 用来表示此函数是否有发送数据(如果没有数据需要发送则不会发送)
  bool is_send = false;
  if (vecs.empty())
	return is_send;

  is_work_ = true;

  for (auto& b : vec2bill(vecs)) {
	  b.hash = hash_;
	  tp_.send_bill(b);
	  is_send = true;
  }

  return is_send;
}

void RecvHelper::send_ckack()
{
  is_work_ = true;

  CKACK ack;
  ack.loss = pos_.get_loss();
  assert(ack.loss >= 0);

  ack.payload = tp_.payload().global;

  vector<uint32_t> b = pos_.next();
  if (b.size() > 0) { //如果还有账单需要发送则
	tp_.send_ckack(ack);  //发送ckack
	send_bill(b); // 和下一部分账单
	// 注意: ckack和bill发送顺序不要颠倒
  }
}

bool RecvHelper::ack(uint32_t index)
{
  if (index >= bits_.size() || bits_.none())
	return false;
  is_work_ = true;


  // 如果缺少对应块返回true, 否则返回false;
  bool v = bits_.test(index);

  // 关闭对应位
  bits_.reset(index);

  // 如果到达临界值则发送ckack
  if (pos_.is_milestone(index))
	send_ckack(); 

  return v;
}

uint32_t RecvHelper::count()
{
  return bits_.count();
}

void RecvHelper::send_end_bill()
{
  uint16_t region = bits_.size() / BLOCK_LEN;
  uint8_t len = bits_.size() % BLOCK_LEN;
  uint32_t base = region*BLOCK_LEN;
  bitset<BLOCK_LEN> bit;
  for (uint8_t i=0; i<len; i++) {
	  bit[i] = bits_[base+i];
  }
  Bill bill;
  bill.hash = hash_;
  bill.region = region;
  bill.bits = bit.to_ulong();
  tp_.send_bill(bill);

  begin_send();
}

void RecvHelper::timeout()
{
  if (bits_.any() && is_work_ == false && !is_stop_) {
	  //尽量减少超时重传的数据量, 这里只发送使发送端进入END状态的
	  //最后一区域数据
	  send_end_bill();

	  tp_.send_ckack(CKACK{0, THRESHOLD});  //发送ckack

	  FLog("%1%") % "***************Recv Time Out!*************";
  }
  is_work_ = false;
}

void RecvHelper::receive_se()
{
  if (bits_.any()) {
	  tp_.send_sb(hash_);
	  send_bill(pos_.init());
  }
}

void RecvHelper::start()
{
  is_stop_ = false;
}

void RecvHelper::pause()
{
  is_stop_ = true;
}

RecordPos::RecordPos(boost::dynamic_bitset<>& b)
	 : bits_(b),
	   is_end_(true)
{
}

vector<uint32_t> RecordPos::init()
{
  pb = pm = pe = 0;
  int counter = 0;
  vector<uint32_t> pos;
  for (; pm < bits_.size(); pm++) {
	  if (bits_.test(pm)) {
		pos.push_back(pm);
		counter++;
		if (counter >= THRESHOLD)
		  break;
	  }
  }
  pe = pm+1;
  counter = 0;
  for (; pe < bits_.size(); pe++) {
	  if (bits_.test(pe)) {
		pos.push_back(pe);
		counter++;
		if (counter >= THRESHOLD)
		  break;
	  }
  }
  cout <<"POS: ";
  for (auto i : pos) {
	  cout << i << ",";
  }
  cout << endl;
  if (pos.size())
	is_end_ = false;
  else
	is_end_ = true;
  return pos;
}
void RecordPos::stop()
{
  is_end_ = true;
}

vector<uint32_t> RecordPos::next()
{
  if (is_end_)
	return vector<uint32_t>();
  int counter = 0;
  vector<uint32_t> pos;
  for (; pe < bits_.size(); pe++) {
	  if (bits_.test(pe)) {
		  pos.push_back(pe);
		  counter++;
		  if (counter >= THRESHOLD)
			break;
	  }
  }
  if (pos.size())
	is_end_ = false;
  else
	is_end_ = true;
  return pos;
}
bool RecordPos::is_milestone(uint32_t index) 
{
  if (index > pm) {
	  for (; pb < pm; pb++) {
		  if (bits_[pb])
			loss_++;
	  }
	  pb = pm+1;
	  pm = pe+1;
	  pe = pe+1;
	  if (index >= pe)
		pe = index+1;
	  loss_ = 0; 
	  return true;
  } else {
	return false;
  }
}

int RecordPos::get_loss()
{
  return loss_;
}

SendHelper::SendHelper(Transport& t, const Hash& h, uint32_t max_i)
:is_work_(false),
	hash_(h), 
	pcr_(0),
	max_index_(max_i),
	end_region_(max_i/BLOCK_LEN),
	state_(END),
	tp_(t)
{
}

void SendHelper::timeout()
{
  if (state_ != END && is_work_ == false) {
	  state_ = END;
	  tp_.send_se(hash_);
	  FLog("%1%") % "***************Send Time Out!*************";
  }

  is_work_ = false;
}

void SendHelper::receive_sb() 
{
  if (state_ == END) {
	  state_ = BEGIN;
	  pcr_ = 0;
	  is_work_ = true;
  }
}

void SendHelper::deal_bill(const Bill& b) 
{
  static int ss = 0;
  bitset<BLOCK_LEN> bit(b.bits);

  if (state_ == END)
	return;

  if (pcr_ <= b.region && b.region <= end_region_)
	pcr_ = b.region + 1;
  else
	return;

  is_work_ = true;

  uint32_t base = b.region*BLOCK_LEN;
  for (int i=0; i<BLOCK_LEN; i++) {
	  if (base+i < max_index_ && bit[i]) {
		  //FLog("************num of send:%1%") % num_send_chunk++;
		  tp_.send_chunk(hash_, base+i);
	  }
  }

  FLog("deal bill:%1% current_pcr: %2%") % b.region % pcr_;
  if (pcr_ == end_region_+1) {
	  state_ = END;
	  tp_.send_se(hash_);
  }

}
