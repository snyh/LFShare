#ifndef __FILEMANAGER_HPP__
#define __FILEMANAGER_HPP__

#include "FInfo.hpp"
#include "Transport.hpp"
#include <vector>
#include <map>
#include <string>

class FileManager {
public:
  FileManager();

  void upload(const std::string& path);
  std::vector<FInfo> current_list();

  void download(const Hash& h);
  void stop(const Hash& h);

  void remove(const Hash& h);

  void on_complete(const Hash& h);
private:
  FInfoManager info_manager_;
  Transport transport_;
};

#endif
