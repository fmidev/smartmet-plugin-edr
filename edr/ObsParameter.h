// ======================================================================
/*!
 * \brief Structure for Obsengine parameter
 *
 */
// ======================================================================

#pragma once

#include <timeseries/ParameterFactory.h>
#include <timeseries/TimeSeriesInclude.h>
#include <vector>

namespace SmartMet
{
namespace Plugin
{
namespace EDR
{
struct ObsParameter
{
  Spine::Parameter param;
  TS::DataFunctions functions;
  unsigned int data_column;
  bool duplicate;

  ObsParameter(Spine::Parameter p, TS::DataFunctions funcs, unsigned int dcol, bool dup)
      : param(std::move(p)), functions(funcs), data_column(dcol), duplicate(dup)
  {
  }
};

using ObsParameters = std::vector<ObsParameter>;
}  // namespace EDR
}  // namespace Plugin
}  // namespace SmartMet
