// ======================================================================
/*!
 * \brief Avi metadata
 */
// ======================================================================

#pragma once

#include <boost/optional.hpp>
#include <set>
#include <string>
#include <vector>

namespace SmartMet
{
namespace Plugin
{
namespace EDR
{
class AviBBox
{
 public:
  AviBBox() = default;
  AviBBox(double xMin, double yMin, double xMax, double yMax)
      : minX(xMin), minY(yMin), maxX(xMax), maxY(yMax)
  {
  }

  boost::optional<double> getMinX() const { return minX; }
  boost::optional<double> getMinY() const { return minY; }
  boost::optional<double> getMaxX() const { return maxX; }
  boost::optional<double> getMaxY() const { return maxY; }

  void setField(uint index, double value);
  void setMinX(double xMin) { minX = xMin; }
  void setMinY(double yMin) { minY = yMin; }
  void setMaxX(double xMax) { maxX = xMax; }
  void setMaxY(double yMax) { maxY = yMax; }

 private:
  boost::optional<double> minX;
  boost::optional<double> minY;
  boost::optional<double> maxX;
  boost::optional<double> maxY;
};

class AviStation
{
 public:
  AviStation(long theId, std::string theIcao, double theLatitude, double theLongitude)
      : itsId(theId),
        itsIcao(std::move(theIcao)),
        itsLatitude(theLatitude),
        itsLongitude(theLongitude)
  {
  }
  AviStation() = delete;

  long getId() const { return itsId; }
  const std::string &getIcao() const { return itsIcao; }
  double getLatitude() const { return itsLatitude; }
  double getLongitude() const { return itsLongitude; }

 private:
  long itsId;
  std::string itsIcao;
  double itsLatitude;
  double itsLongitude;
};

class AviParameter
{
 public:
  AviParameter(std::string theName, std::string theDescription)
      : itsName(std::move(theName)), itsDescription(std::move(theDescription))
  {
  }

  const std::string &getName() const { return itsName; }
  const std::string &getDescription() const { return itsDescription; }

 private:
  std::string itsName;
  std::string itsDescription;
};

class AviMetaData
{
 public:
  AviMetaData(boost::optional<AviBBox> theBBox,
              std::string theProducer,
              std::set<std::string> theMessageTypes,
              bool theLocationCheck)
      : itsBBox(std::move(theBBox)),
        itsProducer(std::move(theProducer)),
        itsMessageTypes(std::move(theMessageTypes)),
        itsLocationCheck(theLocationCheck)
  {
    itsParameters.emplace_back("message", "Aviation message");
  }
  AviMetaData() = delete;

  const boost::optional<AviBBox> &getBBox() const { return itsBBox; }
  const std::string &getProducer() const { return itsProducer; }
  const std::vector<AviStation> &getStations() const { return itsStations; }
  const std::vector<AviParameter> &getParameters() const { return itsParameters; }
  const std::set<std::string> &getMessageTypes() const { return itsMessageTypes; }
  bool getLocationCheck() const { return itsLocationCheck; }

  void setBBox(const AviBBox &theBBox) { itsBBox = theBBox; }
  void addStation(const AviStation &theStation) { itsStations.push_back(theStation); }

 private:
  boost::optional<AviBBox> itsBBox;
  std::string itsProducer;
  std::vector<AviStation> itsStations;
  std::vector<AviParameter> itsParameters;
  std::set<std::string> itsMessageTypes;
  bool itsLocationCheck;
};

}  // namespace EDR
}  // namespace Plugin
}  // namespace SmartMet

// ======================================================================
