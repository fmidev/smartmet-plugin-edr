// ======================================================================
/*!
 * \brief Structure for parameter precisions
 *
 */
// ======================================================================

#pragma once

#include <map>
#include <optional>
#include <string>

namespace SmartMet
{
namespace Plugin
{
namespace EDR
{
struct Precision
{
  using Map = std::map<std::string, int>;

  /**
   * @brief Default precision for parameters that do not have a specific precision defined in the config.
   *
   * This is also used as the default timeseries precision if default_timeseries_precision is not set.
   */
  int default_precision = 0;

  /**
   * @brief Optional default precision for timeseries queries.
   *
   * If not set, default_precision is used for timeseries queries as well.
   */
  std::optional<int> default_timeseries_precision = std::nullopt;

  /**
   * Map of parameter-specific precisions defined in the config.
   *
   * The key is the parameter name, and the value is the precision for that parameter.
   * If a parameter is not found in this map, default_precision is used. Additionally,
   * for timeseries queries, if a parameter is not found in this map, the code will check
   * ts_parameter_precisions_overrides for an override.
   */
  Map parameter_precisions;

  /**
   * Map of parameter-specific precision overrides for timeseries queries.
   *
   * The key is the parameter name, and the value is the precision for that parameter when
   * queried in timeseries mode. If a parameter is not found in this map, the code will fall
   * back to checking parameter_precisions for a general precision, and if not found there,
   * default_timeseries_precision if present and finally default_precision.
   */
  Map ts_parameter_precisions_overrides;

  Precision() = default;
};

}  // namespace EDR
}  // namespace Plugin
}  // namespace SmartMet
