#pragma once

#include <macgyver/DateTime.h>
#include <map>
#include <optional>
#include <set>
#include <string>

namespace SmartMet
{
namespace Plugin
{
namespace EDR
{
class CoordinateFilter
{
 public:
  void add(double longitude, double latitude, double level);
  void add(double longitude, double latitude, const Fmi::DateTime &timestep);
  void add(double longitude, double latitude, double level, const Fmi::DateTime &timestep);
  bool accept(double longitude, double latitude, double level, const Fmi::DateTime &timestep) const;
  bool isEmpty() const;

  // Position of the coordinate in the order the coordinates were added (e.g. the position
  // along the requested trajectory), or nothing if the coordinate was not added
  std::optional<std::size_t> position(double longitude, double latitude) const;
  std::string getLevels() const;
  std::string getDatetime() const;

  using LonLat = std::pair<double, double>;
  using LevelTimestep = std::pair<double, Fmi::DateTime>;
  using AllowedLevels = std::map<LonLat, std::set<double>>;
  using AllowedTimesteps = std::map<LonLat, std::set<Fmi::DateTime>>;
  using AllowedLevelsTimesteps = std::map<LonLat, std::set<LevelTimestep>>;

 private:
  AllowedLevels itsAllowedLevels;
  AllowedTimesteps itsAllowedTimesteps;
  AllowedLevelsTimesteps itsAllowedLevelsTimesteps;
  std::map<LonLat, std::size_t> itsPositions;

  void addPosition(const LonLat &lonlat);
};

}  // namespace EDR
}  // namespace Plugin
}  // namespace SmartMet
