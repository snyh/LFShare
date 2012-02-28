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
	  is_work_ = true;
  }
}

void SendHelper::deal_bill(const Bill& b) 
{
  if (state_ == END)
	return;

  if (pcr_ < b.region)
	pcr_ = b.region;
  else
	return;

  is_work_ = true;

  for (int i=b.region*BLOCK_LEN; i<BLOCK_LEN; i++) {
	  if (i < max_index_)
		tp_.send_chunk(hash_, i);
  }

  if (pcr_ == end_region_) {
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
  if (bits_.size() == 0)
	return;

  is_work_ = true;

  Bill bill;
  bill.hash = hash_;

  bitset<BLOCK_LEN> bit;

  tp_.send_sb(hash_);
  uint16_t second_lastest_region = bits_.size() / BLOCK_LEN - 1;
  for (uint16_t region=0; region<second_lastest_region; region++) {
	  bill.region = region;
	  uint32_t base = region*BLOCK_LEN;
	  for (uint8_t i=0; i<BLOCK_LEN; i++) {
		  bit[i] = bits_[base+i];
	  }
	  if (bit.any()) {
		  bill.bits = bit.to_ulong();
		  tp_.send_bill(bill);
	  }
  }
  //发送最后区域是否有需要发送的文件块都要发送
  bill.region = second_lastest_region + 1;
  uint32_t base = second_lastest_region*BLOCK_LEN;
  uint8_t len = bill.region*BLOCK_LEN - bits_.size();
  for (uint8_t i=0; i<len; i++) {
	  bit[i] = bits_[base+i];
  }
  tp_.send_bill(bill);

}

bool RecvHelper::ack(uint32_t index)
{
  if (index > bits_.size())
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
  if (bits_.size() != 0 && is_work_ == false)
	begin_receive();

  is_work_ = false;
}

void RecvHelper::receive_se()
{
  if (bits_.size() != 0)
	begin_receive();
}
