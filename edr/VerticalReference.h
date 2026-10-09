// ======================================================================
/*!
 * \brief Vertical reference system descriptions for the vertical level types
 *        reported by the querydata, grid and observation engines
 */
// ======================================================================

#pragma once

#include "Json.h"
#include <string>
#include <vector>

namespace SmartMet
{
namespace Plugin
{
namespace EDR
{
namespace VerticalReference
{
// Level type classification. Querydata and observation pressure levels are in hPa
// ("PressureLevel"), grid engine pressure levels in Pa ("PRESSURE").
enum class LevelKind
{
  PressureHPa,
  PressurePa,
  Hybrid,
  HeightAboveGround,
  AltitudeAboveMSL,
  Depth,
  Other
};

LevelKind kind(const std::string& level_type);

// WKT2 definition for the 'vrs' member of a collection's vertical extent
std::string wkt(const std::string& level_type);

// CoverageJSON VerticalCRS reference system object
Json::Value covjson_system(const std::string& level_type);

// Supported 'height-units' values for corridor queries
std::vector<std::string> height_units(const std::string& level_type);

// The unit of the levels of the collection, the only 'height-units' value of cube queries
std::string native_height_unit(const std::string& level_type);

}  // namespace VerticalReference
}  // namespace EDR
}  // namespace Plugin
}  // namespace SmartMet
