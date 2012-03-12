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
		tp_.send_chunk(hash_, base+i);
	  }
  }

  if (pcr_ == end_region_+1) {
	  state_ = END;
	  tp_.send_se(hash_);
  }

  FLog("deal bill:%1% current_pcr: %2%") % b.region % pcr_;
  for (uint8_t i=0; i< BLOCK_LEN; i++) {
	  if (bit[i])
		cout << base+i << ";";
  }
  cout << endl;
}

RecvHelper::RecvHelper(Transport& t, const Hash& h, uint32_t max_i)
:is_work_(false),
	is_stop_(false),
	hash_(h),
	bits_(max_i),
	pb_(0),
	pm_(0),
	ppr_(0),
	threshold(128),
	tp_(t)
{
  // bits初始化时默认值为0
  bits_.flip();
  start();
}

bool RecvHelper::send_bill()
{
  bool is_send = false;
  if (bits_.none())
	return is_send;

  is_work_ = true;

  Bill bill;
  bill.hash = hash_;


  if (ppr_ == 0) {
	tp_.send_sb(hash_);
  }

  int counter = 0;

  uint32_t cknum = bits_.size();
  uint16_t rgnum = cknum / BLOCK_LEN;
  uint8_t  lslen = cknum % BLOCK_LEN;

  bitset<BLOCK_LEN> bit;
  for (; ppr_ < rgnum; ppr_++) {
	  bill.region = ppr_;
	  uint32_t base = ppr_* BLOCK_LEN;
	  bit.reset();
	  for (uint8_t i=0; i < BLOCK_LEN; i++) {
		  assert(base+i <= bits_.size());
		  bit[i] = bits_[base+i];
		  if (bits_[base+i]) counter++;

		  if (counter < threshold/2) pm_++; 
		  if (counter == threshold) break;
	  }
	  if (bit.any()) {
		  bill.bits = bit.to_ulong();
		  tp_.send_bill(bill);

		  is_send = true;
		  FLog("pb: %1%\tpm:%2%\t") % pb_ % pm_;
	  }
	  if (counter == threshold) break;
  }
  if (lslen != 0 && counter < threshold) {
	  bill.region = rgnum;
	  bit.reset();
	  for (uint8_t i=0; i< lslen; i++) {
		  uint32_t base = rgnum * BLOCK_LEN;
		  bit[i] = bits_[base+i];
		  if (bits_[base+i]) counter++;
		  if (counter < threshold/2) pm_++; 
		  if (counter == threshold) break;
	  }
	  bill.bits = bit.to_ulong();
	  tp_.send_bill(bill);

	  is_send = true;
	  FLog("pb: %1%\tpm:%2%\t") % pb_ % pm_;
  }
  return is_send;
}

void RecvHelper::send_ckack()
{
  is_work_ = true;

  CKACK ack;

  int counter = 0;
  for (uint32_t i=pb_; i<=pm_; i++) {
	  if (bits_.test(i))
		counter++;
  }
  pb_ = pm_+1;

  ack.loss = counter;
  assert(ack.loss >= 0);

  ack.payload = tp_.payload().global;

  if (send_bill()) {
	tp_.send_ckack(ack);
	FLog("Send_CKACK: %1%") % pm_;
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

  if (index >= pm_)
	send_ckack(); 

  return v;
}

uint32_t RecvHelper::count()
{
  return bits_.count();
}


void RecvHelper::timeout()
{
  if (bits_.any() && is_work_ == false && !is_stop_) {
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

	  FLog("%1%") % "***************Recv Time Out!*************";
	  for (uint32_t i=0; i<bits_.size(); i++) {
		  if (bits_[i]) cout << i << ";";
	  }
	  cout << endl;
  }
  is_work_ = false;
}

void RecvHelper::receive_se()
{
  if (bits_.any()) {
	  ppr_ = 0;
	  pb_ =  pm_ =0;
	  send_bill();
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
