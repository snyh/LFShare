#ifndef __FILEMANAGER_HPP__
#define __FILEMANAGER_HPP__

#include "FInfo.hpp"
#include "Transport.hpp"
#include <vector>
#include <map>
#include <string>

struct NewMsg {
	Payload payload;
	std::map<Hash, double> progress;
	std::vector<FInfo> new_files;
};

class FileManager {
public:
  FileManager();

  //更新数据
  NewMsg refresh();

  //添加本地文件到网络中去
  void add_local_file(const std::string& path);

  //获得当前所知的所有文件信息
  std::vector<FInfo> current_list();

  //开始或继续下载某一文件
  void start_download(const Hash& h);

  //停止下载某一文件
  void stop_download(const Hash& h);

  //从当前以及文件信息中删除此某一文件信息
  void remove(const Hash& h);


private:
  void cb_new_file(const FInfo&);
  void cb_new_chunk(const Hash&, double progress);

  FInfoManager info_manager_;
  Transport transport_;
  NewMsg msg_;
};
#endif
