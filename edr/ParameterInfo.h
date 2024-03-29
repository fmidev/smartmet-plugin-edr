#pragma once

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
struct parameter_info_config
{
  std::map<std::string, std::string> description;  // language code -> description
  std::map<std::string, std::string> unit_label;   // language code -> label
  std::string unit_symbol_value;
  std::string unit_symbol_type;
};

struct parameter_info
{
  std::string description;
  std::string unit_label;
  std::string unit_symbol_value;
  std::string unit_symbol_type;
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
