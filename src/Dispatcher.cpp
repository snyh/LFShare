#include "Dispatcher.hpp"
using namespace std;
namespace pl = std::placeholders;

Dispatcher::Dispatcher() 
	 :info_manager_(),
	 transport_(info_manager_)
{
  transport_.on_new_chunk.connect(bind(&Dispatcher::cb_new_chunk, this, pl::_1, pl::_2));
  transport_.on_new_file.connect(bind(&Dispatcher::cb_new_file, this, pl::_1));
}

void Dispatcher::add_local_file(const string& path)
{
  FInfo info = info_manager_.add_info(path);
  cb_new_file(info);
}

void Dispatcher::remove(const Hash& h)
{
  FInfo info = info_manager_.del_info(h);
}

vector<FInfo> Dispatcher::current_list()
{
  return info_manager_.list();
}

void Dispatcher::start_download(const Hash& h)
{
  transport_.start_receive(h);

  // 必须在开始之后再进行改变状态。因为如果h指向的文件是
  // remote类型则需要用特殊的方式打开文件
  // 基本意思就是必须开始下载之后才能改变文件的状态
  info_manager_.modify_status(h, FInfo::Downloading);
}

void Dispatcher::stop_download(const Hash& h)
{
  transport_.stop_receive(h);
}


void Dispatcher::cb_new_file(const FInfo& info)
{
  assert("newFile");
  msg_.new_files.push_back(info);
}

void Dispatcher::cb_new_chunk(const Hash& h, double progress)
{
  msg_.progress[h] = progress;
}

NewMsg Dispatcher::refresh() 
{ 
  NewMsg old = msg_;
  old.payload = transport_.payload();
  msg_ = NewMsg();
  return old;
}

void Dispatcher::network_start()
{
  transport_.run();
}
void Dispatcher::native_start()
{
  transport_.native_run();
}

string Dispatcher::chunk_info(const Hash& h)
{
  return transport_.get_chunk_info(h);
}
