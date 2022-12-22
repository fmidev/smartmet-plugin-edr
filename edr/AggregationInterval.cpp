#include "AggregationInterval.h"
#include <iostream>

namespace SmartMet
{
namespace Plugin
{
namespace EDR
{
std::ostream &operator<<(std::ostream &out, const AggregationInterval &interval)
{
  out << "aggregation =\t[" << interval.behind << "..." << interval.ahead << "]\n";
  return out;
}

}  // namespace EDR
}  // namespace Plugin
}  // namespace SmartMet
