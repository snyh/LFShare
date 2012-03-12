#ifndef __CORESTRUCT_HPP__
#define __CORESTRUCT_HPP__
#include "config.hpp"
#include <cstdint>
#include <string>
#include <array>

enum NetMSG : uint8_t {
	FINFO,
	BILL,
	ACK,
	PAYLOAD,
	INIT,
	SENDBEGIN,
	SENDEND,
};

/// 当前使用md4计算hash
typedef std::string  Hash;

/// 计算Hash
Hash hash_data(const char* data, size_t size);

/**
 * @brief Bill用来告诉网络本机所缺少的文件块
 * 考虑到UDP传输的不安全以及Bill并非数据文件因此为了不发生UDP分解发送
 * Bill转换到网络字节时必须小于 MTU-LEN(IP.header)=1500(常见值)-20=1480B
 * 此结构体占用大小16+16+8+32+16=72B, 加上BILL\n的5B远小于1480B.
 */
struct Bill {
  /// 所属文件hash
  Hash hash;

  /// 此Bill所属区域即bits起始位置, 一个区域拥有256块数据因此一共能表达
  //	32 * 2^16 * 60000 = 117.1875GB 大小的文件。
  uint16_t region;

  /// 按位存储文件块是否缺少
  BlockType bits;
};


/**
 * @brief 告诉网络某个文件的信息，以便需要此文件的用户可以通过hash进行下载
 * 		在网络传输时候至少占用80B, 最多占用2144B
 */
struct FInfo {
	enum Type {NormalFile, Directory, RootFile, RootDir };
	enum Status { Local, Downloading, Remote }; 

	/// 表示此文件类型, NormalFile为普通文件, Directory为目录, Root为顶级文件或
	//	目录.	减少传输量使用8位整数来表示此字段
	uint8_t file_type;

	/// 父节点Hash, RootDir类型FInfo不传输此字段
	Hash parent_hash;

	/// 对应文件的hash值用来唯一标示一个文件
	Hash hash;

	/// 对应文件拥有的chunk数量
	uint32_t chunknum;

	/// 对应文件肯定不能完美被CHUNK_SIZE整除，此值保存最后一块chunk的大小
	uint16_t lastchunksize;

	/// 在本机时保存对应文件的物理路径，传递到网络时仅传递文件名
	//	文件名大于256字节时会被自动截断。
	std::string path;


	// 以下成员并不再网络中进行传输

	/// 表示当前FInfo对应状态
	Status status;
};


/**
 * @brief 用来存储文件块信息以便在网络上进行传输
 */
struct Chunk {
	/// 所属文件hash
	Hash file_hash;

	/// 此chunk包含数据的hash, 用来保证chunk的数据正确接受。
	Hash chunk_hash;

	/// 表示当前chunk号，同时能表明当前chunk的大小
	//	(通过对应的FInfo.lastchunksize和FInfo.chunknum来查询)
	uint32_t index;

	/// 考虑到计算chunk_hash时需要知道chunk.size如果使用index字段来查询
	//	size则势必需要在计算chun_hash时涉及到FInfo的信息，造成对处理FInfo模块
	//	部分的依赖。因此增加此字段。
	uint16_t size;

	/// 指向数据真实存储地, 网络传输时占用大小1~CHUNKSIZE(60000) 
	const char* data;

	Chunk() = default;


	Chunk(const Hash& fh, const Hash& ch, uint32_t i, uint16_t s, const char* d):
		file_hash(fh),
		chunk_hash(ch),
		index(i),
		size(s),
		data(d)
	{
	}

	Chunk(const Hash& fh, uint32_t i, uint16_t s, const char* d):
		file_hash(fh), 
		chunk_hash(hash_data(d, s)),
		index(i),
		size(s),
		data(d)
	{
	}
};

struct CKACK {
	uint16_t payload;
	uint8_t loss;
};

#include "BufType.hpp"

FInfo 		info_from_net(const char* data, size_t s);
SendBufPtr 	info_to_net(const FInfo& info);

Chunk 		chunk_from_net(const char* data, size_t size);
SendBufPtr 	chunk_to_net(const Chunk& c);

Bill 		bill_from_net(const char* data, size_t s);
SendBufPtr 	bill_to_net(const Bill& b);

CKACK		ckack_from_net(const char* data, size_t s);
SendBufPtr	ckack_to_net(const CKACK&);

Hash 		sb_from_net(const char* data, size_t s);
SendBufPtr	sb_to_net(const Hash&);

Hash 		se_from_net(const char* data, size_t s);
SendBufPtr	se_to_net(const Hash&);

class InfoNotFound : public std::exception {
};
class InfoExists: public std::exception {
};
class InfoTypeError: public std::exception {
};

class HashInvalid: public std::exception {
public:
  HashInvalid() = default;
  HashInvalid(const std::string& msg): m_(msg) {}
  const char* what() const noexcept { return m_.c_str(); }
  ~HashInvalid() noexcept {}
private:
  std::string m_;
};
class IllegalData : public std::exception {
public:
  IllegalData(const std::string& msg): m_(msg) {}
  const char* what() const noexcept { return m_.c_str(); }
  ~IllegalData() noexcept {}
private:
  std::string m_;
};

#endif
