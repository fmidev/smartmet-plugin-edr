#include "ParameterInfo.h"
#include <boost/algorithm/string/case_conv.hpp>

namespace SmartMet
{
namespace Plugin
{
namespace EDR
{
Json::Value parameter_info::unit(const std::string &unit_label, const std::string &language) const
{
  auto ret = Json::Value(Json::ValueType::objectValue);
  if (!unit_id.empty())
    ret["id"] = Json::Value(unit_id);
  if (language.empty())
    ret["label"] = Json::Value(unit_label);
  else
  {
    auto lang_label = Json::Value(Json::ValueType::objectValue);
    lang_label[language] = Json::Value(unit_label);
    ret["label"] = lang_label;
  }
  // The symbol is a plain string unless the unit serialization scheme is known
  if (unit_symbol_type.empty())
    ret["symbol"] = Json::Value(unit_symbol_value);
  else
  {
    ret["symbol"] = Json::Value(Json::ValueType::objectValue);
    ret["symbol"]["value"] = Json::Value(unit_symbol_value);
    ret["symbol"]["type"] = Json::Value(unit_symbol_type);
  }
  return ret;
}

parameter_info ParameterInfo::get_parameter_info(const std::string &pname,
                                                 const std::string &language) const
{
  parameter_info ret;

  auto parameter_name = pname;
  boost::algorithm::to_lower(parameter_name);

  if (find(parameter_name) != end())
  {
    const auto &pinfo = at(parameter_name);
    ret.label = pinfo.label;
    ret.metocean = pinfo.metocean;
    if (pinfo.description.find(language) != pinfo.description.end())
      ret.description = pinfo.description.at(language);
    if (pinfo.observed_property_label.find(language) != pinfo.observed_property_label.end())
      ret.observed_property_label = pinfo.observed_property_label.at(language);
    else
      ret.observed_property_label = pinfo.label;
    if (pinfo.unit_label.find(language) != pinfo.unit_label.end())
      ret.unit_label = pinfo.unit_label.at(language);
    if (!pinfo.unit_symbol_value.empty())
      ret.unit_symbol_value = pinfo.unit_symbol_value;
    if (!pinfo.unit_symbol_type.empty())
      ret.unit_symbol_type = pinfo.unit_symbol_type;
    ret.unit_id = pinfo.unit_id;
  }

  return ret;
}
}  // namespace EDR
}  // namespace Plugin
}  // namespace SmartMet
