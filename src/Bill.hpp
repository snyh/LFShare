#ifndef __BILL_HPP__
#define __BILL_HPP__

#define BOOST_DYNAMIC_BITSET_DONT_USE_FRIENDS //boost::dynamic_bitset无法序列化只能设置其成员变量为public
#include <boost/dynamic_bitset.hpp>
#include <boost/serialization/vector.hpp>
namespace boost {
	namespace serialization {
		template <class Archive>
		  void serialize(Archive& ar, boost::dynamic_bitset<>& set, const unsigned int version) {
			  ar & set.m_bits & set.m_num_bits;
		  }
	}
}
#include <boost/serialization/access.hpp>
#include <boost/serialization/map.hpp>
#include <boost/archive/text_iarchive.hpp>
#include <boost/archive/text_oarchive.hpp>

#include <map>
#include <string>
#include <sstream>


typedef std::string Hash;

class Bill {
public:
    Bill(Hash h, boost::dynamic_bitset<> s) { data[h] = s; }
    void merge(const Bill& b)  {
		for (data_type::const_iterator it = b.data.begin();
			 it != b.data.end(); it++) {
			this->data[it->first] |= it->second;
		}
	}
	void check(const Bill& b) {
		for (data_type::const_iterator it = b.data.begin();
			 it != b.data.end(); it++) {
			this->data[it->first] &= it->second;
		}
	}
	void print(); //used for debug;
private:
	typedef std::map<Hash, boost::dynamic_bitset<> > data_type;
	data_type data;

	//序列化Bill
	friend class boost::serialization::access;
	template <class Archive>
	  void serialize(Archive& ar, const unsigned int version) {
		  ar & data;
	  }

};

//通过字符串来保存和读取对象.
template<typename T>
std::string to_string(T t)
{
  std::ostringstream ost;
  boost::archive::text_oarchive toa(ost);
  toa << t;
  return ost.str();
}
template<typename T>
T from_string(const std::string& s)
{
  std::istringstream ist(s);
  boost::archive::text_iarchive tia(ist);
  T t;
  tia  >> t;
  return t;
}

#endif
