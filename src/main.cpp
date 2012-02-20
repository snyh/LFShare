#include "FileManager.hpp"
#include "UI/AppWebServer/server.hpp"
#include <boost/thread.hpp>

using namespace std;

JRPC::Service& rpc_filemanager(FileManager&);
JRPC::Service& rpc_systeminfo();


int main()
{
  FileManager filemanager;
  boost::thread t1(bind(&FileManager::network_start, &filemanager));
  boost::thread t2(bind(&FileManager::native_start, &filemanager));

  JRPC::Server rpc;
  rpc.install_service(rpc_filemanager(filemanager));
  rpc.install_service(rpc_systeminfo());

  Server s;
  s.set_rpc(rpc);
  s.run();

  t1.join();
  t2.join();
  return 0;
}

