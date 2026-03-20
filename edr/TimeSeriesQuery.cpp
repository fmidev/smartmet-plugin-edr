// ======================================================================
/*!
 * \brief Query parameters for TimeSeries-style requests
 */
// ======================================================================

#include "TimeSeriesQuery.h"
#include "Config.h"
#include <spine/Convenience.h>

namespace SmartMet
{
namespace Plugin
{
namespace EDR
{

TimeSeriesQuery::TimeSeriesQuery(const State& state,
                                 const Spine::HTTP::Request& req,
                                 Config& config)
    : CommonQuery(state, req, config)
{
  // format is parsed from the request param (unlike EDR which hardcodes "ascii")
  // commonInit will set it from "format" request parameter

  // stepDefault=1.0, setDataTimesMode=false, no extra producer check
  commonInit(state, req, config);
}

}  // namespace EDR
}  // namespace Plugin
}  // namespace SmartMet

// ======================================================================
