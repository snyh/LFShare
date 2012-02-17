#include "FileManager.hpp"
using namespace std;

FileManager::FileManager() 
	 :info_manager_(),
	 transport_(info_manager_)
{
}

void FileManager::upload(const string& path)
{
  //transport_.add_completed_file(hash);
}

void FileManager::remove(const Hash& h)
{
  //tmp_.remove(h);
}

vector<FInfo> FileManager::current_list()
{
  vector<FInfo> infos;
  return infos;
}

void FileManager::download(const Hash& h)
{
  transport_.start_receive(h);
}

void FileManager::stop(const Hash& h)
{
  transport_.stop_receive(h);
}
void FileManager::on_complete(const Hash& h)
{
  //tmps_.remove(h);
}
