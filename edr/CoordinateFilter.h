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

  // Tolerance of coordinate comparisons, by default 1e-5 (degrees)
  void setTolerance(double tolerance);

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
  double itsTolerance = 1e-5;

  // The coordinates rounded to the tolerance, for finding them in constant time
  using Bucket = std::pair<long, long>;
  std::multimap<Bucket, LonLat> itsBuckets;

  void addPosition(const LonLat &lonlat);
  Bucket bucket(const LonLat &lonlat) const;

  // The added coordinate matching the given one within the tolerance
  std::optional<LonLat> match(const LonLat &lonlat) const;

  template <typename Map>
  typename Map::const_iterator find(const Map &map, const LonLat &lonlat) const
  {
    auto it = map.find(lonlat);
    if (it != map.end())
      return it;
    auto matched = match(lonlat);
    return (matched ? map.find(*matched) : map.end());
  }
};

}  // namespace EDR
}  // namespace Plugin
}  // namespace SmartMet
