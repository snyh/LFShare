/*
 * JSON spec
 * Service: system_info
 *
 * Method: list_file
 *
 * Need parameter : path
 * the first call this method can supply "." to get current 
 * file list.
 *
 * On successfuly
 * result: [
 * 		{
 * 			"type": 1(file) | 
 * 					2(dir) |
 * 					3(parent dir) | 
 * 					4(driver name,windows only),
 * 			"path": "absoulte path"(used for ajax fetch other info,
 * 			"name": "file or dir name"(used for show in web)
 * 		}
 * ]
 *
 **********You didn't need read below implementation details*******
 */

#include "../../../AppWebServer/jrpc.hpp"
#include <boost/filesystem.hpp>
#include <vector>
#include "../tools.hpp"
using namespace std;

struct Node {
	int type;
	string path;
	string name;
};

namespace {
	namespace fs = boost::filesystem;
	void add_drivers(vector<Node>& nodes)
	  {
		char driver[] = "c:/";
		boost::system::error_code ignore_ec;
		for (; driver[0] <= 'z'; driver[0]++) {
			if (fs::exists(driver, ignore_ec)) {
				nodes.push_back({ 4, driver, driver });
			}
		}
	  }

	vector<Node> list_file(const wstring& ps )
	  {
		fs::path p(ps);
		vector<Node> nodes;
		boost::system::error_code ignore_ec;
		if (!is_directory(p) || !fs::exists(p, ignore_ec)){
			clog << "dir_list()'s parameter must be a directory" << endl;
			return nodes;
		}
		p = fs::canonical(p);

		if (p.has_root_name() && p.parent_path() == p.root_path() ) {
			//linux always not have a root_name.
			//windows always has one, and root_path() equal parent_path() when X:/p 
			add_drivers(nodes);
		} else if (p.has_parent_path()){
			Node n{3,
				to_utf8(p.parent_path().wstring()),
				to_utf8(p.filename().wstring()),
			};
			nodes.push_back(n);
		}


		fs::directory_iterator end_iter;
		for (fs::directory_iterator dir_itr(p); 
			 dir_itr != end_iter;
			 ++dir_itr) {
			try {
				if (fs::is_directory(dir_itr->status())) {
					Node n{
						2,
						  to_utf8(dir_itr->path().wstring()),
						  to_utf8(dir_itr->path().filename().wstring()),
					};
					nodes.push_back(n);
				} else if (fs::is_regular_file(dir_itr->status())) {
					Node n{
						1,
						  to_utf8(dir_itr->path().wstring()),
						  to_utf8(dir_itr->path().filename().wstring()),
					};
					nodes.push_back(n);
				}

			} catch (const std::exception & ex) {
				clog << dir_itr->path().filename() << " " << ex.what() << endl;
			}
		}
		return  nodes;
	  }
}

AWS::Service& rpc_systeminfo()
{
  static AWS::Service systeminfo;
  systeminfo.name = "systeminfo";
  systeminfo.methods = {
		{
		  "list_file", [](const AWS::JSON& j){
			  AWS::JSON result;
			  if (j["path"].isString()) {
				  string p(j["path"].asString());
				  for (auto& n : list_file(to_ucs2(p))) {
					  AWS::JSON node;
					  node["type"] = n.type;
					  node["path"] = n.path;
					  node["name"] = n.name;
					  result.append(node);
				  }
			  } 
			  return result;
		  }
		},
		{
		  "close_server", [](const AWS::JSON& j) {
			  cout << "close_server" << endl;
			  exit(0);
			  return AWS::JSON();
		  }
		}
  };
  return systeminfo;
}
