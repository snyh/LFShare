#include "Dispatcher.hpp"
#include "../../AppWebServer/server.hpp"
#include <boost/thread.hpp>

using namespace std;

AWS::Service& rpc_dispatcher(Dispatcher&);
AWS::Service& rpc_systeminfo();


int main()
{
  Dispatcher dispatcher;
  boost::thread t1(bind(&Dispatcher::network_start, &dispatcher));
  boost::thread t2(bind(&Dispatcher::native_start, &dispatcher));

  AWS::JSONPServer rpc;
  rpc.install_service(rpc_dispatcher(dispatcher));
  rpc.install_service(rpc_systeminfo());

  AWS::HTTPServer s;
  s.set_rpc(rpc);
  s.open_browser();
  s.run();

  t1.join();
  t2.join();
  return 0;
}

