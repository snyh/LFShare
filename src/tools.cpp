#include <iostream>
#include <boost/thread/mutex.hpp>
#include "tools.hpp"
#include <chrono>
#include "utf8.h"

using namespace std;

boost::mutex log_mutex;
FLog::FLog(const char* msg) 
	 : fmt(msg) 
{
  time_ = chrono::high_resolution_clock::now().time_since_epoch().count();
}
FLog::~FLog() 
{ 
  log_mutex.lock();
  cout << "[" << time_ << "] " << fmt << endl;
  log_mutex.unlock();
}

/// wchar在win平台下是2字节,linux下4字节.
// 这里用wchar来表达ucs2不是很准确但不会影响使用

std::string to_utf8(const std::wstring& s)
{
  string r;
#ifdef WIN32
  utf8::utf32to8(s.begin(), s.end(), back_inserter(r));
#else
  utf8::utf16to8(s.begin(), s.end(), back_inserter(r));
#endif
  return r;
}
std::wstring to_ucs2(const std::string& s)
{
  wstring r;
  try {
#ifdef WIN32
	  utf8::utf8to32(s.begin(), s.end(), back_inserter(r));
#else
	  utf8::utf8to16(s.begin(), s.end(), back_inserter(r));
#endif
  } catch (...) {
	  cout << "Erro invalid UTF-8:" << s << endl;
  }
  return r;
}
