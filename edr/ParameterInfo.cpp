#include "ParameterInfo.h"
#include <boost/algorithm/string/case_conv.hpp>

namespace SmartMet
{
namespace Plugin
{
namespace EDR
{
  parameter_info ParameterInfo::get_parameter_info(const std::string& pname, const std::string& language) const
  {
	parameter_info ret;

	auto parameter_name = pname;
	boost::algorithm::to_lower(parameter_name);
	
	if(find(parameter_name) != end())
	  {
		const auto& pinfo = at(parameter_name);
		if(pinfo.description.find(language) != pinfo.description.end())
		  ret.description = pinfo.description.at(language);
		if(pinfo.unit_label.find(language) != pinfo.unit_label.end())
		  ret.unit_label = pinfo.unit_label.at(language);
		if(!pinfo.unit_symbol_value.empty())
		  ret.unit_symbol_value = pinfo.unit_symbol_value;
		if(!pinfo.unit_symbol_type.empty())
		  ret.unit_symbol_type = pinfo.unit_symbol_type;
	  }
	
	return ret;
  }
}  // namespace EDR
}  // namespace Plugin
}  // namespace SmartMet
