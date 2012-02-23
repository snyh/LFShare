#include "Dispatcher.hpp"
#include "UI/AppWebServer/server.hpp"
#include <boost/thread.hpp>

using namespace std;

JRPC::Service& rpc_dispatcher(Dispatcher&);
JRPC::Service& rpc_systeminfo();


int main()
{
  Dispatcher dispatcher;
  boost::thread t1(bind(&Dispatcher::network_start, &dispatcher));
  boost::thread t2(bind(&Dispatcher::native_start, &dispatcher));

  JRPC::Server rpc;
  rpc.install_service(rpc_dispatcher(dispatcher));
  rpc.install_service(rpc_systeminfo());

  Server s;
  s.set_rpc(rpc);
  s.run();

  t1.join();
  t2.join();
  return 0;
}

