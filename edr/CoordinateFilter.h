#pragma once

#include <boost/date_time/posix_time/ptime.hpp>
#include <map>
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
  void add(double longitude, double latitude, const boost::posix_time::ptime &timestep);
  void add(double longitude,
           double latitude,
           double level,
           const boost::posix_time::ptime &timestep);
  bool accept(double longitude,
              double latitude,
              double level,
              const boost::posix_time::ptime &timestep) const;
  bool isEmpty() const;
  std::string getLevels() const;
  std::string getDatetime() const;

 private:
  using LonLat = std::pair<double, double>;
  using LevelTimestep = std::pair<double, boost::posix_time::ptime>;
  std::map<LonLat, std::set<double>> itsAllowedLevels;
  std::map<LonLat, std::set<boost::posix_time::ptime>> itsAllowedTimesteps;
  std::map<LonLat, std::set<LevelTimestep>> itsAllowedLevelsTimesteps;
};

}  // namespace EDR
}  // namespace Plugin
}  // namespace SmartMet
