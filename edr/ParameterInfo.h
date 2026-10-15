#pragma once

#include "Json.h"
#include "ProducerParameters.h"
#include <macgyver/StringConversion.h>
#include <spine/Location.h>
#include <map>
#include <string>

namespace SmartMet
{
namespace Plugin
{
namespace EDR
{
struct metocean_config
{
  std::string standard_name;
  std::string method;
  std::string duration;
  float       level = 0.0F;
};
struct parameter_info_config
{
  std::map<std::string, std::string> description;  // language code -> description
  std::map<std::string, std::string> unit_label;   // language code -> label
  std::map<std::string, std::string> observed_property_label;  // language code -> label
  std::string unit_symbol_value;
  std::string unit_symbol_type;
  std::string unit_id;
  std::string label;
  metocean_config metocean;
};

struct parameter_info
{
  std::string description;
  std::string unit_label;
  std::string observed_property_label;
  std::string unit_symbol_value;
  std::string unit_symbol_type;
  std::string unit_id;
  std::string label;
  metocean_config metocean;

  // CoverageJSON/EDR unit object, the label is an i18n object unless the language is empty
  Json::Value unit(const std::string &unit_label, const std::string &language) const;
};

class ParameterInfo : public std::map<std::string,
                                      parameter_info_config>  // parameter name -> info
{
 public:
  parameter_info get_parameter_info(const std::string &pname, const std::string &language) const;

  const ProducerParameters *producerParameters = nullptr;
};

}  // namespace EDR
}  // namespace Plugin
}  // namespace SmartMet
