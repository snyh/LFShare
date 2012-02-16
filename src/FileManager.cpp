#include "FileManager.hpp"

FileManager::FileManager() 
{
}

FileManager::get_info(Hash h)
{
}

FInfo FileManager::upload(std::string path)
{
  port_.add_local_file(hash);
}

void FileManager::remove(Hash h)
{
  tmp_.remove(h);
}

vector<FInfo> FileManager::infos()
{
  vector<FInfo> infos;
  return infos;
}

void FileManager::download(Hash h)
{
  port_.begin_receive(h);
}

void FileManager::stop(Hash h)
{
  port_.stop_receive(h);
}
void FileManager::on_complete(Hash h)
{
  //tmps_.remove(h);
}
