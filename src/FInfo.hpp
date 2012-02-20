#ifndef __FINFO_HPP__
#define __FINFO_HPP__

#include <string>
#include <map>
#include <vector>
#include <boost/signals2/signal.hpp>

class InfoNotFound : public std::exception {
};
class InfoExists: public std::exception {
};
class InfoTypeError: public std::exception {
};

typedef std::string Hash;
Hash hash_data(const char* data, size_t size);
class HashInvalid: public std::exception {
public:
  	HashInvalid() = default;
	HashInvalid(const std::string& msg): m_(msg) {}
	const char* what() const noexcept { return m_.c_str(); }
	~HashInvalid() noexcept {}
private:
	std::string m_;
};

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

	/**
	 * @brief del_info 
	 *
	 * @param Hash
	 *
	 * @exception 如果未找到文件则抛出InfoNotFound异常
	 *
	 * @return 返回被删除的FInfo
	 */
	FInfo del_info(const Hash&);

	/**
	 * @brief find 
	 *
	 * @exception 如果没有找到则抛出InfoNotFound异常
	 *
	 * @return 
	 */
	const FInfo& find(const Hash& h);
	std::vector<FInfo> list();

	boost::signals2::signal<void(const FInfo&)> on_new_info;
	boost::signals2::signal<void(const Hash&)> on_del_info;
private:
	std::map<Hash, FInfo> all_info_;
};

#endif
