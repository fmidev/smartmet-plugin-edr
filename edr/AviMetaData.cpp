// ======================================================================
/*!
 * \brief Implementation of AviMetaData
 */
// ======================================================================

#include "AviMetaData.h"
#include <macgyver/StringConversion.h>

namespace SmartMet
{
namespace Plugin
{
namespace EDR
{
void AviBBox::setField(uint index, double value)
{
  if (index == 0)
    setMinX(value);
  else if (index == 1)
    setMinY(value);
  else if (index == 2)
    setMaxX(value);
  else if (index == 3)
    setMaxY(value);
  else
    throw std::runtime_error("invalid bbox field index: " + Fmi::to_string(index));
}

std::map<std::string, std::optional<int>> AviMetaData::getGeometryIds() const
{
  std::map<std::string, std::optional<int>> geometryIds;

  for (const auto& station : itsStations)
    geometryIds.insert(make_pair(station.getIcao(), station.getFIRId()));

  return geometryIds;
}
}  // namespace EDR
}  // namespace Plugin
}  // namespace SmartMet

// ======================================================================
