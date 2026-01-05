#pragma once

#include "Config.h"
#include "CoordinateFilter.h"
#include "EDRQuery.h"
#include "State.h"
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

  const EDRQuery& edrQuery() const { return itsEDRQuery; }
  bool isEDRMetaDataQuery() const { return itsEDRQuery.query_id != EDRQueryId::DataQuery; }
  const CoordinateFilter& coordinateFilter() const { return itsCoordinateFilter; }

  std::vector<std::string> icaos;
  std::string requestWKT;
  std::string output_format;

 protected:
  Spine::HTTP::Request req;  // this is used by Query
  static bool isAviProducer(const EDRProducerMetaData& avi_metadata, const std::string& producer);

 private:
  std::string parseEDRQuery(const State& state, const Config& config, const std::string& resource);
  std::string parseResourceParts2AndBeyond(const State& state,
                                           const std::vector<std::string>& resource_parts);
  std::string parseResourceParts3AndBeyond(const State& state,
                                           const std::vector<std::string>& resource_parts);
  std::string parseLocations(const State& state, const std::vector<std::string>& resource_parts);
  std::string parseInstances(const State& state, const std::vector<std::string>& resource_parts);
  void parseCoords(const std::string& coordinates);
  std::string parsePosition(const std::string& coords);
  std::string parseTrajectoryAndCorridor(const std::string& coords);
  void parseLocations(const EDRMetaData& emd, std::string& coords);
  void parseCube();
  void parseDateTime(const State& state, const std::string& producer, const EDRMetaData& emd);
  std::string parseParameterNamesAndZ(const State& state,
                                      const EDRMetaData& emd,
                                      bool grid_producer,
                                      bool& noReqParams);
  std::string cleanParameterNames(const std::string& parameter_names,
                                  const EDRMetaData& emd,
                                  bool grid_producer,
                                  const std::string& z) const;
  static void handleGridParameter(std::string& p,
                                  const std::string& producerId,
                                  const std::string& geometryId,
                                  const std::string& levelId,
                                  const std::string& z);

  void parseICAOCodesAndAviProducer(const EDRMetaData& emd);

  void validateRequestParametersWithMetaData(const EDRMetaData& emd) const;
  void validateRequestParameterNamesWithMetaData(const EDRMetaData& emd) const;
  void validateRequestDateTimeWithMetaData(const EDRMetaData& emd) const;
  void validateRequestLevelsWithMetaData(const EDRMetaData& emd) const;
  void validateRequestCoordinatesWithMetaData(const EDRMetaData& emd) const;

  EDRQuery itsEDRQuery;
  CoordinateFilter itsCoordinateFilter;
};

}  // namespace EDR
}  // namespace Plugin
}  // namespace SmartMet
