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

 protected:
  void remove_duplicate_parameters(std::list<std::string>& names) const override;
};

}  // namespace EDR
}  // namespace Plugin
}  // namespace SmartMet

// ======================================================================
