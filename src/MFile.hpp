#ifndef __MFILE_HPP__
#define __MFILE_HPP__

#include <string>
#include <vector>
#include <boost/filesystem.hpp>
#include <boost/iostreams/device/mapped_file.hpp>

#include "Bill.hpp"

typedef std::string Hash;

//MChunk用来包装一块数据(当前默认大小64KB)，可以在网络上进行安全的传输
//可以通过MChunk来组装完整到文件。
//MChunk本身不动态分配内存,因此MChunk体积较小,大概占用40byte的内存(32位系统)，
struct MChunk {
    MChunk(int index, Hash file_hash, size_t size, const char* data);
    MChunk(int index, Hash file_hash, size_t size, const char* data, Hash hash);
    bool valid() const { return is_valid_; }
    ~MChunk();

    Hash file_hash;
    int size;
    const char* data;
    int index;
private:
    bool is_valid_;
    Hash chunk_hash_;
};

//MFInfo用来描述一个MFile的信息，以便可以在网络上进行传输.
struct MFInfo {
    boost::filesystem::path path;
	Hash hash;
	int chunknum;
	size_t chunksize;
	size_t lastchunksize;

	MFInfo()=default;
	MFInfo(boost::filesystem::path p, Hash h, int cn, size_t cs, size_t ls)
	  : path(p), hash(h), chunknum(cn), chunksize(cs), lastchunksize(ls){}

	template <class Archive>
	  void serialize(Archive& ar, const unsigned int version) {
		  std::string s;
		  if (Archive::is_saving::value)
			s = this->path.string();
		  ar & boost::serialization::make_nvp("string", s);
		  if (Archive::is_loading::value)
			this->path = s;

		  ar & hash & chunknum & chunksize & lastchunksize;
	  }
};

class MFile {
public:
    enum State { full=0, lack, init};
    //通过网络数据创建MFile
    MFile(boost::filesystem::path save_path, MFInfo& info);
    //通过本地文件创建MFile(如果发现cfg文件则根据cfg内容恢复MFile状态)
    MFile(boost::filesystem::path path, size_t chunksize=65536);

	//用来关闭打开的mapped文件
    ~MFile();

	const Hash& hash() { return info_.hash; }
	const std::string name() { return boost::filesystem::basename(info_.path); }
	double size() {
		uintmax_t t = (info_.chunknum-1)*info_.chunksize + info_.lastchunksize;
		return t/(1024*1024.0);
	}
	State state() const {return state_;}


	bool valid() { return is_valid_; }

    bool fill_chunk(MChunk c);
    MChunk fetch_chunk(int index);

    Bill produce_bill();

	const MFInfo& get_info() const {return info_;} 

private:
	MFInfo info_;
    State state_;

    boost::iostreams::mapped_file mapped_;
    boost::iostreams::mapped_file cfg_;
    std::vector<MChunk> chunks_;
	int filled_number;
	void clear_cfg();
	bool is_valid_;
};

#endif
