#include "AppWebServer/jrpc.hpp"
#include "../FileManager.hpp"
#include <boost/filesystem.hpp>

using namespace std;
namespace fs = boost::filesystem;
typedef const JRPC::JSON MP;

string hash2str(const Hash& h)
{
  char buf[33];
  for (int i=0; i<16; i++) {
	  snprintf(buf+i*2, 3, "%02x", (unsigned char)h[i]);
  }
  return string(buf, 33);
}
Hash str2hash(const string& s)
{
  Hash hash(16, ' ');
  for (int i=0; i<32; i+=2) {
	  hash[i/2] = stoi(s.substr(i, 2), 0, 16);
  }
  return hash;
}

MP info2json(const FInfo& info)
{
  JRPC::JSON node;
  node["hash"] = hash2str(info.hash);
  node["name"] = fs::path(info.path).filename().string();
  node["size"] = (info.chunknum-1) * FInfo::chunksize + info.lastchunksize;
  node["status"] = info.type;

  //当前本版不支持文件目录因此全是文件类型(0)
  node["type"] = 0;
  return node;
}

MP msg2json(const NewMsg& msg)
{
  JRPC::JSON result; result["newfile"]; result["payload"]; result["pfile"]; result["progress"];
  for (auto& file: msg.new_files) {
	  result["newfile"].append(info2json(file));
  }
  result["payload"] = msg.payload.global;
  for (auto& payload: msg.payload.files) {
	  static JRPC::JSON node;
	  node["hash"] = hash2str(payload.first);
	  node["value"] = payload.second;
	  result["pfile"].append(node);
  }
  for (auto& p : msg.progress) {
	  static JRPC::JSON node;
	  node["hash"] = hash2str(p.first);
	  node["value"] = p.second;
	  result["progress"].append(node);
  }
  return result;
}

JRPC::Service& rpc_filemanager(FileManager& theMFM)
{
  static JRPC::Service filemanager;
  filemanager.name = "filemanager";
  filemanager.methods = {
		{
		  "file_list", [&](MP& j){
			  JRPC::JSON result;
			  for (auto i : theMFM.current_list()) {
				  result.append(info2json(i));
			  }
			  return result;
		  }
		},
		{
		  "add_file", [&](MP& j){
			  try {
				  theMFM.add_local_file(j["path"].asString());
				  return JRPC::JSON("添加文件成功");
			  } catch (InfoExists&) {
				  return JRPC::JSON("已经存在此文件"); 
			  } catch (...) {
				  return JRPC::JSON("未知错误"); 
			  }
		  }
		},
		{
		  "refresh", [&](MP& j){
			  NewMsg msg = theMFM.refresh();
			  return msg2json(msg);
		  }
		}
  };
  return filemanager;
}

