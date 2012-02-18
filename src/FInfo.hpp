#ifndef __FINFO_HPP__
#define __FINFO_HPP__

#include <string>
#include <map>
#include <vector>

class InfoNotFound : public std::exception {
};
class InfoExists: public std::exception {
};
class InfoTypeError: public std::exception {
};

typedef std::string Hash;
Hash hash_data(const char* data, size_t size);

//FInfo用来描述一个File的信息，以便可以在网络上进行传输.
struct FInfo {
	std::string path;
	Hash hash;
	uint32_t chunknum;
	static const uint32_t chunksize = 65536;
	uint32_t lastchunksize;
	enum Type { Local, Downloading, Remote} type;

	FInfo()=default;
	FInfo(const std::string& p, Hash h, int cn, uint32_t ls)
	  : path(p), hash(h), chunknum(cn), lastchunksize(ls){}
};

class FInfoManager {
public:
	/**
	 * @brief add_info 向local_插入一条数据
	 *
	 * @param path
	 * @exception 如果path所指向的文件件存在则抛出InfoExists异常
	 * @return 生成的FInfo信息
	 */
	FInfo add_info(const std::string& path);

	/**
	 * @brief add_info 
	 *
	 * @exception 如果此Info类型错误则抛出InfoTypeError异常
	 * @param FInfo
	 */
	void add_info(const FInfo&);

	void del_info(const Hash&);

	/**
	 * @brief find 
	 *
	 * @exception 如果没有找到则抛出InfoNotFound异常
	 *
	 * @return 
	 */
	const FInfo& find(const Hash& h);
	const FInfo& find(const Hash& h, FInfo::Type);
	std::vector<FInfo> list();
	std::vector<FInfo> list(FInfo::Type);
private:
	std::map<Hash, FInfo> local_;
	std::map<Hash, FInfo> remote_;
	std::map<Hash, FInfo> downloading_;
};

#endif
