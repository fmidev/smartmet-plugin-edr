#pragma once

#include <string>
#include <map>
#include <spine/Location.h>
#include <macgyver/StringConversion.h>

namespace SmartMet
{
namespace Plugin
{
namespace EDR
{
  struct location_info
  {
	location_info(const Spine::LocationPtr& loc, const std::string& k) : id(loc->fmisid ? Fmi::to_string(*loc->fmisid) : Fmi::to_string(loc->geoid))
																	   ,longitude(loc->longitude)
																	   ,latitude(loc->latitude)
																	   ,name(loc->name)
																	   ,type(loc->fmisid ? "fmisid" : "geoid")
																	   ,keyword(k) {}
	location_info(const location_info& li) : id(li.id)
										   ,longitude(li.longitude)
										   ,latitude(li.latitude)
										   ,name(li.name)
										   ,type(li.type)
										   ,keyword(li.keyword) {}
	location_info(){}
	std::string id;
	double longitude;
	double latitude;
	std::string name; // From loc->name
	std::string type; // From loc->fmisid if it exists, otherwise from loc->geoid
	std::string keyword; // Keyword used to get this location
  };

  using SupportedLocations = std::map<std::string, location_info>; // id -> details
  using SupportedProducerLocations = std::map<std::string, SupportedLocations>; // producer -> locations

}  // namespace EDR
}  // namespace Plugin
}  // namespace SmartMet
