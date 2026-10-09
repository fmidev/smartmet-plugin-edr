#include "Json.h"
#include <boost/algorithm/string/predicate.hpp>
#include <fmt/format.h>
#include <macgyver/StringConversion.h>
#include <macgyver/ValueFormatter.h>
#include <algorithm>
#include <array>
#include <iostream>
#include <iterator>
#include <memory>
#include <set>
#include <string_view>
#include <type_traits>
#include <utility>

namespace SmartMet
{
namespace Plugin
{
namespace EDR
{
namespace Json
{
#define UNINITIALIZED_KEY "__uninitialized__"
#define RIGHT_ROUND_BRACKET_PLUS_COMMA "},"
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
// Insignificant whitespace is emitted only when pretty printing has been requested. All the
// helpers below return the compact form when pretty is false.

std::string tabs(bool pretty, unsigned int level)
{
  if (pretty)
    return std::string(level, '\t');
  return {};
}

std::string newline(bool pretty)
{
  return (pretty ? NEWLINE : "");
}

// Separator between array elements and between object members
std::string comma(bool pretty)
{
  return (pretty ? ",\n" : ",");
}

// Separator between an object member name and its value
std::string name_separator(bool pretty)
{
  return (pretty ? " : " : ":");
}

std::string open_brace(bool pretty)
{
  return (pretty ? "{\n" : "{");
}

std::string open_bracket(bool pretty)
{
  return (pretty ? "[\n" : "[");
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

void append_json_encoded(std::string &out, const std::string &input, bool isStringObject)
{
  for (unsigned char c : input)
  {
    switch (c)
    {
      case '"':
        out += (isStringObject ? "\"" : "\\\"");
        break;
      case '\\':
        out += "\\\\";
        break;
      case '\b':
        out += "\\b";
        break;
      case '\f':
        out += "\\f";
        break;
      case '\n':
        out += "\\n";
        break;
      case '\r':
        out += "\\r";
        break;
      case '\t':
        out += "\\t";
        break;
      default:
        if (c < 0x20)
          out += fmt::format("\\u{:04x}", c);
        else
          out += static_cast<char>(c);
        break;
    }
  }
}

// Only a string object (inserted into the output as is) can be serialized to nothing
bool is_empty_data_value(const DataValue &dv)
{
  const auto *str = std::get_if<std::string>(&dv.get_data());
  return (str != nullptr && dv.isStringObjectValue() && str->empty());
}

void append_data_value(std::string &out, const DataValue &dv, int precision)
{
  const auto &data = dv.get_data();

  if (const auto *str = std::get_if<std::string>(&data))
  {
    if (dv.isStringObjectValue())
      append_json_encoded(out, *str, true);
    else
    {
      out += '"';
      append_json_encoded(out, *str, false);
      out += '"';
    }
  }
  else if (const auto *ivalue = std::get_if<std::size_t>(&data))
  {
    out += Fmi::to_string(*ivalue);
  }
  else if (const auto *dvalue = std::get_if<double>(&data))
  {
    // format() does not modify the formatter, so one instance can be shared
    static const Fmi::ValueFormatter formatter(Fmi::ValueFormatterParam("null", "fixed"));
    out += formatter.format(*dvalue, precision);
  }
  else if (const auto *bvalue = std::get_if<bool>(&data))
  {
    out += (*bvalue ? "true" : "false");
  }
  else
  {
    out += "null";
  }
}

std::string data_value_to_string(const DataValue &dv, int precision)
{
  std::string ret;
  append_data_value(ret, dv, precision);
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

Value::Value(const std::string &value, bool _isStringObject)
    : data_value(value, _isStringObject),
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

namespace
{
// Moves a member of the assigned value when assigning from an rvalue, copies it otherwise
template <typename V, typename T>
decltype(auto) member(T &m)
{
  if constexpr (std::is_rvalue_reference_v<V &&>)
    return std::move(m);
  else
    return static_cast<const T &>(m);
}
}  // namespace

Value &Value::operator=(const Value &value)
{
  return assign(value);
}

Value &Value::operator=(Value &&value)
{
  return assign(std::move(value));
}

template <typename V>
Value &Value::assign(V &&value)
{
  if (this == &value)
    return *this;

  if (parentNode != nullptr)
  {
    if (value.valueType == ValueType::objectValue)
    {
      // If object value -> child
      auto &key_value = parentNode->children[nodeKey];
      key_value.valueType = value.valueType;
      key_value.data_value = member<V>(value.data_value);
      key_value.data_value_vector = member<V>(value.data_value_vector);
      key_value.values = member<V>(value.values);
      key_value.precision = value.precision;
      key_value.parentNode = parentNode;
      for (auto &item : value.children)
      {
        auto &key_value_child = key_value.children[item.first];
        key_value_child.data_value = member<V>(item.second.data_value);
        key_value_child.data_value_vector = member<V>(item.second.data_value_vector);
        key_value_child.values = member<V>(item.second.values);
        key_value_child.children = member<V>(item.second.children);
      }
    }
    else
    {
      // data value
      auto &key_value = (nodeKey == UNINITIALIZED_KEY ? *this : parentNode->values[nodeKey]);
      key_value.data_value = member<V>(value.data_value);
      key_value.data_value_vector = member<V>(value.data_value_vector);
      key_value.values = member<V>(value.values);
      key_value.precision = value.precision;
      key_value.children = member<V>(value.children);
      key_value.valueType = value.valueType;
      key_value.parentNode = parentNode;
    }
  }
  else
  {
    data_value = member<V>(value.data_value);
    data_value_vector = member<V>(value.data_value_vector);
    values = member<V>(value.values);
    precision = value.precision;
    children = member<V>(value.children);
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

Value::Value(Value &&value) noexcept
    : data_value(std::move(value.data_value)),
      data_value_vector(std::move(value.data_value_vector)),
      values(std::move(value.values)),
      children(std::move(value.children)),
      valueType(value.valueType),
      nodeKey(std::move(value.nodeKey)),
      precision(value.precision),
      parentNode(value.parentNode)
{
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
    data_value_vector.emplace_back();

  return data_value_vector.at(index);
}

// The serializer appends everything to a single output buffer. Separators and brackets depend on
// how the next part of the output begins, which is decided from the values before they are
// written instead of inspecting already serialized temporary strings.

bool Value::is_empty_array(const std::vector<Value> &elements)
{
  for (const auto &val : elements)
  {
    if (val.valueType < ValueType::arrayValue)
    {
      if (!is_empty_data_value(val.data_value))
        return false;
    }
    else if (val.valueType != ValueType::arrayValue || !is_empty_array(val.data_value_vector))
      return false;  // objects always have braces
  }
  return true;
}

// Whether append_elements output begins with a line break: only pretty printed objects do
bool Value::elements_start_with_newline(const std::vector<Value> &elements, bool pretty)
{
  if (!pretty)
    return false;

  for (const auto &val : elements)
  {
    if (val.valueType == ValueType::objectValue)
      return true;
    // Scalars always begin with indentation, empty arrays produce nothing
    if (val.valueType < ValueType::arrayValue || !is_empty_array(val.data_value_vector))
      return false;
  }
  return false;
}

void Value::append_elements(std::string &out,
                            const std::vector<Value> &elements,
                            bool pretty,
                            unsigned int level)
{
  const std::size_t start = out.size();
  for (const auto &val : elements)
  {
    if (val.valueType < ValueType::arrayValue)
    {
      if (out.size() > start && !is_empty_data_value(val.data_value))
        out.append(comma(pretty));
      out.append(tabs(pretty, level + 2));
      append_data_value(out, val.data_value, val.precision);
    }
    else
    {
      const bool empty =
          (val.valueType == ValueType::arrayValue && is_empty_array(val.data_value_vector));
      if (out.size() > start && !empty)
      {
        // Objects begin with a line break of their own when pretty printing
        if (pretty && val.valueType == ValueType::objectValue)
          out.append(",");
        else
          out.append(comma(pretty));
      }
      val.append_to_string(out, pretty, level + 2);
    }
  }
}

void Value::append_values(std::string &out, bool pretty, unsigned int level) const
{
  if (values.empty())
  {
    append_array(out, pretty, level);
    return;
  }

  // Order of fields in output document: id,title,description,links,output_formats,keywords,crs
  // and then the rest in alphabetical order

  const std::array<const char *, 7> fields{
      "id", "title", "description", "links", "output_formats", "keywords", "crs"};

  std::vector<const std::pair<const std::string, Value> *> members;
  members.reserve(values.size());
  for (const auto *field : fields)
  {
    auto pos = values.find(field);
    if (pos != values.end())
      members.push_back(&(*pos));
  }
  if (members.size() < values.size())
  {
    for (const auto &item : values)
    {
      if (std::find_if(fields.begin(),
                       fields.end(),
                       [&item](const char *field) { return item.first == field; }) == fields.end())
        members.push_back(&item);
    }
  }

  const std::size_t start = out.size();

  for (const auto *member : members)
  {
    const auto &key = member->first;
    const auto &value_obj = member->second;

    const std::string_view result(out.data() + start, out.size() - start);
    if (!result.empty() && !boost::algorithm::ends_with(result, open_brace(pretty)) &&
        !boost::algorithm::ends_with(result, RIGHT_ROUND_BRACKET_PLUS_COMMA))
      out.append(comma(pretty));

    out.append(tabs(pretty, level + 1));
    out.append("\"");
    out.append(key);
    out.append("\"");
    out.append(name_separator(pretty));

    if (!value_obj.data_value_vector.empty())
    {
      out.append(newline(pretty));
      out.append(tabs(pretty, level + 1));
      if (elements_start_with_newline(value_obj.data_value_vector, pretty))
        out.append(LEFT_SQUARE_BRACKET);
      else
        out.append(open_bracket(pretty));
      append_elements(out, value_obj.data_value_vector, pretty, level);
      out.append(newline(pretty));
      out.append(tabs(pretty, level + 1));
      out.append(RIGHT_SQUARE_BRACKET);
    }
    else if (is_empty_data_value(value_obj.data_value))
      out.append("error: data empty");
    else
      append_data_value(out, value_obj.data_value, value_obj.precision);
  }
}

void Value::append_array(std::string &out, bool pretty, unsigned int level) const
{
  if (is_empty_array(data_value_vector))
    return;

  out.append(tabs(pretty, level));
  out.append(open_bracket(pretty));

  const std::size_t start = out.size();
  for (const auto &dv : data_value_vector)
  {
    const bool scalar = (dv.valueType < ValueType::arrayValue);
    if (scalar ? is_empty_data_value(dv.data_value)
               : (dv.valueType == ValueType::arrayValue && is_empty_array(dv.data_value_vector)))
      continue;

    const std::string_view ret(out.data() + start, out.size() - start);
    if (!ret.empty() && !boost::algorithm::ends_with(ret, open_brace(pretty)) &&
        !boost::algorithm::ends_with(ret, RIGHT_ROUND_BRACKET_PLUS_COMMA))
      out.append(comma(pretty));
    out.append(tabs(pretty, level + 1));
    if (scalar)
      append_data_value(out, dv.data_value, dv.precision);
    else
      dv.append_to_string(out, pretty, 0);
  }

  out.append(newline(pretty));
  out.append(tabs(pretty, level));
  out.append(RIGHT_SQUARE_BRACKET);
}

std::string Value::to_string_impl(bool pretty, unsigned int level) const
{
  std::string result;
  append_to_string(result, pretty, level);
  return result;
}

// Appends the serialized value to the output so that nested objects are written directly into
// one buffer instead of being built as temporaries and copied into each parent level.
void Value::append_to_string(std::string &out, bool pretty, unsigned int level) const
{
  if (valueType == ValueType::arrayValue)
  {
    append_array(out, pretty, level);
    return;
  }

  const std::size_t start = out.size();

  if (level == 0)
    out.append(open_brace(pretty));
  else
  {
    out.append(newline(pretty));
    out.append(tabs(pretty, level));
    out.append(open_brace(pretty));
  }

  append_values(out, pretty, level);

  // The separator test must only look at what this object has written so far
  if (!children.empty())
  {
    const std::string_view result(out.data() + start, out.size() - start);
    if (!result.empty() && !boost::algorithm::ends_with(result, open_brace(pretty)) &&
        !boost::algorithm::ends_with(result, RIGHT_ROUND_BRACKET_PLUS_COMMA) &&
        !boost::algorithm::ends_with(result, comma(pretty)))
      out.append(comma(pretty));
  }

  bool first = true;
  for (const auto &item : children)
  {
    if (!first)
      out.append(comma(pretty));
    first = false;
    out.append(tabs(pretty, level + 1));
    out.append("\"");
    out.append(item.first);
    out.append("\"");
    out.append(name_separator(pretty));
    item.second.append_to_string(out, pretty, level + 1);
  }

  out.append(newline(pretty));
  out.append(tabs(pretty, level));
  out.append("}");
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

void Value::append(Value &&value)
{
  if (!value.data_value_vector.empty())
    data_value_vector.insert(data_value_vector.end(),
                             std::make_move_iterator(value.data_value_vector.begin()),
                             std::make_move_iterator(value.data_value_vector.end()));
  else
    data_value_vector.push_back(std::move(value));
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
