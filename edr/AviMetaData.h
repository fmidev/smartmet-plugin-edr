// ======================================================================
/*!
 * \brief Avi metadata
 */
// ======================================================================

#pragma once

#include <optional>
#include <set>
#include <string>
#include <vector>

#include <engines/avi/Engine.h>

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

  std::optional<double> getMinX() const { return minX; }
  std::optional<double> getMinY() const { return minY; }
  std::optional<double> getMaxX() const { return maxX; }
  std::optional<double> getMaxY() const { return maxY; }

  void setField(uint index, double value);
  void setMinX(double xMin) { minX = xMin; }
  void setMinY(double yMin) { minY = yMin; }
  void setMaxX(double xMax) { maxX = xMax; }
  void setMaxY(double yMax) { maxY = yMax; }

 private:
  std::optional<double> minX;
  std::optional<double> minY;
  std::optional<double> maxX;
  std::optional<double> maxY;
};

class AviStation
{
 public:
  AviStation(long theId,
             std::string theIcao,
             std::string theName,
             double theLatitude,
             double theLongitude,
             std::optional<int> firId)
      : itsId(theId),
        itsIcao(std::move(theIcao)),
        itsName(std::move(theName)),
        itsLatitude(theLatitude),
        itsLongitude(theLongitude),
        itsFIRId(firId)
  {
  }
  AviStation() = delete;

  long getId() const { return itsId; }
  const std::string &getIcao() const { return itsIcao; }
  const std::string &getName() const { return itsName; }
  double getLatitude() const { return itsLatitude; }
  double getLongitude() const { return itsLongitude; }
  std::optional<int> getFIRId() const { return itsFIRId; }

 private:
  long itsId;
  std::string itsIcao;
  std::string itsName;
  double itsLatitude;
  double itsLongitude;
  std::optional<int> itsFIRId;
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
  AviMetaData(std::optional<AviBBox> theBBox,
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

  const std::optional<AviBBox> &getBBox() const { return itsBBox; }
  std::map<std::string, std::optional<int>> getGeometryIds() const;
  const std::string &getProducer() const { return itsProducer; }
  const std::vector<AviStation> &getStations() const { return itsStations; }
  const std::vector<AviParameter> &getParameters() const { return itsParameters; }
  const std::set<std::string> &getMessageTypes() const { return itsMessageTypes; }
  bool getLocationCheck() const { return itsLocationCheck; }

  void setBBox(const AviBBox &theBBox) { itsBBox = theBBox; }
  const Engine::Avi::FIRQueryData *getFIRAreas() const { return itsFIRAreas; }
  void setFIRAreas(const SmartMet::Engine::Avi::FIRQueryData &firAreas) { itsFIRAreas = &firAreas; }
  void addStation(const AviStation &theStation) { itsStations.push_back(theStation); }

 private:
  std::optional<AviBBox> itsBBox;
  std::string itsProducer;
  std::vector<AviStation> itsStations;
  std::vector<AviParameter> itsParameters;
  std::set<std::string> itsMessageTypes;
  bool itsLocationCheck;

  const Engine::Avi::FIRQueryData *itsFIRAreas = nullptr;
};

}  // namespace EDR
}  // namespace Plugin
}  // namespace SmartMet

// ======================================================================
