#pragma once

#include "EDRQuery.h"
#include "Config.h"
#include "State.h"
#include "CoordinateFilter.h"
#include <spine/HTTP.h>

namespace SmartMet
{
namespace Plugin
{
namespace EDR
{

struct EDRMetaData;
using EDRProducerMetaData = std::map<std::string, std::vector<EDRMetaData>>;

class EDRQueryParams
{
public:
  EDRQueryParams(const State& state, const Spine::HTTP::Request& request, const Config& config);

  const EDRQuery &edrQuery() const { return itsEDRQuery; }
  bool isEDRMetaDataQuery() const { return itsEDRQuery.query_id != EDRQueryId::DataQuery; }
  const CoordinateFilter &coordinateFilter() const { return itsCoordinateFilter; }

  std::vector<std::string> icaos;
  std::string requestWKT;
  std::string output_format;

protected:
  Spine::HTTP::Request req; // this is used by Query
  bool isAviProducer(const EDRProducerMetaData &avi_metadata, const std::string &producer) const;
private:
  EDRQuery itsEDRQuery;
  CoordinateFilter itsCoordinateFilter;
};

}  // namespace EDR
}  // namespace Plugin
}  // namespace SmartMet

