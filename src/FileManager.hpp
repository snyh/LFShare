#ifndef __FILEMANAGER_HPP__
#define __FILEMANAGER_HPP__

#include "FInfo.hpp"

class FileManager {
public:
  FileManager();

  FInfo upload(std::string path);
  vector<MFInfo> infos();

  void download(Hash h);
  void stop(Hash h);

  void remove(Hash h);

  void on_complete(Hash file_hash);
private:
  map<Hash, FInfo> tmps_;
  Transport port_;
};

inline
FileManager& theMFM() {
	static FileManager m;;
	return m;
}
#endif
