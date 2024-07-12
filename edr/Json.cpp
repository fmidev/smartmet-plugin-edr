#include "Json.h"
#include <boost/algorithm/string/predicate.hpp>
#include <fmt/format.h>
#include <macgyver/StringConversion.h>
#include <macgyver/ValueFormatter.h>
#include <iostream>
#include <memory>
#include <set>

namespace SmartMet
{
namespace Plugin
{
namespace EDR
{
namespace Json
{
#define UNINITIALIZED_KEY "__uninitialized__"
#define COMMA_PLUS_NEWLINE ",\n"
#define LEFT_ROUND_BRACKET_PLUS_NEWLINE "{\n"
#define RIGHT_ROUND_BRACKET_PLUS_COMMA "},"
#define LEFT_SQUARE_BRACKET_PLUS_NEWLINE "[\n"
#define LEFT_SQUARE_BRACKET "["
#define RIGHT_SQUARE_BRACKET "]"
#define NEWLINE "\n"

std::string value_type_to_string(ValueType type)
{
  switch (type)
  {
    case ValueType::stringValue:
      return "stringValue";
    case ValueType::doubleValue:
      return "doubleValue";
    case ValueType::intValue:
      return "intValue";
    case ValueType::boolValue:
      return "boolValue";
    case ValueType::objectValue:
      return "objectValue";
    case ValueType::arrayValue:
      return "arrayValue";
    case ValueType::nullValue:
      return "nullValue";
  }

  return "UNKNOWN";
}

namespace
{
std::string tabs(bool pretty, unsigned int level)
{
  if (pretty)
    return std::string(level, '\t');
  return {};
}

ValueType get_value_type(const DataValue &dv)
{
  const auto &data = dv.get_data();

  if (std::get_if<std::string>(&data) != nullptr)
    return ValueType::stringValue;

  if (std::get_if<std::size_t>(&data) != nullptr)
    return ValueType::intValue;

  if (std::get_if<double>(&data) != nullptr)
    return ValueType::doubleValue;

  if (std::get_if<bool>(&data) != nullptr)
    return ValueType::boolValue;

  return ValueType::nullValue;
}

std::string json_encode(const std::string &input)
{
  std::string output;
  output.reserve(input.size());

  for (unsigned char c : input)
  {
    switch (c)
    {
      case '"':
        output += "\\\"";
        break;
      case '\\':
        output += "\\\\";
        break;
      case '\b':
        output += "\\b";
        break;
      case '\f':
        output += "\\f";
        break;
      case '\n':
        output += "\\n";
        break;
      case '\r':
        output += "\\r";
        break;
      case '\t':
        output += "\\t";
        break;
      default:
        if (c < 0x20)
          output += fmt::format("\\u{:04x}", c);
        else
          output += c;
        break;
    }
  }

  return output;
}

std::string data_value_to_string(const DataValue &dv, int precision)
{
  std::string ret;

  ValueType vt = get_value_type(dv);

  const auto &data = dv.get_data();

  if (vt == ValueType::stringValue)
  {
    auto str = *(std::get_if<std::string>(&data));
    ret = "\"" + json_encode(str) + "\"";
  }
  else if (vt == ValueType::intValue)
  {
    ret = Fmi::to_string(*(std::get_if<std::size_t>(&data)));
  }
  else if (vt == ValueType::boolValue)
  {
    auto bool_value = *(std::get_if<bool>(&data));
    ret = (bool_value ? "true" : "false");
  }
  else if (vt == ValueType::doubleValue)
  {
    Fmi::ValueFormatterParam fmtParam("null", "fixed");
    Fmi::ValueFormatter formatter(fmtParam);
    double value = *(std::get_if<double>(&data));
    ret = formatter.format(value, precision);
  }
  else if (vt == ValueType::nullValue)
  {
    ret = "null";
  }

  return ret;
}

}  // anonymous namespace

Value::Value()
    : valueType(ValueType::nullValue),
      nodeKey(UNINITIALIZED_KEY),
      beginIter(data_value_vector.begin()),
      endIter(data_value_vector.end())
{
}

Value::Value(ValueType type)
    : valueType(type),
      nodeKey(UNINITIALIZED_KEY),
      beginIter(data_value_vector.begin()),
      endIter(data_value_vector.end())
{
}

Value::Value(const std::string &value)
    : data_value(value),
      valueType(ValueType::stringValue),
      beginIter(data_value_vector.begin()),
      endIter(data_value_vector.end())
{
}

Value::Value(const char *value)
    : data_value(std::string(value)),
      valueType(ValueType::stringValue),
      beginIter(data_value_vector.begin()),
      endIter(data_value_vector.end())
{
}

Value::Value(std::size_t value)
    : data_value(value),
      valueType(ValueType::intValue),
      nodeKey(UNINITIALIZED_KEY),
      beginIter(data_value_vector.begin()),
      endIter(data_value_vector.end())
{
}

Value::Value(int value)
    : data_value(value),
      valueType(ValueType::intValue),
      nodeKey(UNINITIALIZED_KEY),
      beginIter(data_value_vector.begin()),
      endIter(data_value_vector.end())
{
}

Value::Value(bool value)
    : data_value(value),
      valueType(ValueType::boolValue),
      nodeKey(UNINITIALIZED_KEY),
      beginIter(data_value_vector.begin()),
      endIter(data_value_vector.end())
{
}

Value::Value(double value, int prec /*= DEFAULT_PRECISION*/)
    : data_value(value),
      valueType(ValueType::doubleValue),
      nodeKey(UNINITIALIZED_KEY),
      precision(prec),
      beginIter(data_value_vector.begin()),
      endIter(data_value_vector.end())
{
}

Value::Value(const NullValue &value)
    : data_value(value),
      valueType(ValueType::nullValue),
      nodeKey(UNINITIALIZED_KEY),
      beginIter(data_value_vector.begin()),
      endIter(data_value_vector.end())
{
}

Value &Value::operator=(const Value &value)
{
  if (this == &value)
    return *this;

  if (parentNode != nullptr)
  {
    if (value.valueType == ValueType::objectValue)
    {
      //  std::cout << "Assigning object for key: " << nodeKey << std::endl;
      // If object value -> child
      auto &key_value = parentNode->children[nodeKey];
      key_value.valueType = value.valueType;
      key_value.data_value = value.data_value;
      key_value.data_value_vector = value.data_value_vector;
      key_value.values = value.values;
      key_value.precision = value.precision;
      key_value.parentNode = parentNode;
      for (const auto &item : value.children)
      {
        auto &key_value_child = key_value.children[item.first];
        key_value_child.data_value = item.second.data_value;
        key_value_child.data_value_vector = item.second.data_value_vector;
        key_value_child.values = item.second.values;
        key_value_child.children = item.second.children;
      }
    }
    else
    {
      // data value
      //		  std::cout << "Assgining data value for key_0 " <<
      // std::endl;
      auto &key_value = (nodeKey == UNINITIALIZED_KEY ? *this : parentNode->values[nodeKey]);
      //		  std::cout << "Assgining data value for key "  <<
      //&key_value << ", "<< nodeKey  << std::endl;
      key_value.data_value = value.data_value;
      key_value.data_value_vector = value.data_value_vector;
      key_value.values = value.values;
      key_value.precision = value.precision;
      key_value.children = value.children;
      key_value.valueType = value.valueType;
      key_value.parentNode = parentNode;
    }
  }
  else
  {
    //	  std::cout << "Assignment operator= to this (this, key, valuekey) " <<
    // this << ", " << nodeKey << ", "  << value.nodeKey << ", "  <<
    // value_type_to_string(valueType) << std::endl;
    data_value = value.data_value;
    data_value_vector = value.data_value_vector;
    values = value.values;
    precision = value.precision;
    children = value.children;
    valueType = value.valueType;
    parentNode = value.parentNode;
  }

  return *this;
}

Value &Value::operator[](const std::string &key)
{
  if (key != UNINITIALIZED_KEY)
  {
    // If key found in values
    if (values.find(key) != values.end())
    {
      return values.at(key);
    }
    // If key found in children
    if (children.find(key) != children.end())
    {
      return children.at(key);
    }

    // Return object from key_map, it is later inserted into values or children
    if (key_map.find(key) == key_map.end())
      key_map[key] = Value();
    key_map[key].parentNode = this;
    key_map[key].nodeKey = key;
    return key_map[key];
  }

  nodeKey = key;

  return *this;
}

Value::Value(const Value &value)
    : data_value(value.data_value),
      data_value_vector(value.data_value_vector),
      values(value.values),
      children(value.children),
      valueType(value.valueType),
      nodeKey(value.nodeKey),
      precision(value.precision),
      parentNode(value.parentNode)

{
  //  std::cout << "Copy constructor\n";
  /*
  data_value = value.data_value;
  data_value_vector = value.data_value_vector;
  values = value.values;
  children = value.children;
  nodeKey = value.nodeKey;
  valueType = value.valueType;
  parentNode = value.parentNode;
  precision = value.precision;
  */
}

Value &Value::operator[](ArrayIndex index)
{
  if (valueType != ValueType::arrayValue)
  {
    // This is actully not error, but value type should be set before using it,
    // for example
    //  **
    //  ** auto referencing_xy = Json::Value(Json::ValueType::objectValue);
    //  ** referencing_xy["coordinates"] =
    //  Json::Value(Json::ValueType::arrayValue);
    //  ** referencing_xy["coordinates"][0] = Json::Value("y");
  }

  while (data_value_vector.size() <= index)
    data_value_vector.emplace_back(Value());

  return data_value_vector.at(index);
}

std::string Value::data_value_vector_to_string(const std::vector<Value> &data_value_vector,
                                               bool pretty,
                                               unsigned int level)
{
  std::string value_array;
  for (const auto &val : data_value_vector)
  {
    if (val.valueType < ValueType::arrayValue)
    {
      auto new_value = val.data_value.to_string(val.precision);
      if (!value_array.empty() && !new_value.empty())
        value_array.append(COMMA_PLUS_NEWLINE);
      value_array.append(tabs(pretty, level + 2) + new_value);
    }
    else
    {
      auto new_value = val.to_string_impl(pretty, level + 2);
      if (!value_array.empty() && !new_value.empty())
      {
        if (boost::algorithm::starts_with(new_value, NEWLINE))
          value_array.append(",");
        else
          value_array.append(COMMA_PLUS_NEWLINE);
      }
      value_array.append(new_value);
    }
  }
  return value_array;
}

std::string Value::values_to_string(bool pretty, unsigned int level) const
{
  std::string result;

  if (values.empty())
    return data_value_vector_to_string(pretty, level);

  std::vector<std::string> keys;
  for (const auto &item : values)
  {
    if (!(item.first == "id" || item.first == "title" || item.first == "description" ||
          item.first == "links" || item.first == "output_formats" || item.first == "keywords" ||
          item.first == "crs"))
      keys.push_back(item.first);
  }

  // Order of fields in output document: id,title,description,links,output_formats,keywords,crs

  const std::array<const char *, 7> fields{
      "crs", "keywords", "output_formats", "links", "description", "title", "id"};

  for (const auto *field : fields)
    if (values.find(field) != values.end())
      keys.insert(keys.begin(), field);

  for (const auto &key : keys)
  {
    const auto &value_obj = values.at(key);
    std::string value = (tabs(pretty, level + 1) + "\"" + key + "\" : ");
    if (!value_obj.data_value_vector.empty())
    {
      auto value_array = data_value_vector_to_string(value_obj.data_value_vector, pretty, level);

      if (boost::algorithm::starts_with(value_array, NEWLINE))
        value.append(NEWLINE + tabs(pretty, level + 1) + LEFT_SQUARE_BRACKET);
      else
        value.append(NEWLINE + tabs(pretty, level + 1) + LEFT_SQUARE_BRACKET_PLUS_NEWLINE);
      value.append(value_array + NEWLINE + tabs(pretty, level + 1) + RIGHT_SQUARE_BRACKET);
    }
    else
    {
      DataValue dv = value_obj.data_value;
      std::string data = data_value_to_string(dv, value_obj.precision);
      if (data.empty())
        data = "error: data empty";
      value.append(data);
    }
    if (!result.empty() && !boost::algorithm::ends_with(result, LEFT_ROUND_BRACKET_PLUS_NEWLINE) &&
        !boost::algorithm::ends_with(result, RIGHT_ROUND_BRACKET_PLUS_COMMA) && !value.empty())
      result.append(COMMA_PLUS_NEWLINE);

    result.append(value);
  }

  return result;
}

std::string Value::data_value_vector_to_string(bool pretty, unsigned int level) const
{
  auto ret = std::string();

  for (const auto &dv : data_value_vector)
  {
    std::string value;
    if (dv.valueType < ValueType::arrayValue)
      value = dv.data_value.to_string(dv.precision);
    else
      value = dv.to_string(pretty);
    if (!value.empty())
    {
      if (!ret.empty() && !boost::algorithm::ends_with(ret, LEFT_ROUND_BRACKET_PLUS_NEWLINE) &&
          !boost::algorithm::ends_with(ret, RIGHT_ROUND_BRACKET_PLUS_COMMA))
        ret.append(COMMA_PLUS_NEWLINE);
      ret.append(tabs(pretty, level + 1) + value);
    }
  }

  if (ret.empty())
    return ret;

  return (tabs(pretty, level) + LEFT_SQUARE_BRACKET_PLUS_NEWLINE + ret + NEWLINE +
          tabs(pretty, level) + RIGHT_SQUARE_BRACKET);
}

std::string Value::to_string_impl(bool pretty, unsigned int level) const
{
  if (valueType == ValueType::arrayValue)
    return data_value_vector_to_string(pretty, level);

  std::string result =
      (level == 0 ? LEFT_ROUND_BRACKET_PLUS_NEWLINE
                  : (NEWLINE + tabs(pretty, level) + LEFT_ROUND_BRACKET_PLUS_NEWLINE));

  result.append(values_to_string(pretty, level));
  std::string children_string;
  for (const auto &item : children)
  {
    auto child_key = item.first;
    auto child_value = item.second.to_string_impl(pretty, level + 1);
    if (!children_string.empty())
      children_string.append(COMMA_PLUS_NEWLINE);
    children_string.append(tabs(pretty, level + 1) + "\"" + child_key + "\" : " + child_value);
  }
  if (!result.empty() && !boost::algorithm::ends_with(result, LEFT_ROUND_BRACKET_PLUS_NEWLINE) &&
      !boost::algorithm::ends_with(result, RIGHT_ROUND_BRACKET_PLUS_COMMA) &&
      !boost::algorithm::ends_with(result, COMMA_PLUS_NEWLINE) && !children_string.empty())
    result.append(COMMA_PLUS_NEWLINE);

  result.append(children_string);
  result.append(NEWLINE + tabs(pretty, level) + "}");

  return result;
}

std::string Value::to_string(bool pretty) const
{
  return to_string_impl(pretty, 0);
}

std::string Value::toStyledString(bool pretty) const
{
  return to_string(pretty);
}

std::string Value::value() const
{
  return data_value.to_string(precision);
}

Value::const_iterator Value::begin() const
{
  return beginIter;
}

Value::const_iterator Value::end() const
{
  return endIter;
}

void Value::append(const Value &value)
{
  if (!value.data_value_vector.empty())
    data_value_vector.insert(
        data_value_vector.end(), value.data_value_vector.begin(), value.data_value_vector.end());
  else
    data_value_vector.push_back(value);
}

std::string DataValue::to_string(int precision /*= DEFAULT_PRECISION*/) const
{
  return (data_value_to_string(*this, precision));
}

ValueType DataValue::valueType() const
{
  return get_value_type(*this);
}

}  // namespace Json
}  // namespace EDR
}  // namespace Plugin
}  // namespace SmartMet
