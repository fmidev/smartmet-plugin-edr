#include "CoordinateFilter.h"
#include <boost/date_time/posix_time/posix_time.hpp>
#include <macgyver/StringConversion.h>

namespace SmartMet
{
namespace Plugin
{
namespace EDR
{
void CoordinateFilter::add(double longitude, double latitude, double level)
{
  LonLat lonlat(longitude, latitude);
  itsAllowedLevels[lonlat].insert(level);
}

void CoordinateFilter::add(double longitude,
                           double latitude,
                           const boost::posix_time::ptime &timestep)
{
  LonLat lonlat(longitude, latitude);
  itsAllowedTimesteps[lonlat].insert(timestep);
}

void CoordinateFilter::add(double longitude,
                           double latitude,
                           double level,
                           const boost::posix_time::ptime &timestep)
{
  LonLat lonlat(longitude, latitude);
  itsAllowedLevelsTimesteps[lonlat].insert(std::make_pair(level, timestep));
}

bool CoordinateFilter::isEmpty() const
{
  return (itsAllowedLevels.empty() && itsAllowedTimesteps.empty() &&
          itsAllowedLevelsTimesteps.empty());
}

// Get all levels
std::string CoordinateFilter::getLevels() const
{
  std::set<double> levels;
  if (!itsAllowedLevels.empty())
  {
    for (const auto &item : itsAllowedLevels)
    {
      levels.insert(item.second.begin(), item.second.end());
    }
  }
  else if (!itsAllowedLevelsTimesteps.empty())
  {
    for (const auto &coords : itsAllowedLevelsTimesteps)
    {
      for (const auto &item : coords.second)
      {
        levels.insert(item.first);
      }
    }
  }

  std::string ret;

  for (const auto &l : levels)
  {
    if (!ret.empty())
      ret.append(",");
    ret.append(Fmi::to_string(l));
  }

  return ret;
}

// Get datetime
std::string CoordinateFilter::getDatetime() const
{
  boost::posix_time::ptime starttime(boost::posix_time::not_a_date_time);
  boost::posix_time::ptime endtime(boost::posix_time::not_a_date_time);

  if (!itsAllowedTimesteps.empty())
  {
    for (const auto &item : itsAllowedTimesteps)
    {
      for (const auto &timestep : item.second)
      {
        auto timesteps = item.second;

        if (starttime.is_not_a_date_time())
        {
          starttime = timestep;
          endtime = timestep;
          continue;
        }

        if (timestep < starttime)
          starttime = timestep;
        if (timestep > endtime)
          endtime = timestep;
      }
    }
  }
  else if (!itsAllowedLevelsTimesteps.empty())
  {
    for (const auto &coords : itsAllowedLevelsTimesteps)
    {
      for (const auto &item : coords.second)
      {
        auto timestep = item.second;
        if (starttime.is_not_a_date_time())
        {
          starttime = timestep;
          endtime = timestep;
          continue;
        }

        if (timestep < starttime)
          starttime = timestep;
        if (timestep > endtime)
          endtime = timestep;
      }
    }
  }

  if (!starttime.is_not_a_date_time())
    return (boost::posix_time::to_iso_extended_string(starttime) + "Z/" +
            boost::posix_time::to_iso_extended_string(endtime) + "Z");

  return "";
}

bool CoordinateFilter::accept(double longitude,
                              double latitude,
                              double level,
                              const boost::posix_time::ptime &timestep) const
{
  if (isEmpty())
    return true;

  auto lonlat = LonLat(longitude, latitude);

  if (!itsAllowedLevelsTimesteps.empty())
  {
    if (itsAllowedLevelsTimesteps.find(lonlat) == itsAllowedLevelsTimesteps.end())
      return false;

    const auto &level_time_pairs = itsAllowedLevelsTimesteps.at(lonlat);

    for (const auto &item : level_time_pairs)
    {
      if (item.first == level && item.second == timestep)
        return true;
    }
  }
  else if (!itsAllowedLevels.empty())
  {
    if (itsAllowedLevels.find(lonlat) == itsAllowedLevels.end())
      return false;

    const auto &levels = itsAllowedLevels.at(lonlat);

    return (levels.find(level) != levels.end());
  }
  else if (!itsAllowedTimesteps.empty())
  {
    if (itsAllowedTimesteps.find(lonlat) == itsAllowedTimesteps.end())
      return false;

    const auto &timesteps = itsAllowedTimesteps.at(lonlat);

    return (timesteps.find(timestep) != timesteps.end());
  }

  return false;
}

}  // namespace EDR
}  // namespace Plugin
}  // namespace SmartMet
