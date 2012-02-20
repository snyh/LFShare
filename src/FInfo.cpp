#include "FInfo.hpp"
#include <ios>
#include <openssl/md4.h>
//#include <cryptopp/md4.h>
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

FInfo FInfoManager::add_info(const std::string& path)
{
	Hash hash;
	int32_t chunknum;
	int32_t lastchunksize;
	uintmax_t filesize;

	filesize = filesystem::file_size(path);
	iostreams::mapped_file file(path, ios_base::out);

	hash = hash_data(file.data(), filesize);

	try {
		find(hash);
		throw InfoExists();
	} catch (InfoNotFound&) {
		chunknum = filesize / FInfo::chunksize;

		lastchunksize = filesize % FInfo::chunksize;
		chunknum += lastchunksize > 0 ? 1: 0;

		FInfo info(path, hash, chunknum, lastchunksize);
		info.type = FInfo::Local;
		this->add_info(info);
		return info;
	}
}


void FInfoManager::add_info(const FInfo& f)
{
  if (all_info_.count(f.hash) != 0)
	throw InfoExists();

  all_info_.insert(make_pair(f.hash, f));

  on_new_info(f);
}


FInfo FInfoManager::del_info(const Hash& h)
{
  auto it = all_info_.find(h);
  if (it == all_info_.end())
	throw InfoNotFound();

  FInfo tmp = it->second;
  all_info_.erase(it);

  on_del_info(h);
  return tmp;
}

const FInfo& FInfoManager::find(const Hash& h)
{
  auto it = all_info_.find(h);
  if (it == all_info_.end())
	throw InfoNotFound();

  return it->second;
}

std::vector<FInfo> FInfoManager::list()
{
  vector<FInfo> v;
  for (auto& t: all_info_) {
	  v.push_back(t.second);
  }
  return v;
}
