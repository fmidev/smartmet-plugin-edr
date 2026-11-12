
#pragma once

#include <timeseries/DataFilter.h>
#include <timeseries/TimeSeriesInclude.h>
#include <map>
#include <set>
#include <string>

namespace SmartMet::Plugin::EDR
{
class State;
}  // namespace SmartMet::Plugin::EDR
namespace SmartMet::Spine::HTTP
{
class Request;
}  // namespace SmartMet::Spine::HTTP

namespace SmartMet
{
namespace Plugin
{
namespace EDR
{

struct ObsQueryParams
{
  ObsQueryParams(const Spine::HTTP::Request& req);
#ifndef WITHOUT_OBSERVATION
  std::string iot_producer_specifier;
  std::map<std::string, double> boundingBox;
  TS::DataFilter dataFilter;
  int numberofstations;
  bool numberofstationsOptionGiven;
  std::set<std::string> stationgroups;
  bool allplaces;
  bool latestObservation;
  bool useDataCache;
#endif
  static std::set<std::string> getObsProducers(const State& state);
};

}  // namespace EDR
}  // namespace Plugin
}  // namespace SmartMet
