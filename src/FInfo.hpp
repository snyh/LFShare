#ifndef __FINFO_HPP__
#define __FINFO_HPP__

#include <string>
#include <map>

typedef std::string Hash;
Hash hash_data(const char* data, size_t size);

//FInfo用来描述一个File的信息，以便可以在网络上进行传输.
struct FInfo {
	std::string path;
	Hash hash;
	int chunknum;
	static const size_t chunksize = 65536;
	size_t lastchunksize;
	enum Type { Local, Downloading, Remote} type;

	FInfo()=default;
	FInfo(boost::filesystem::path p, Hash h, int cn, size_t ls)
	  : path(p), hash(h), chunknum(cn), lastchunksize(ls){}
};

class FInfoManager {
public:
	FInfo path2info(path);
	void add_category(const std::string&);
	void add_info(const std::string&, const FInfo&);
	void del_info(const std::string&, const Hash&);
	const FInfo& find_info(const Hash&);
private:
	std::map<std::string, std::map<Hash, FInfo> > infos_;
};

#endif
