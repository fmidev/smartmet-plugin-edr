// ======================================================================
/*!
 * \brief Structures for query level cache. From QEngine we fetch results
 * parameter by parameter. During the query parameters are stored in cache,
 * to prevent multiple queries per parameter, e.g. t2m, min_t(t2m:1h)
 *
 */
// ======================================================================

#pragma once

namespace SmartMet {
namespace Plugin {
namespace EDR {
struct QueryLevelDataCache {
  using PressureLevelParameterPair = std::pair<float, std::string>;
  using ParameterTimeSeriesMap =
      std::map<PressureLevelParameterPair, TS::TimeSeriesPtr>;
  using ParameterTimeSeriesGroupMap =
      std::map<PressureLevelParameterPair, TS::TimeSeriesGroupPtr>;

  ParameterTimeSeriesMap itsTimeSeries;
  ParameterTimeSeriesGroupMap itsTimeSeriesGroups;
};

} // namespace EDR
} // namespace Plugin
} // namespace SmartMet
