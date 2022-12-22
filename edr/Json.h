#pragma once

#include <boost/date_time/posix_time/ptime.hpp>
#include <boost/variant.hpp>
#include <iostream>
#include <map>

namespace SmartMet
{
namespace Plugin
{
namespace EDR
{
namespace Json
{
#define DEFAULT_PRECISION 4

enum class ValueType
{
  stringValue,
  doubleValue,
  intValue,
  boolValue,
  nullValue,
  arrayValue,
  objectValue
};

struct NullValue
{
  // NullValues are always identical to themselves, no nothing else
  template <class T>
  bool operator==(const T &other) const;
};

template <class T>
inline bool NullValue::operator==(const T & /* other */) const
{
  return false;
}

template <>
inline bool NullValue::operator==(const NullValue & /* other */) const
{
  return true;
}

class DataValue
{
 public:
  DataValue() { data = NullValue(); }
  DataValue(const DataValue &dv) { data = dv.data; }
  DataValue(const std::string &value) { data = value; }
  DataValue(bool value) { data = value; }
  DataValue(std::size_t value) { data = value; }
  DataValue(int value) { data = std::size_t(value); }
  DataValue(double value) { data = value; }
  DataValue(const NullValue &value) { data = value; }
  DataValue &operator=(const DataValue &);

  std::string to_string(unsigned int precision = DEFAULT_PRECISION) const;
  ValueType valueType() const;
  const boost::variant<NullValue, std::string, double, std::size_t, bool> &get_data() const
  {
    return data;
  }

 private:
  boost::variant<NullValue, std::string, double, std::size_t, bool> data;
};

using ArrayIndex = std::size_t;

class Value
{
 public:
  Value();
  Value(ValueType valueType);
  Value(const std::string &value);
  Value(const char *value);
  Value(bool value);
  Value(int value);
  Value(std::size_t value);
  Value(double value, unsigned int precision = DEFAULT_PRECISION);
  Value(const NullValue &value);

  Value(const Value &value);
  Value &operator=(const Value &value);
  Value &operator[](const std::string &key);
  Value &operator[](ArrayIndex index);
  std::string toStyledString() const;
  void append(const Value &value);
  std::size_t size() const { return data_value_vector.size(); }

  using ValueVectorType = std::vector<Value>;
  using const_iterator = ValueVectorType::const_iterator;

  const_iterator begin() const;
  const_iterator end() const;

 private:
  std::string to_string() const;
  std::string to_string_impl(unsigned int level) const;
  std::string value() const;
  std::string values_to_string(unsigned int level) const;
  std::string data_value_vector_to_string(unsigned int level) const;
  DataValue data_value;                  // Value is stored here
  std::vector<Value> data_value_vector;  // // Vector of values

  std::map<std::string, Value> values;    // Current level
  std::map<std::string, Value> children;  // Children
  std::map<std::string, Value> key_map;   // Temporary store of values

  ValueType valueType;
  std::string nodeKey;
  unsigned int precision{4};  // default value for precision (in case of double)
  Value *parentNode{nullptr};
  const ValueVectorType::const_iterator beginIter;
  const ValueVectorType::const_iterator endIter;
};

std::string value_type_to_string(ValueType type);

}  // namespace Json
}  // namespace EDR
}  // namespace Plugin
}  // namespace SmartMet
