#pragma once

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
struct location_info
{
  // Note: constuctor is not used for avi locations
  //
  location_info(const Spine::LocationPtr &loc, std::string k)
      : id(loc->fmisid ? Fmi::to_string(*loc->fmisid) : Fmi::to_string(loc->geoid)),
        longitude(loc->longitude),
        latitude(loc->latitude),
        name(loc->name),
        type(loc->fmisid ? "fmisid" : "geoid"),
        keyword(std::move(k))
  {
  }

  location_info() = default;

  std::string id;
  double longitude = 0.0;
  double latitude = 0.0;
  std::string name;     // From loc->name or station id for avi location (id is icao code)
  std::string type;     // From loc->fmisid if it exists, otherwise from loc->geoid or "avid"
  std::string keyword;  // Keyword used to get this location
};

using SupportedLocations = std::map<std::string, location_info>;  // id -> details
using SupportedProducerLocations =
    std::map<std::string, SupportedLocations>;  // producer -> locations

}  // namespace EDR
}  // namespace Plugin
}  // namespace SmartMet
