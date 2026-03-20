// ======================================================================
/*!
 * \brief Query parameters for TimeSeries-style requests
 */
// ======================================================================

#pragma once

#include "CommonQuery.h"

namespace SmartMet
{
namespace Plugin
{
namespace EDR
{
class Config;

class TimeSeriesQuery : public CommonQuery
{
 public:
  TimeSeriesQuery() = delete;
  TimeSeriesQuery(const State& state, const Spine::HTTP::Request& req, Config& config);
};

}  // namespace EDR
}  // namespace Plugin
}  // namespace SmartMet

// ======================================================================
