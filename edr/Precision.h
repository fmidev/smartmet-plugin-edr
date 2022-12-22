// ======================================================================
/*!
 * \brief Structure for parameter precisions
 *
 */
// ======================================================================

#pragma once

#include <map>
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

  int default_precision = 0;
  Map parameter_precisions;

  Precision() = default;
};

}  // namespace EDR
}  // namespace Plugin
}  // namespace SmartMet
