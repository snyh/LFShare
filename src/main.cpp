#include "FileManager.hpp"
#include "UI/AppWebServer/server.hpp"
#include <thread>

using namespace std;

JRPC::Service& rpc_filemanager(FileManager&);
JRPC::Service& rpc_systeminfo();


int main()
{
  FileManager filemanager;
  std::thread t1(&FileManager::network_start, &filemanager);

  JRPC::Server rpc;
  rpc.install_service(rpc_filemanager(filemanager));
  rpc.install_service(rpc_systeminfo());

  Server s;
  s.set_rpc(rpc);
  s.run();

  t1.join();
  return 0;
}

