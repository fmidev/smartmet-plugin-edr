// ======================================================================
/*!
 * \brief Implementation of AviCollection
 */
// ======================================================================

#include <boost/algorithm/string.hpp>
#include <macgyver/StringConversion.h>
#include <stdexcept>

#include "AviCollection.h"

using namespace boost::algorithm;

namespace SmartMet
{
namespace Plugin
{
namespace EDR
{
void AviCollection::setName(const std::string &theName)
{
  auto name = trim_copy(to_upper_copy(theName));

  if (name.empty())
    throw std::runtime_error("collection name expected");

  itsName = name;
}

void AviCollection::addMessageType(const std::string &theMessageType)
{
  auto messageType = trim_copy(to_upper_copy(theMessageType));

  if (messageType.empty())
    throw std::runtime_error("message type expected");
  if (!itsMessageTypes.insert(messageType).second)
    throw std::runtime_error("value is duplicate");
}

void AviCollection::addCountry(const std::string &theCountry)
{
  auto country = trim_copy(to_upper_copy(theCountry));

  if (country.empty() || (country.length() != 2))
    throw std::runtime_error("iso2 country code expected");
  if (!itsCountries.insert(country).second)
    throw std::runtime_error("value is duplicate");
}

void AviCollection::setBBox(const AviBBox &theBBox)
{
  auto minX = *theBBox.getMinX();
  auto minY = *theBBox.getMinY();
  auto maxX = *theBBox.getMaxX();
  auto maxY = *theBBox.getMaxY();

  if ((minX < -180) || (minX >= 180))
    throw std::runtime_error("xmin is out of range [-180..180[");
  if ((minY < -90) || (minY >= 90))
    throw std::runtime_error("ymin is out of range [-90..90[");
  if ((maxX <= -180) || (maxX > 180))
    throw std::runtime_error("xmax is out of range ]-180..180]");
  if ((maxY <= -90) || (maxY > 90))
    throw std::runtime_error("ymax is out of range ]-90..90]");
  if (minX >= maxX)
    throw std::runtime_error("xmin must be less than xmax");
  if (minY >= maxY)
    throw std::runtime_error("ymin must be less than ymax");

  itsBBox = theBBox;
}

void AviCollection::addIcao(const std::string &theIcao)
{
  auto icao = trim_copy(to_upper_copy(theIcao));

  if (theIcao.length() != 4)
    throw std::runtime_error("4 letter icao code expected");
  if (!itsIcaos.insert(icao).second)
    throw std::runtime_error("value is duplicate");
}

void AviCollection::addIcaoFilter(const std::string &theIcaoFilter)
{
  auto icaoFilter = trim_copy(to_upper_copy(theIcaoFilter));
  auto length = theIcaoFilter.length();

  if ((length == 0) || (length > 4))
    throw std::runtime_error("1-4 letter icao code filter expected");

  for (auto const &filter : itsIcaoFilters)
  {
    auto length2 = filter.length();

    if (((length == length2) && (icaoFilter == filter)) ||
        ((length > length2) && (icaoFilter.substr(0, length2) == filter)) ||
        ((length < length2) && (filter.substr(0, length) == icaoFilter)))
      throw std::runtime_error("value is duplicate");
  }

  itsIcaoFilters.insert(icaoFilter);
}

bool AviCollection::filter(const std::string &theIcao) const
{
  auto icao = trim_copy(to_upper_copy(theIcao));
  auto length = icao.length();

  if (length != 4)
  {
    // This should not happen, icao codes read from avidb should have 4 letters.
    //
    // Anyway, filter the icao off

    return true;
  }

  for (auto const &filter : itsIcaoFilters)
  {
    auto length2 = filter.length();

    if (((length == length2) && (icao == filter)) ||
        ((length > length2) && (icao.substr(0, length2) == filter)))
      return true;
  }

  return false;
}

static AviCollection empty_avi_collection;

const AviCollection& get_avi_collection(const std::string& producer, const AviCollections& aviCollections)
{
  for(const auto& collection : aviCollections)
	{
	  if(producer == collection.getName())
		return collection;
	}
  return empty_avi_collection;
}

}  // namespace EDR
}  // namespace Plugin
}  // namespace SmartMet

// ======================================================================
