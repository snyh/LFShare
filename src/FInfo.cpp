#include "FInfo.hpp"
#include <ios>
#include <openssl/md4.h>
#include <boost/filesystem.hpp>
#include <boost/iostreams/device/mapped_file.hpp>

using namespace std;
using namespace boost;

Hash hash_data(const char* data, size_t size)
{
  char md4[16];
  MD4((const unsigned char*)data, size, (unsigned char*)md4);
  Hash hash(md4, 16);
  return hash;
}

FInfo FInfoManager::path2info(const std::string& path)
{
	Hash hash;
	int chunknum;
	size_t lastchunksize;
	uintmax_t filesize;

	filesize = filesystem::file_size(path);
	iostream::mapped_file file(path, ios_base::out);

	hash = hash_data(file.data(), filesize);

	chunknum = filesize / FInfo::chunksize;

	lastchunksize = filesize % FInfo::chunksize;
	chunknum += lastchunksize > 0 ? 1: 0;

	MFInfo info(path, hash, chunknum, lastchunksize);
	return info;
}

void FInfoManager::add_category(const std::string& c)
{
  infos_.insert(make_pair(c, map<Hash, FInfo>()));
}

void FInfoManager::add_info(const std::string& c, const FInfo& f)
{
  infos_[c].insert(make_pair(f.hash, f));
}

void FInfoManager::del_info(const std::string& c, const Hash& h)
{
  infos_[c].remove(h);
}

const FInfo& FInfoManager::find_info(const std::string& c, const Hash& h)
{
  return infos_[c].find(h)->second;
}
