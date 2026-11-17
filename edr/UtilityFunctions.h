// ======================================================================
/*!
 * \brief Smartmet TimeSeries data functions
 */
// ======================================================================

#pragma once

#include "Json.h"
#include <spine/Location.h>
#include <timeseries/TimeSeries.h>
#include <timeseries/TimeSeriesGenerator.h>
#include <timeseries/TimeSeriesInclude.h>
#include <optional>
#include <set>
#include <string>
#include <vector>

namespace Fmi
{
class TimeZones;
}  // namespace Fmi
namespace SmartMet::Plugin::EDR
{
class CommonQuery;
class State;
}  // namespace SmartMet::Plugin::EDR

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

// Grid engine parameters carry the level in the last field of the parameter name
// (<param>:<producer>[:<geometry>[:<levelid>]][:<level>]) and each level of a parameter is
// fetched as a separate parameter. The level of such a column can only be told from its name,
// the separate 'level' column has no per parameter values.
std::optional<double> parse_parameter_level(const std::string& name, const std::set<int>& levels);

// Data of location independent parameters (e.g. the grid engine 'level' column) is returned as
// a plain time series even when the other columns of the same query are per coordinate groups.
// Such a column is handled as a group having data for the first coordinate only.
TS::TimeSeriesGroupPtr as_timeseries_group(const TS::TimeSeriesData& tsdata);

}  // namespace UtilityFunctions
}  // namespace EDR
}  // namespace Plugin
}  // namespace SmartMet

// ======================================================================
