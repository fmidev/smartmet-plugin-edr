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
  std::string parseEDRQuery(const State& state, const std::string& resource);
  std::string parseResourceParts2AndBeyond(const State& state, const std::vector<std::string>& resource_parts);
  std::string parseResourceParts3AndBeyond(const State& state, const std::vector<std::string>& resource_parts);
  std::string parseLocations(const State& state, const std::vector<std::string>& resource_parts);
  std::string parseInstances(const State& state, const std::vector<std::string>& resource_parts);
  void parseCoords(const std::string& coordinates);
  std::string parsePosition(const std::string& coords);
  std::string parseTrajectoryAndCorridor(const std::string& coords);
  void parseLocations(const EDRMetaData& emd);
  void parseCube();
  void parseDateTime(const State& state, const EDRMetaData& emd);
  std::string parseParameterNamesAndZ(const State& state, const EDRMetaData& emd, bool grid_producer);
  std::string cleanParameterNames(const std::string& parameter_names, const EDRMetaData& emd, bool grid_producer, const std::string& z) const;
  void handleGridParameter(std::string& p, const std::string& producerId, const std::string& geometryId, const std::string& levelId, const std::string& z) const;

  void parseICAOCodesAndAviProducer(const EDRMetaData& emd);

  EDRQuery itsEDRQuery;
  CoordinateFilter itsCoordinateFilter;
};

}  // namespace EDR
}  // namespace Plugin
}  // namespace SmartMet

