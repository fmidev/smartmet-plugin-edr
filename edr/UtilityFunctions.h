// ======================================================================
/*!
 * \brief Smartmet TimeSeries data functions
 */
// ======================================================================

#pragma once

#include "CommonQuery.h"
#include "Json.h"
#include "ObsParameter.h"
#include <timeseries/TimeSeriesInclude.h>

namespace SmartMet
{
namespace Plugin
{
namespace EDR
{
namespace UtilityFunctions
{
bool is_special_parameter(const std::string& paramname);
void get_special_parameter_values(const std::string& paramname,
                                  int precision,
                                  const TS::TimeSeriesGenerator::LocalTimeList& tlist,
                                  const Spine::LocationPtr& loc,
                                  const CommonQuery& query,
                                  const State& state,
                                  const Fmi::TimeZones& timezones,
                                  TS::TimeSeriesPtr& result);
void get_special_parameter_values(const std::string& paramname,
                                  int precision,
                                  const TS::TimeSeriesGenerator::LocalTimeList& tlist,
                                  const Spine::LocationList& llist,
                                  const CommonQuery& query,
                                  const State& state,
                                  const Fmi::TimeZones& timezones,
                                  TS::TimeSeriesGroupPtr& result);
bool is_mobile_producer(const std::string& producer);
bool is_flash_producer(const std::string& producer);
bool is_icebuoy_or_copernicus_producer(const std::string& producer);
bool is_flash_or_mobile_producer(const std::string& producer);
EDR::Json::Value json_value(const TS::Value& val, int precision);

bool parseRangeListValue(std::string& valueStr,
                         bool& isRange,
                         std::string& loValue,
                         std::string& hiValue);
std::vector<std::string> parseBBoxAndZ(
    const std::string& bbox, std::string& z, bool& zIsRange, std::string& zLo, std::string& zHi);
std::vector<std::string> parseBBox(const std::string& bbox, bool parse2d = false);

// Media type of an EDR output format (CoverageJSON, GeoJSON, IWXXM, IWXXMZIP, TAC)
std::string output_format_mime_type(const std::string& output_format);

// Check the number of values of an area of gridded data against request_limits.maxareaelements,
// 0 disables the check
void check_area_elements(std::size_t elements, std::size_t max_elements);

}  // namespace UtilityFunctions
}  // namespace EDR
}  // namespace Plugin
}  // namespace SmartMet

// ======================================================================
