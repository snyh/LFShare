const int ONE_NUM = 128;
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
	  cout << "***************Send Time Out!*************" << endl;
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
	pb_(0),
	pe_(0),
	ppr_(0),
	tp_(t)
{
  // bits初始化时默认值为0
  bits_.flip();
}

void RecvHelper::send_bill()
{
  if (bits_.none())
	return;

  is_work_ = true;

  Bill bill;
  bill.hash = hash_;

  bitset<BLOCK_LEN> bit;

  if (ppr_ == 0)
	tp_.send_sb(hash_);

  int counter = 0;

  uint32_t cknum = bits_.size();
  uint16_t rgnum = cknum / BLOCK_LEN;
  uint8_t  lslen = cknum % BLOCK_LEN;

  for (; ppr_ < rgnum; ppr_++) {
	  bill.region = ppr_;
	  uint32_t base = ppr_* BLOCK_LEN;
	  bit.reset();
	  for (uint8_t i=0; i < BLOCK_LEN; i++, pe_++) {
		  bit[i] = bits_[base+i];

		  if (bits_[base+i])
			counter++;
		  if (counter == ONE_NUM/2)
			pm_ = pe_;
		  else if (counter > ONE_NUM)
			break;
	  }
	  if (bit.any()) {
		  bill.bits = bit.to_ulong();
		  tp_.send_bill(bill);
	  }
	  if (counter == ONE_NUM/2)
		pm_ = pe_;
	  else if (counter > ONE_NUM)
		break;
  }
  if (lslen != 0 && counter < ONE_NUM) {
	  bill.region = rgnum;
	  bit.reset();
	  for (uint8_t i=0; i< lslen; i++, pe_++) {
		  uint32_t base = rgnum * BLOCK_LEN;
		  bit[i] = bits_[base+i];
		  if (counter == ONE_NUM/2)
			pm_ = pe_;
		  else if (counter > ONE_NUM)
			break;
		  //if (bit[i]) cout << "Request Chunk: " << (base+i) << endl;
	  }
	  bill.bits = bit.to_ulong();
	  tp_.send_bill(bill);
  }
}

void RecvHelper::send_ckack()
{
  is_work_ = true;

  CKACK ack;

  int counter = 0;
  for (uint32_t i=pb_; i<pm_; i++) {
	  if (bits_.test(i))
		counter++;
  }

  pb_ = pm_; 
  pm_ = pe_;

  ack.loss = counter;
  assert(ack.loss >= 0);

  ack.payload = tp_.payload().global;

  tp_.send_ckack(ack);
  send_bill();
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

  if (index >= pm_ )
	send_ckack(); //send_ckack后pm_就变成pe_了所以这里不会发送过的的CKACK

  return v;
}

uint32_t RecvHelper::count()
{
  return bits_.count();
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
	  cout << "***************Recv Time Out!*************" << endl;
  }
  is_work_ = false;
}

void RecvHelper::receive_se()
{
  if (bits_.any()) {
	  ppr_ = 0;
	  pb_ = pe_ = pm_ =0;
	  send_bill();
  }
}
