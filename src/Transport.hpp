#ifndef _TRANSPOR_HPP__
#define _TRANSPOR_HPP__
#include "pre.hpp"
#include <boost/dynamic_bitset.hpp>
#include <bitset>
#include "CoreStruct.hpp"
#include "NetDriver.hpp"
#include "NativeFile.hpp"
class FInfoManager;

struct Payload {
	int global;
	std::map<Hash, int> files;
};

class Transport;

class SendHelper {
public:
  SendHelper(Transport& t, const Hash& h, uint32_t max_i);
  void receive_sb();
  void deal_bill(const Bill& b);
  /// 每隔一秒会自动调用此函数，若发现SendHelper在非END状态下切未发任何Chunk则自
  //动转换为END状态且发送SENDEND信息以便其他节点可以尽快获知.
  void timeout();
private:
  bool is_work_;
  Hash hash_;
  uint16_t pcr_;
  uint16_t end_region_;
  enum State {
	  BEGIN,
	  END,
  } state_;
  Transport& tp_;
  uint32_t max_index_;
};
class RecordPos {
public:
  	RecordPos(boost::dynamic_bitset<>&);
	std::vector<uint32_t> init();
	std::vector<uint32_t> next();
	bool is_milestone(uint32_t index);
	void stop();
	int get_loss();
	boost::dynamic_bitset<>& bits_;
private:
	int loss_;
	bool is_end_;
	uint32_t pb;
	uint32_t pe;
	uint32_t pm;
};

class RecvHelper {
public:
  RecvHelper(Transport& t, const Hash& h, uint32_t max_i);
  RecvHelper(const RecvHelper& old);
  void start();
  void pause();
  void begin_send();
  /// 若缺少此文件块则返回True并标记为不缺
  bool ack(uint32_t index);
  void receive_se();
  uint32_t count();

  /// 每隔一秒会自动调用此函数,若发现RecvHelper没有收到任何此文件的任何Chunk则启
  //动超时重传机制
  void timeout();
private:
  bool send_bill(const std::vector<uint32_t>&);
  void send_end_bill();
  void send_ckack();
  bool is_work_;
  bool is_stop_;
  Hash hash_;
  boost::dynamic_bitset<> bits_;
  RecordPos pos_;
  Transport& tp_;
};

class Transport {
public:
  	Transport(FInfoManager& info_manager);

	/// 开始或者继续某个文件的下载
	void start_receive(const Hash& file_hash);

	/// 暂停下载
	void stop_receive(const Hash& file_hash);

	/// 系统获得新FInfo时候会调用此函数，用来添加元素到complte队列
	void cb_new_info(const FInfo& h);
	/// 系统删除某个FInfo时会调用此函数，用来更新incomplte, complete队列
	void cb_del_info(const Hash& h);


	std::string get_chunk_info(const Hash&);

	void run();
	void native_run();

	/// 获得上一秒的网络整体负载和下载队列中文件的速度
	Payload payload();

	/// 从网络收到一个本地还没有的FInfo时会触发此信号，以便上层逻辑进行处理
	boost::signals2::signal<void(const FInfo&)> on_new_file;

	/// 接收到有效文件块时触发此信号，第一个参数表示文件hash, 第二个参数表示此文
	//	件的进度
	boost::signals2::signal<void(const Hash&, double)> on_new_chunk;


private:
	friend class SendHelper;
	friend class RecvHelper;

	/// 发送指定文件的某块chunk
	void send_chunk(const Hash& fh, uint32_t index);

	/// 发送已经构造好的Bill
	void send_bill(const Bill&);

	void send_sb(const Hash& fh);

	void send_se(const Hash& fh);

	void send_ckack(const CKACK& ack);

	///从NetDriver处获得CHUNK数据时触发此函数，
	void handle_chunk(RecvBufPtr buf, size_t s);

	void handle_info(const FInfo& info);

	void handle_bill(const Bill&);

	void handle_ckack(const Bill&);

	void handle_ckack(const CKACK& ack);

	void handle_sb(const Hash& b);

	void handle_se(const Hash& b);

	void sendqueue_new(const Hash& h);

	void write_chunk(const Chunk&, std::function<void()>);

	void check_complete(const Hash& h);

	/// 检查是SendHelper或RecvHelper是否停止工作
	void check_timeout();

	/// 用来统计速度
	void record_speed();

	/// 用来保存上一秒的结果
	Payload payload_;
	/// 用来统计当前这一秒的信息
	Payload payload_tmp_;

	/// 发包间隔,根据网络丢包率动态计算
	int interval_;
	bool is_recv_ack_;

	NativeFileManager native_;
	FInfoManager& info_manager_;
	NetDriver ndriver_;
	std::map<Hash, SendHelper> sendqueue_;
	std::map<Hash, RecvHelper> recvqueue_;
};

#endif
