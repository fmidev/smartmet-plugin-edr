// ======================================================================
/*!
 * \brief Avi collection
 */
// ======================================================================

#pragma once

#include "AviMetaData.h"
#include <boost/optional.hpp>
#include <list>
#include <set>
#include <string>

namespace SmartMet
{
namespace Plugin
{
namespace EDR
{
class AviCollection
{
 public:
  const std::string &getName() const { return itsName; }
  const std::set<std::string> &getMessageTypes() const { return itsMessageTypes; }
  const std::set<std::string> &getCountries() const { return itsCountries; }
  const boost::optional<AviBBox> &getBBox() const { return itsBBox; }
  const std::set<std::string> &getIcaos() const { return itsIcaos; }
  const std::set<std::string> &getIcaoFilters() const { return itsIcaoFilters; }
  bool getLocationCheck() const { return itsLocationCheck; }
  int getPeriodLength() const { return itsPeriodLength; }

  void setName(const std::string &theName);
  void addMessageType(const std::string &theMessageType);
  void addCountry(const std::string &theCountry);
  void addIcao(const std::string &theIcao);
  void addIcaoFilter(const std::string &theIcaoFilter);
  void setBBox(const AviBBox &theBBox);
  void setLocationCheck(bool theLocationCheck) { itsLocationCheck = theLocationCheck; }
  void setPeriodLength(int thePeriodLength) { itsPeriodLength = thePeriodLength; }

  bool filter(const std::string &theIcao) const;

 private:
  std::string itsName;
  std::set<std::string> itsMessageTypes;
  std::set<std::string> itsCountries;
  boost::optional<AviBBox> itsBBox;
  std::set<std::string> itsIcaos;
  std::set<std::string> itsIcaoFilters;
  bool itsLocationCheck = false;
  int itsPeriodLength = 30; // 30 days by default
};  // class AviCollection

using AviCollections = std::list<AviCollection>;
const AviCollection& get_avi_collection(const std::string& producer, const AviCollections& aviCollections);

}  // namespace EDR
}  // namespace Plugin
}  // namespace SmartMet

// ======================================================================
