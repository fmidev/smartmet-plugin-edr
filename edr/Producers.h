// ======================================================================

#pragma once

#include <list>
#include <string>

namespace SmartMet {
namespace Plugin {
namespace EDR {
using AreaProducers = std::list<std::string>;
using TimeProducers = std::list<AreaProducers>;

} // namespace EDR
} // namespace Plugin
} // namespace SmartMet
