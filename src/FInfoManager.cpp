#include "FInfoManager.hpp"
#include <boost/iostreams/device/mapped_file.hpp>
using namespace std;
using namespace boost;

FInfo FInfoManager::add_info(const std::string& p)
{
  //TODO: 增加目录支持
	Hash hash;
	int32_t chunknum;
	int32_t lastchunksize;
	uintmax_t filesize;

	boost::filesystem::path path(to_ucs2(p));

	filesize = filesystem::file_size(path);
	iostreams::mapped_file file(path);

	hash = hash_data(file.data(), filesize);
	file.close();

	try {
		find(hash);
		throw InfoExists();
	} catch (InfoNotFound&) {
		chunknum = filesize / CHUNK_SIZE; 

		lastchunksize = filesize % CHUNK_SIZE;
		chunknum += lastchunksize > 0 ? 1: 0;

		FInfo info;
		info.file_type = FInfo::RootFile;
		info.hash = hash;
		info.chunknum = chunknum;
		info.path = p;
		info.lastchunksize = lastchunksize;
		info.status = FInfo::Local;

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
  if (it == all_info_.end()) {
	throw InfoNotFound();
  }

  FInfo tmp = it->second;
  all_info_.erase(it);

  on_del_info(h);
  return tmp;
}

const FInfo& FInfoManager::find(const Hash& h)
{
  auto it = all_info_.find(h);
  if (it == all_info_.end()) {
	throw InfoNotFound();
  }

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
void FInfoManager::modify_status(const Hash& h, FInfo::Status s)
{
  auto it = all_info_.find(h);
  if (it == all_info_.end()) {
	throw InfoNotFound();
  }
  it->second.status = s;
}
