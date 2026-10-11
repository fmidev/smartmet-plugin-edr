#include "CoordinateFilter.h"
#include <macgyver/DateTime.h>
#include <macgyver/StringConversion.h>
#include <cmath>

namespace SmartMet
{
namespace Plugin
{
namespace EDR
{
namespace
{
void get_time_interval(const CoordinateFilter::AllowedTimesteps &timesteps,
                       Fmi::DateTime &starttime,
                       Fmi::DateTime &endtime)
{
  for (const auto &item : timesteps)
  {
    for (const auto &timestep : item.second)
    {
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

void get_time_interval(const CoordinateFilter::AllowedLevelsTimesteps &timesteps,
                       Fmi::DateTime &starttime,
                       Fmi::DateTime &endtime)
{
  for (const auto &coords : timesteps)
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

}  // namespace

void CoordinateFilter::add(double longitude, double latitude, double level)
{
  LonLat lonlat(longitude, latitude);
  itsAllowedLevels[lonlat].insert(level);
  addPosition(lonlat);
}

void CoordinateFilter::add(double longitude, double latitude, const Fmi::DateTime &timestep)
{
  LonLat lonlat(longitude, latitude);
  itsAllowedTimesteps[lonlat].insert(timestep);
  addPosition(lonlat);
}

void CoordinateFilter::add(double longitude,
                           double latitude,
                           double level,
                           const Fmi::DateTime &timestep)
{
  LonLat lonlat(longitude, latitude);
  itsAllowedLevelsTimesteps[lonlat].insert(std::make_pair(level, timestep));
  addPosition(lonlat);
}

// Coordinates are compared with a tolerance since the output coordinates may have been
// stored with a lower precision (e.g. as floats) than the requested coordinates

void CoordinateFilter::setTolerance(double tolerance)
{
  itsTolerance = tolerance;
  itsBuckets.clear();
  for (const auto &item : itsPositions)
    itsBuckets.insert(std::make_pair(bucket(item.first), item.first));
}

CoordinateFilter::Bucket CoordinateFilter::bucket(const LonLat &lonlat) const
{
  return {std::lround(std::floor(lonlat.first / itsTolerance)),
          std::lround(std::floor(lonlat.second / itsTolerance))};
}

void CoordinateFilter::addPosition(const LonLat &lonlat)
{
  // The first occurrence of a coordinate decides
  if (itsPositions.insert(std::make_pair(lonlat, itsPositions.size())).second)
    itsBuckets.insert(std::make_pair(bucket(lonlat), lonlat));
}

std::optional<CoordinateFilter::LonLat> CoordinateFilter::match(const LonLat &lonlat) const
{
  // A coordinate within the tolerance is in the same or a neighbouring bucket
  const auto center = bucket(lonlat);
  for (long dx = -1; dx <= 1; dx++)
    for (long dy = -1; dy <= 1; dy++)
    {
      auto range = itsBuckets.equal_range(Bucket(center.first + dx, center.second + dy));
      for (auto it = range.first; it != range.second; ++it)
        if (std::abs(it->second.first - lonlat.first) < itsTolerance &&
            std::abs(it->second.second - lonlat.second) < itsTolerance)
          return it->second;
    }
  return {};
}

std::optional<std::size_t> CoordinateFilter::position(double longitude, double latitude) const
{
  auto it = find(itsPositions, LonLat(longitude, latitude));
  if (it == itsPositions.end())
    return {};
  return it->second;
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
  Fmi::DateTime starttime(Fmi::DateTime::NOT_A_DATE_TIME);
  Fmi::DateTime endtime(Fmi::DateTime::NOT_A_DATE_TIME);

  if (!itsAllowedTimesteps.empty())
    get_time_interval(itsAllowedTimesteps, starttime, endtime);
  else if (!itsAllowedLevelsTimesteps.empty())
    get_time_interval(itsAllowedLevelsTimesteps, starttime, endtime);

  if (!starttime.is_not_a_date_time())
    return (Fmi::date_time::to_iso_extended_string(starttime) + "Z/" +
            Fmi::date_time::to_iso_extended_string(endtime) + "Z");

  return {};
}

bool CoordinateFilter::accept(double longitude,
                              double latitude,
                              double level,
                              const Fmi::DateTime &timestep) const
{
  if (isEmpty())
    return true;

  auto lonlat = LonLat(longitude, latitude);

  if (!itsAllowedLevelsTimesteps.empty())
  {
    auto it = find(itsAllowedLevelsTimesteps, lonlat);
    if (it == itsAllowedLevelsTimesteps.end())
      return false;

    const auto &level_time_pairs = it->second;

    for (const auto &item : level_time_pairs)
    {
      if (item.first == level && item.second == timestep)
        return true;
    }
  }
  else if (!itsAllowedLevels.empty())
  {
    auto it = find(itsAllowedLevels, lonlat);
    if (it == itsAllowedLevels.end())
      return false;

    const auto &levels = it->second;

    return (levels.find(level) != levels.end());
  }
  else if (!itsAllowedTimesteps.empty())
  {
    auto it = find(itsAllowedTimesteps, lonlat);
    if (it == itsAllowedTimesteps.end())
      return false;

    const auto &timesteps = it->second;

    return (timesteps.find(timestep) != timesteps.end());
  }

  return false;
}

}  // namespace EDR
}  // namespace Plugin
}  // namespace SmartMet
