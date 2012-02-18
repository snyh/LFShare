#include "FileManager.hpp"
using namespace std;
namespace pl = std::placeholders;

FileManager::FileManager() 
	 :info_manager_(),
	 transport_(info_manager_)
{
  transport_.on_new_chunk.connect(bind(&FileManager::cb_new_chunk, this, pl::_1, pl::_2));
  transport_.on_new_file.connect(bind(&FileManager::cb_new_file, this, pl::_1));
}

void FileManager::add_local_file(const string& path)
{
  FInfo info = info_manager_.add_info(path);
  transport_.add_completed_file(info.hash);
}

void FileManager::remove(const Hash& h)
{
  info_manager_.del_info(h);
}

vector<FInfo> FileManager::current_list()
{
  return info_manager_.list();
}

void FileManager::start_download(const Hash& h)
{
  transport_.start_receive(h);
}

void FileManager::stop_download(const Hash& h)
{
  transport_.stop_receive(h);
}


void FileManager::cb_new_file(const FInfo& info)
{
  msg_.new_files.push_back(info);
}

void FileManager::cb_new_chunk(const Hash& h, double progress)
{
  msg_.progress[h] = progress;
}

NewMsg FileManager::refresh() 
{ 
  NewMsg old = msg_;
  old.payload = transport_.get_payload();
  msg_ = NewMsg();
  return old;
}

void FileManager::network_start()
{
  transport_.run();
}
