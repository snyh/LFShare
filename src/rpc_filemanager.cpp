#include "AppWebServer/jrpc.hpp"
#include "MFileManager.hpp"

using namespace std;
typedef const JRPC::JSON MP;

string hash2str(Hash h)
{
  char buf[33];
  for (int i=0; i<16; i++) {
	  snprintf(buf+i*2, 3, "%02x", (unsigned char)h[i]);
  }
  return string(buf, 33);
}
Hash str2hash(string s)
{
  Hash hash(16, ' ');
  for (int i=0; i<32; i+=2) {
	  hash[i/2] = stoi(s.substr(i, 2), 0, 16);
  }
  return hash;
}

MP file2json(MFile& file)
{
  JRPC::JSON node;
  node["hash"] = hash2str(file.hash());
  node["name"] = file.name();
  char buf[16];
  snprintf(buf, 16, "%0.3f", file.size());
  node["size"] = buf;
  node["status"] = file.state();
  node["type"] = 0;

  node["progress"] = 100;
  return node;
}

JRPC::Service& rpc_filemanager()
{
  static JRPC::Service filemanager;
  filemanager.name = "filemanager";
  filemanager.methods = {
		{
		  "file_list", [](MP& j){
			  JRPC::JSON result;
			  for (auto i : theMFM().list_file()) {
				  result.append(file2json(i.second));
			  }
			  return result;
		  }
		},
		{
		  "add_file", [](MP& j){
			  MFile& f = theMFM().upload(j["path"].asString());
			  //TODO
			  return file2json(file.);
		  }
		}
  };
  return filemanager;
}

