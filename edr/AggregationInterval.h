// ======================================================================
/*!
 * \brief Structure for AggregationInterval
 *
 */
// ======================================================================

#pragma once

#include <iosfwd>
#include <map>

namespace SmartMet
{
namespace Plugin
{
namespace EDR
{
struct AggregationInterval
{
  unsigned int behind = 0;
  unsigned int ahead = 0;

  AggregationInterval() {}
  AggregationInterval(unsigned int be, unsigned int ah) : behind(be), ahead(ah) {}
  friend std::ostream &operator<<(std::ostream &out, const AggregationInterval &interval);
};

using MaxAggregationIntervals = std::map<std::string, AggregationInterval>;

std::ostream &operator<<(std::ostream &out, const AggregationInterval &interval);

}  // namespace EDR
}  // namespace Plugin
}  // namespace SmartMet
