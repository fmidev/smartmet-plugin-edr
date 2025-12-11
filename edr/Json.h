#pragma once

#include <variant>
#include <macgyver/DateTime.h>
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
static int DEFAULT_PRECISION = -1;

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
  DataValue() : data(NullValue()) {}
  DataValue(const std::string &value, bool _isStringObject = false)
      : data(value), isStringObject(_isStringObject) {}
  DataValue(bool value) : data(value) {}
  DataValue(std::size_t value) : data(value) {}
  DataValue(int value) : data(std::size_t(value)) {}
  DataValue(double value) : data(value) {}
  DataValue(const NullValue &value) : data(value) {}

  std::string to_string(int precision = DEFAULT_PRECISION) const;
  ValueType valueType() const;
  const std::variant<NullValue, std::string, double, std::size_t, bool> &get_data() const
  {
    return data;
  }

  bool isStringObjectValue() const { return isStringObject; }

 private:
  std::variant<NullValue, std::string, double, std::size_t, bool> data;
  bool isStringObject = false;
};

using ArrayIndex = std::size_t;

class Value
{
 public:
  Value();
  Value(ValueType valueType);
  Value(const std::string &value, bool isStringObject = false);
  Value(const char *value);
  Value(bool value);
  Value(int value);
  Value(std::size_t value);
  Value(double value, int precision = DEFAULT_PRECISION);
  Value(const NullValue &value);

  Value(const Value &value);
  Value &operator=(const Value &value);
  Value &operator[](const std::string &key);
  Value &operator[](ArrayIndex index);
  std::string toStyledString(bool pretty) const;
  void append(const Value &value);
  std::size_t size() const { return data_value_vector.size(); }
  bool isNull() const { return (valueType == ValueType::nullValue); }

  using ValueVectorType = std::vector<Value>;
  using const_iterator = ValueVectorType::const_iterator;

  const_iterator begin() const;
  const_iterator end() const;

 private:
  std::string to_string(bool pretty) const;
  std::string to_string_impl(bool pretty, unsigned int level) const;
  std::string value() const;
  std::string values_to_string(bool pretty, unsigned int level) const;
  static std::string data_value_vector_to_string(const std::vector<Value> &data_value_vector,
                                                 bool pretty,
                                                 unsigned int level);
  std::string data_value_vector_to_string(bool pretty, unsigned int level) const;

  DataValue data_value;                   // Value is stored here
  std::vector<Value> data_value_vector;   // // Vector of values
  std::map<std::string, Value> values;    // Current level
  std::map<std::string, Value> children;  // Children
  std::map<std::string, Value> key_map;   // Temporary store of values
  ValueType valueType;
  std::string nodeKey;
  int precision = DEFAULT_PRECISION;  // default value for precision (in case of double)
  Value *parentNode{nullptr};
  const ValueVectorType::const_iterator beginIter;
  const ValueVectorType::const_iterator endIter;
};

std::string value_type_to_string(ValueType type);

}  // namespace Json
}  // namespace EDR
}  // namespace Plugin
}  // namespace SmartMet
