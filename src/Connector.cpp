#include "Connector.hpp"
#include "NetDriver.hpp"
#include "MFileManager.hpp"

using namespace std;
namespace pl = std::placeholders;

Connector::Connector() 
{
	this->info_send.connect([](SendBuffer& buf){
		theND().info_send(buf.value());
		});

	this->data_send.connect([](SendBuffer& buf){
		//theND.data_send<SendBuffer::SequenceType>(buf.value());
		});
	theND().data_receive.connect(bind(&Connector::data_receive, this,
									pl::_1, pl::_2));
	theND().info_receive.connect(bind(&Connector::info_receive, this,
									pl::_1, pl::_2));
}

void Connector::data_receive(const char* data, size_t s)
{
  if (s <= Plugin::KeyMaxSize) return;
  int pos = -1;
  pos = find(data, data+Plugin::KeyMaxSize, '\n') - data;
  if (pos <= 0) return;

  string key(data, pos);
  for(auto it = cb_data_map_.find(key); it != cb_data_map_.end(); it++) {
	  it->second(data+pos+1, s-pos-1);
  }
}
void Connector::info_receive(const char* data, size_t s)
{
  if (s <= Plugin::KeyMaxSize) return;
  int pos = -1;
  pos = find(data, data+Plugin::KeyMaxSize, '\n') - data;
  if (pos <= 0) return;

  string key(data, pos);
  for(auto it = cb_info_map_.find(key); it != cb_info_map_.end(); it++) {
	  cout << "info_receive " << s << endl;
	  it->second(data+pos+1, s-pos-1);
  }
}
void Connector::register_plugin(Plugin& p)
{
  if (p.type == Plugin::INFO)
    this->cb_info_map_.insert(make_pair(p.key, p.on_receive));
  else
    this->cb_data_map_.insert(make_pair(p.key, p.on_receive));
}
