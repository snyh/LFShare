using namespace std;
#include "NetDriver.hpp"
#include "AppWebServer/server.hpp"
#include <thread>

JRPC::Service& rpc_filemanager();
JRPC::Service& rpc_systeminfo();


int main()
{
  std::thread t1(&NetDriver::run, &theND());

  JRPC::Server rpc;
  rpc.install_service(rpc_filemanager());
  rpc.install_service(rpc_systeminfo());

  Server s;
  s.set_rpc(rpc);
  s.run();

  t1.join();
  return 0;
}

