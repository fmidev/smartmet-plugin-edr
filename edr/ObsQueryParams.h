
#pragma once

#include "State.h"
#include <spine/HTTP.h>

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
  std::set<std::string> stationgroups;
  bool allplaces;
  bool latestObservation;
  bool useDataCache;
#endif
  std::set<std::string> getObsProducers(const State& state) const;
};

}  // namespace EDR
}  // namespace Plugin
}  // namespace SmartMet

