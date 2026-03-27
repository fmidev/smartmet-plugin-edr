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
  format = Spine::optional_string(req.getParameter("format"), "ascii");

  // stepDefault=1.0, setDataTimesMode=false, no extra producer check
  commonInit(state, req, config);
}

void TimeSeriesQuery::remove_duplicate_parameters(std::list<std::string>& names) const
{
  // Remove duplicate parameters from the list. This is needed for timeseries queries, because
  // they can contain multiple parameters with the same name (e.g. parameter=temperature,parameter=temperature).
  // It is not currently implemented for EDR queries, One can move implementaton to the base class if needed later.
  // Let us not currently change the behavior of EDR queries.

  std::set<std::string> uniqueNames;
  names.remove_if([&uniqueNames](const std::string& name) {
    if (uniqueNames.find(name) != uniqueNames.end())
      return true;  // Duplicate found, remove it
    uniqueNames.insert(name);
    return false;  // Not a duplicate, keep it
  });
}

}  // namespace EDR
}  // namespace Plugin
}  // namespace SmartMet

// ======================================================================
