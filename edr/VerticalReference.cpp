#include "VerticalReference.h"
#include <boost/algorithm/string/predicate.hpp>

namespace SmartMet
{
namespace Plugin
{
namespace EDR
{
namespace VerticalReference
{
LevelKind kind(const std::string& level_type)
{
  using boost::algorithm::iequals;

  // Grid engine (GRIB) level types are upper case and pressures are in Pa
  if (level_type == "PRESSURE")
    return LevelKind::PressurePa;
  if (iequals(level_type, "PressureLevel"))
    return LevelKind::PressureHPa;
  if (iequals(level_type, "HybridLevel") || iequals(level_type, "HYBRID"))
    return LevelKind::Hybrid;
  if (iequals(level_type, "Height"))
    return LevelKind::HeightAboveGround;
  if (iequals(level_type, "Altitude"))
    return LevelKind::AltitudeAboveMSL;
  if (iequals(level_type, "Depth"))
    return LevelKind::Depth;
  return LevelKind::Other;
}

std::string wkt(const std::string& level_type)
{
  switch (kind(level_type))
  {
    case LevelKind::PressureHPa:
      return "PARAMETRICCRS[\"Pressure\",PDATUM[\"Isobaric surfaces\"],CS[parametric,1],"
             "AXIS[\"pressure (hPa)\",down],PARAMETRICUNIT[\"hectopascal\",100.0]]";
    case LevelKind::PressurePa:
      return "PARAMETRICCRS[\"Pressure\",PDATUM[\"Isobaric surfaces\"],CS[parametric,1],"
             "AXIS[\"pressure (Pa)\",down],PARAMETRICUNIT[\"pascal\",1.0]]";
    case LevelKind::Hybrid:
      return "PARAMETRICCRS[\"Model level\",PDATUM[\"Hybrid sigma-pressure model levels\"],"
             "CS[parametric,1],AXIS[\"model level number (n)\",down],"
             "PARAMETRICUNIT[\"unity\",1.0]]";
    case LevelKind::HeightAboveGround:
      return "VERTCRS[\"Height above ground\",VDATUM[\"Ground surface\"],CS[vertical,1],"
             "AXIS[\"gravity-related height (H)\",up],LENGTHUNIT[\"metre\",1.0]]";
    case LevelKind::AltitudeAboveMSL:
      return "VERTCRS[\"MSL height\",VDATUM[\"Mean Sea Level\"],CS[vertical,1],"
             "AXIS[\"gravity-related height (H)\",up],LENGTHUNIT[\"metre\",1.0],"
             "ID[\"EPSG\",5714]]";
    case LevelKind::Depth:
      return "VERTCRS[\"Depth below surface\",VDATUM[\"Surface\"],CS[vertical,1],"
             "AXIS[\"depth (D)\",down],LENGTHUNIT[\"metre\",1.0]]";
    case LevelKind::Other:
      break;
  }
  return "PARAMETRICCRS[\"" + level_type + "\",PDATUM[\"" + level_type +
         "\"],CS[parametric,1],AXIS[\"level (L)\",up],PARAMETRICUNIT[\"unity\",1.0]]";
}

Json::Value covjson_system(const std::string& level_type)
{
  auto system = Json::Value(Json::ValueType::objectValue);
  system["type"] = Json::Value("VerticalCRS");

  std::string description;
  switch (kind(level_type))
  {
    case LevelKind::PressureHPa:
      description = "Pressure level in hPa";
      break;
    case LevelKind::PressurePa:
      description = "Pressure level in Pa";
      break;
    case LevelKind::Hybrid:
      description = "Hybrid model level number";
      break;
    case LevelKind::HeightAboveGround:
      description = "Height above ground in metres";
      break;
    case LevelKind::AltitudeAboveMSL:
      system["id"] = Json::Value("http://www.opengis.net/def/crs/EPSG/0/5714");
      description = "Height above mean sea level in metres";
      break;
    case LevelKind::Depth:
      description = "Depth below surface in metres";
      break;
    case LevelKind::Other:
      description = level_type;
      break;
  }

  if (!description.empty())
  {
    system["description"] = Json::Value(Json::ValueType::objectValue);
    system["description"]["en"] = Json::Value(description);
  }
  return system;
}

std::vector<std::string> height_units(const std::string& level_type)
{
  switch (kind(level_type))
  {
    case LevelKind::PressureHPa:
    case LevelKind::PressurePa:
      return {"hPa", "mbar", "Pa"};
    case LevelKind::Hybrid:
      return {"1"};
    default:
      return {"m", "km", "mi", "ft"};
  }
}

std::string native_height_unit(const std::string& level_type)
{
  switch (kind(level_type))
  {
    case LevelKind::PressureHPa:
      return "hPa";
    case LevelKind::PressurePa:
      return "Pa";
    case LevelKind::Hybrid:
      return "1";
    default:
      return "m";
  }
}

}  // namespace VerticalReference
}  // namespace EDR
}  // namespace Plugin
}  // namespace SmartMet
