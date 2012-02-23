#ifndef _TRANSPOR_HPP__
#define _TRANSPOR_HPP__
#include "pre.hpp"
//#include <boost/dynamic_bitset.hpp>
#include <bitset>
#include "CoreStruct.hpp"
#include "NetDriver.hpp"
#include "NativeFile.hpp"
class FInfoManager;

struct Payload {
	int global;
	std::map<Hash, int> files;
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
	/// 发送指定文件的某块chunk
	void send_chunk(const Hash& fh, uint32_t index, uint16_t size);

	/// 发送global_bill中所缺少的chunk
	void send_all_chunk();

	/// 发送local_bill中所缺少的bill
	void send_all_bill();

	/// 发送指定文件所缺少的bill
	void send_bill(const Hash& fh);

	/**
	 * @brief 从NetDriver处获得CHUNK数据时触发此函数，
	 *
	 * @param buf 动态分配的空间用来保存chunk数据，是一个shared_ptr
	 * @param s 数据大小
	 */
	void handle_chunk(RecvBufPtr buf, size_t s);


	void handle_bill(const Bill& b);
	void handle_info(const FInfo& info);

	void check_complete(const Hash& h);

	/// 用来统计速度
	void record_speed();

	/// 用来保存上一秒的结果
	Payload payload_;
	/// 用来统计当前这一秒的信息
	Payload payload_tmp_;

	/// 记录本地所缺少的文件块
	std::map<Hash, std::vector<std::bitset<BLOCK_LEN>>> local_bill_;

	/// 拥有完整拷贝的文件
	std::set<Hash> complete_;

	/// 正在下载队列中的文件
	std::set<Hash> incomplete_;

	NativeFileManager native_;
	FInfoManager& info_manager_;
	NetDriver ndriver_;
};

#endif
