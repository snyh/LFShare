#include "MFile.hpp"
#include <openssl/md4.h>
#include <vector>
#include <stdio.h>
#include <fstream>

using namespace std;
using namespace boost::filesystem;
using namespace boost::iostreams;
boost::system::error_code ec;

inline
void write_index(char* data, long position, char value)
{
    data[16+sizeof(size_t)*2+position-1] = value;
}
bool has_chunk(char* data, long position)
{
    char value = data[16+sizeof(size_t)*2+position-1];
    if (value == 1)
        return true;
    else if (value == 0)
        return false;
    else
        return false; //something's wrong!!!
}
Hash hash_data(const char* data, size_t size)
{
  char md4[16];
  MD4((const unsigned char*)data, size, (unsigned char*)md4);
  Hash hash(md4, 16);
  return hash;
}


//通过传递进来到参数建立MFInfo并且建立cfg和mapped的映射
MFile::MFile(boost::filesystem::path base, MFInfo& info)
  : info_(info)
{
    this->state_ = MFile::init;
	boost::filesystem::path path = (base/=info_.path);
	info_.path = path.string();

    mapped_file_params parm;
    parm.mode = ios_base::out | ios_base::in;
    parm.offset = 0;
    //建立具体文件到内存的映射
    parm.path = path.native();
    parm.length = info_.lastchunksize + (info_.chunknum-1)*info_.chunksize;
    parm.new_file_size = parm.length;
    this->mapped_.open(parm);
    //建立临时文件的隐射
    parm.path = path.native() + ".cfg";
    //Hash code 16byte + chunksize 4byte + lstchksize 4byte + chunknum Nbyte
    parm.length = 16 + sizeof(info_.chunksize) + sizeof(info_.lastchunksize) 
	  + info_.chunknum;
    parm.new_file_size = parm.length;
    this->cfg_.open(parm);
    //写入基本信息
    char* data = this->cfg_.data();
    memset(data, 0, parm.length);
    memcpy(data, info_.hash.c_str(), 16);
    memcpy(data+16, (char*)&info_.chunksize, sizeof(info_.chunksize));
    memcpy(data+16+sizeof(info_.chunksize), (char*)&info_.lastchunksize, 
		   sizeof(info_.lastchunksize));

	this->filled_number = 0;
}

//通过传递进来到参数建立MFInfo和mapped到映射。
//此构造函数建立到是MFile::full类型已经是完整到文件因此不需要
//临时配置文件cfg的建立。
MFile::MFile(boost::filesystem::path path, size_t chunksize)
	 :is_valid_(false)
{
	if (!exists(path)) {
		//TODO:fix this
		cerr << "File doesn't Exists!\n";
		is_valid_ = false;
	}

	Hash hash;
	int chunknum;
	size_t lastchunksize;
	uintmax_t filesize;
	mapped_file_params parm;

	parm.path = path.native();
	parm.length = file_size(path);
	parm.mode = ios_base::out | ios_base::in;
	parm.offset = 0;

	this->mapped_.open(parm);

	boost::filesystem::path cfg_path = path.string() + ".cfg";
	if (exists(cfg_path, ec)) {
		this->state_ = MFile::lack;

		parm.path = cfg_path.native();
		parm.length = file_size(cfg_path);
		this->cfg_.open(parm);

		char *data = this->cfg_.data();
		hash = Hash(data, 16);
		chunksize = *(size_t*)(data+16);
		lastchunksize = *(size_t*)(data+16+sizeof(size_t));
		chunknum = filesize - 16 - 2*sizeof(size_t);

		this->filled_number = 0;
		data = data+16+2*sizeof(size_t);
		for (int i=0; i<chunknum; i++) {
			if (data[i] == 1)
			  filled_number++;
		}
		cout << "Continuely download, has completed " << 
		  filled_number / double(chunknum) << endl;
	} else {
		this->state_ = MFile::full;

		filesize = file_size(path);

		hash = hash_data(this->mapped_.data(), filesize);
		chunknum = filesize / chunksize;
		lastchunksize = filesize % chunksize;
		chunknum += lastchunksize > 0 ? 1: 0;
		this->filled_number = chunknum;
	}
	this->info_ = MFInfo(path, hash, chunknum, chunksize, lastchunksize);
	this->is_valid_ = true;
}

//提取指定index上的文件块
MChunk MFile::fetch_chunk(int index)
{
  assert(index > 0);
  assert(index <= this->info_.chunknum);
  size_t s=0;
  intmax_t offset = this->info_.chunksize * (index-1);
  if (index == this->info_.chunknum && this->info_.lastchunksize > 0) {
	  s = this->info_.lastchunksize;
  } else {
	  s = this->info_.chunksize;
  }

  const char* data = this->mapped_.data();
  MChunk m(index, this->info_.hash, s, data+offset);
  return m;
}

//将c的数据填充到真实文件上
bool MFile::fill_chunk(MChunk c)
{
  if (this->state_ == MFile::full)
	return true;
  //如果此chunk无效则失败返回
  if (!c.valid())
	return false;
  //不属于此文件的chunk直接丢弃
  if (this->info_.hash != c.file_hash)
	return false;
  //如果此chunk已经存在则直接成功返回
  if (has_chunk(this->cfg_.data(), c.index)) {
	  cout << "Chunk " << c.index << "is already in!\n";
	  return true;
  }
  //计算chunk在真实文件中的起始位置
  long begin = this->info_.chunksize * (c.index-1);
  char* data = this->mapped_.data();
  memcpy(data+begin, c.data, c.size);
  write_index(this->cfg_.data(), c.index, 1);

  this->filled_number++;
  if (this->filled_number == info_.chunknum)
	this->clear_cfg();

  return true;
}

void MFile::clear_cfg()
{
  if (this->cfg_.is_open())
	this->cfg_.close();

  path cfg_path(this->info_.path.string() + ".cfg");
  remove(cfg_path, ec);
  this->state_ = MFile::full;
  cout << "Has transimit completely!\n";
}

//建立当前文件缺少部分到清单
Bill MFile::produce_bill()
{
  boost::dynamic_bitset<> set(this->info_.chunknum);
  char* value = this->cfg_.data() + 16+sizeof(size_t)*2;
  for (int i=0; i<this->info_.chunknum; i++) {
	  set[i] = value[i];
  }
  return Bill(this->info_.hash, set);
}

MFile::~MFile()
{
  if (this->mapped_.is_open())
	this->mapped_.close();
  if (this->cfg_.is_open())
	this->cfg_.close();
}

MChunk::MChunk(int index, Hash file_hash, size_t size, const char *data, Hash hash)
{
  //TODO
  this->index = index;
}

MChunk::MChunk(int index, Hash file_hash, size_t size, const char* data)
{
  this->index = index;
  this->size = size;
  this->data = data;
  this->chunk_hash_ = hash_data(data, size);
  this->file_hash = file_hash;
  this->is_valid_ = true;
}

MChunk::~MChunk()
{
}
