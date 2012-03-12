#ifndef TOOLS_HPP_
#define TOOLS_HPP_
#include <boost/format.hpp>
class FLog {
public:
  FLog(const char* msg);
  ~FLog();
  template <typename T>
	FLog& operator%(T value) {
		fmt % value;
		return *this;
	}
protected:
  boost::format fmt;
  long long time_;
};

std::wstring to_ucs2(const std::string&);
std::string to_utf8(const std::wstring&);
#endif
