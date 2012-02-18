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

FInfo FInfoManager::add_info(const std::string& path)
{
	Hash hash;
	int chunknum;
	size_t lastchunksize;
	uintmax_t filesize;

	filesize = filesystem::file_size(path);
	iostreams::mapped_file file(path, ios_base::out);

	hash = hash_data(file.data(), filesize);

	chunknum = filesize / FInfo::chunksize;

	lastchunksize = filesize % FInfo::chunksize;
	chunknum += lastchunksize > 0 ? 1: 0;

	FInfo info(path, hash, chunknum, lastchunksize);
	info.type = FInfo::Local;
	local_.insert(make_pair(info.hash, info));
}


void FInfoManager::add_info(const FInfo& f)
{
  if (f.type == FInfo::Local)
	local_.insert(make_pair(f.hash, f));
  else if (f.type == FInfo::Downloading)
	downloading_.insert(make_pair(f.hash, f));
  else if (f.type == FInfo::Remote)
	remote_.insert(make_pair(f.hash, f));
}


void FInfoManager::del_info(const Hash& h)
{
  local_.erase(h);
  downloading_.erase(h);
  remote_.erase(h);
}

const FInfo& FInfoManager::find(const Hash& h)
{
  auto it = local_.find(h);
  if (it != local_.end())
	return it->second;

  it = downloading_.find(h);
  if (it != local_.end())
	return it->second;

  it = remote_.find(h);
  if (it != local_.end())
	return it->second;
}

const FInfo& FInfoManager::find(const Hash& h, FInfo::Type type)
{
  std::map<Hash, FInfo>::iterator it;
  if (type == FInfo::Local)
	it = local_.find(h);
  else if (type == FInfo::Downloading)
	it = downloading_.find(h);
  else if (type == FInfo::Remote)
	it = remote_.find(h);

  if (it != map<Hash, FInfo>::iterator())
	return it->second;
}

std::vector<FInfo> FInfoManager::list()
{
  vector<FInfo> v;
  for (auto& t: local_) {
	  v.push_back(t.second);
  }
  for (auto& t: downloading_) {
	  v.push_back(t.second);
  }
  for (auto& t: remote_) {
	  v.push_back(t.second);
  }
  return v;
}

std::vector<FInfo> FInfoManager::list(FInfo::Type type)
{
  vector<FInfo> v;
  if (type == FInfo::Local) {
	  for (auto& t: local_)
		v.push_back(t.second);
  } else if (type == FInfo::Downloading) {
	  for (auto& t: downloading_)
		v.push_back(t.second);
  } else if (type == FInfo::Remote) {
	  for (auto& t: remote_)
		v.push_back(t.second);
  }
  return v;
}
