#ifndef __MFILEMANAGER_HPP__
#define __MFILEMANAGER_HPP__

#include <boost/signals2.hpp>
#include "MFile.hpp"

struct Payload {
	int global; //局域网整体下载速度
	std::map<Hash, std::pair<int,int> > file; //单文件下载上传速度
};

class MFileManager {
public:
    MFileManager();

	void upload(std::string path);
	void download(Hash file_hash);

	const std::map<Hash, MFile>& list_file() {return files_;}
	Payload payload_info();

private:
  void install_fileinfo_plugin();
  void install_chunks_plugin();

private:
  	std::map<Hash, MFile> files_;
	std::string buf_;
};

inline
MFileManager& theMFM()
{
  static MFileManager m;;
  return m;
}
#endif
