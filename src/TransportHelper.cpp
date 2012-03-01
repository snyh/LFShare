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
		tp_.send_chunk(hash_, base+i);
	  }
  }

  if (pcr_ == end_region_+1) {
	  state_ = END;
	  tp_.send_se(hash_);
  }
}

RecvHelper::RecvHelper(Transport& t, const Hash& h, uint32_t max_i)
:is_work_(false),
	hash_(h),
	bits_(max_i),
	tp_(t)
{
  // bits初始化时默认值为0
  bits_.flip();
}

void RecvHelper::begin_receive()
{
  if (bits_.none())
	return;

  is_work_ = true;

  Bill bill;
  bill.hash = hash_;

  bitset<BLOCK_LEN> bit;

  tp_.send_sb(hash_);
  assert(bits_.size() > 0);

  uint32_t cknum = bits_.size();
  uint16_t rgnum = cknum / BLOCK_LEN;
  uint8_t  lslen = cknum % BLOCK_LEN;
  for (uint16_t region=0; region<rgnum; region++) {
	  bill.region = region;
	  uint32_t base = region * BLOCK_LEN;
	  bit.reset();
	  for (uint8_t i=0; i < BLOCK_LEN; i++) {
		  bit[i] = bits_[base+i];
		  //if (bit[i]) cout << "Request Chunk: " << (base+i) << endl;
	  }
	  if (bit.any()) {
		  bill.bits = bit.to_ulong();
		  tp_.send_bill(bill);
	  }
  }
  if (lslen != 0) {
	  bill.region = rgnum;
	  bit.reset();
	  for (uint8_t i=0; i< lslen; i++) {
		  uint32_t base = rgnum * BLOCK_LEN;
		  bit[i] = bits_[base+i];
		  //if (bit[i]) cout << "Request Chunk: " << (base+i) << endl;
	  }
	  bill.bits = bit.to_ulong();
	  tp_.send_bill(bill);
  }
}

bool RecvHelper::ack(uint32_t index)
{
  if (index >= bits_.size() || bits_.none())
	return false;

  is_work_ = true;

  // 如果缺少对应块返回true, 否则返回false;
  bool v = bits_.test(index);

  // 如果缺少对应的文件块, 则关闭对应位
  if (v)
	bits_.reset(index);

  return v;
}

uint32_t RecvHelper::count()
{
  return bits_.count();
}
CKACK RecvHelper::gen_ckack(uint16_t region)
{
  is_work_ = true;

  CKACK ack;
  uint32_t base = region * BLOCK_LEN;
  ack.bill.hash = hash_;
  ack.bill.region = region;

  uint8_t len = bits_.size() - region*BLOCK_LEN;
  if (len > BLOCK_LEN) len = BLOCK_LEN;

  bitset<BLOCK_LEN> bit;
  for (uint8_t i=0; i<len; i++) {
	  bit[i] = bits_[base+i];
  }
  ack.bill.bits = bit.to_ulong();

  ack.payload = tp_.payload().global;
  return ack;
}


void RecvHelper::timeout()
{
  if (bits_.any() && is_work_ == false) {
	  //尽量减少超时重传的数据量, 这里只发送使发送端进入END状态的
	  //最后一区域数据
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
  }
  is_work_ = false;
}

void RecvHelper::receive_se()
{
  if (bits_.any())
	begin_receive();
}
