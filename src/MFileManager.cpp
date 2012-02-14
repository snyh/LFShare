#include "MFileManager.hpp"
#include "Connector.hpp"
#include <sstream>
#include <string>
using namespace std;

MFileManager::MFileManager()
{
  this->install_fileinfo_plugin();
  this->install_chunks_plugin();
  //this->install_bill_plugin();
}

void MFileManager::install_fileinfo_plugin()
{
  Plugin fileinfo;
  fileinfo.key = "FILEINFO";
  fileinfo.type = Plugin::INFO;
  fileinfo.on_receive = [this](const char* data, size_t s){
	  MFInfo info = from_string<MFInfo>(string(data,s));
	  //files_.insert(make_pair(info.hash,
	  //this->add_file_to_local(from_string<MFInfo>(string(data,s)));
  };

  theConnector().register_plugin(fileinfo);
}

void MFileManager::upload(std::string path)
{
  MFile file(path);
  files_.insert(make_pair(file.hash(), file));

  this->buf_ = "FILEINFO\n" + to_string(file.get_info());
  Connector::SendBuffer buf;
  buf.add(buf_.data(), buf_.size());
  theConnector().info_send(buf);
}


void MFileManager::install_chunks_plugin()
{
  Plugin pchunk;
  pchunk.key = "CHUNKS";
  pchunk.type = Plugin::DATA;
  pchunk.on_receive = [this](const char* data, size_t s){
	  //TODO:
  };
  theConnector().register_plugin(pchunk);
}
/* 
void MFileManager::fill_file(MChunk& c)
{
}

void MFileManager::install_bill_plugin()
{
}
void MFileManager::ask_file()
{
}
*/

