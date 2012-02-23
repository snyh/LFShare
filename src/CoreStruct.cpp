#include "CoreStruct.hpp"
#include <openssl/md4.h>

using namespace std;

Hash hash_data(const char* data, size_t size)
{
  char md4[16];
  MD4((const unsigned char*)data, size, (unsigned char*)md4);
  return Hash(md4, 16);
}

class ByteReader {
public:
  ByteReader(const char* data, uint32_t s)
	: pos_(0), data_(data), max_size_(s) {}
  template<typename T>
	T read() {
		T t = *(T*)(data_+pos_);
		pos_ += sizeof(T);
		if (pos_ > max_size_)
		  throw IllegalData();
		return t;
	}
  string str(uint32_t size) {
	  string str(data_+pos_, size);
	  pos_ += size;
	  if (pos_ > max_size_)
		throw IllegalData();
	  return str;
  }
  string str() {
	  uint8_t s = this->read<uint8_t>();
	  return str(s);
  }
  const char* data() { return data_ + pos_; }

private:
  uint32_t pos_;
  const char* data_;
  uint32_t max_size_;
};

FInfo info_from_net(const char* data, size_t s)
{
  /*
  千万不要这样来构造INFO, 因为求参顺序是不确定的.
  FInfo info(r.str(16), //file_hash
			 r.str(), //file_path
			 r.read<uint32_t>(), //chunknum
			 r.read<uint32_t>()); //lastchunksize
  */
  ByteReader r(data, s);
  FInfo info;
  info.file_type = r.read<uint8_t>();

  if (info.file_type != FInfo::RootFile && info.file_type != FInfo::RootDir)
	info.parent_hash = r.str(16);

  info.hash = r.str(16);

  info.chunknum = r.read<uint32_t>();

  info.lastchunksize = r.read<uint16_t>();

  info.path = r.str();

  if (info.lastchunksize > CHUNK_SIZE)
	throw IllegalData();

  info.status = FInfo::Remote;

  return info;
}

SendBufPtr info_to_net(const FInfo& info)
{
  SendBufPtr buf(new SendBuf);
  buf->add_val("FILEINFO\n");

  uint8_t type = info.file_type;
  buf->add_val(&type, sizeof(uint8_t));

  if (info.file_type != FInfo::RootFile && info.file_type != FInfo::RootDir)
	buf->add_val(info.parent_hash);

  buf->add_val(info.hash);

  uint32_t num = info.chunknum;
  buf->add_val(&num, sizeof(uint32_t));

  uint16_t size = info.lastchunksize;
  buf->add_val(&size, sizeof(uint16_t));

  string name = boost::filesystem::path(info.path).filename().string();
  uint8_t name_s = name.size();
  buf->add_val(&name_s, sizeof(uint8_t));
  buf->add_val(name);

  return buf;
}

SendBufPtr chunk_to_net(const Chunk& c)
{
  SendBufPtr buf(new SendBuf);

  buf->add_val(c.file_hash);

  buf->add_val(c.chunk_hash);

  uint32_t index = c.index;
  buf->add_val(&index, sizeof(uint32_t));

  uint16_t size = c.size;
  buf->add_val(&size, sizeof(uint16_t));

  buf->add_ref(c.data, c.size);
  return buf;
}

Chunk chunk_from_net(const char* data, size_t size)
{
  ByteReader r(data, size);
  Chunk c;

  c.file_hash = r.str(16);

  c.chunk_hash = r.str(16);

  c.index = r.read<uint32_t>();

  c.size = r.read<uint16_t>();

  c.data = r.data();

  if (c.size > CHUNK_SIZE)
	throw IllegalData();
  return c;
}



SendBufPtr bill_to_net(const Bill& b)
{
  assert(b.hash.size() == 16);

  SendBufPtr buf(new SendBuf);
  buf->add_val("BILL\n");

  buf->add_val(b.hash);

  uint16_t region = b.region;
  buf->add_val(&region, sizeof(uint16_t));

  BlockType bits = b.bits;
  buf->add_val(&bits, sizeof(BlockType));

  return buf;
}

Bill bill_from_net(const char* data, size_t s)
{
  ByteReader r(data, s);
  Bill bill;
  bill.hash = r.str(16);
  bill.region = r.read<uint16_t>();
  bill.bits = r.read<BlockType>();
  return bill;
}
