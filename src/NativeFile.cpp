#include "NativeFile.hpp"
using namespace std;

NativeFileManager::NativeFileManager(int n): 
	current_(nullptr), 
	max_num_(n),
	io_(),
	io_work_(io_) 
{
}
void NativeFileManager::new_file(const FInfo& info)
{
  files_.insert(make_pair(info.hash, info));
  if (hot_.empty()) {
	  push_hot(info.hash);
  }
}

void NativeFileManager::push_hot(const Hash& h)
{
  using namespace boost::iostreams;
  FInfo& info = files_.find(h)->second;

  if (info.type == FInfo::Local) {
	  hot_.push_front(make_pair(info.hash, mapped_file(info.path, mapped_file::readwrite)));
  } else if (info.type == FInfo::Remote) {
	  mapped_file_params param;
	  param.path = info.path;
	  param.flags = mapped_file::readwrite;
	  param.new_file_size = (info.chunknum-1) * FInfo::chunksize + info.lastchunksize;

	  hot_.push_front(make_pair(info.hash, mapped_file(param)));
  } else if (info.type == FInfo::Downloading) {
	  hot_.push_front(make_pair(info.hash, mapped_file(info.path, mapped_file::readwrite)));
  }

  //设置当前数据流指向热度最高的文件
  current_= hot_.front().second.data();
}

void NativeFileManager::set_current_file(const Hash& h)
{
  assert(hot_.size() > 0);
  assert(hot_.size() <= max_num_);

  //如果已经在hot列表最前端则直接返回
  if (hot_.front().first == h) {
	return;
  }
  //如果hot列表未满则直接添加
  if (hot_.size() < max_num_) {
	  push_hot(h);
	  return;
  }

  //如果此文件在hot列表中则将其位置提前，以便下次可以直接命中
  auto hot_it = hot_.begin();
  for (; hot_it != hot_.end(); hot_it++) {
	  if (hot_it->first == h) {
		  hot_.erase(hot_it);
		  push_hot(h);
	  }
  }

  //如果此文件不在hot列表则将其加入hot中，并将hot中最后的文件关闭
  if (hot_it == hot_.end()) {
	  push_hot(h);
	  hot_.back().second.close();
	  hot_.pop_back();
  }
}

void NativeFileManager::run()
{
  io_.run();
}

void NativeFileManager::write(const Hash& h, long begin, const char* src, size_t s, RecvBufPtr _ignore)
{
  io_.post([=](){
		   set_current_file(h);
		   memcpy(current_+begin, src, s);
		   });
}

void NativeFileManager::read(const Hash& h, long begin, char* dest, size_t s)
{
  assert(current_ != nullptr);
  set_current_file(h);
  memcpy(dest, current_+begin, s);
}
