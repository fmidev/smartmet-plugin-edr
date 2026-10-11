#pragma once

#include "Config.h"
#include "CoordinateFilter.h"
#include "EDRQuery.h"
#include "Links.h"
#include "State.h"
#include <macgyver/DateTime.h>
#include <spine/HTTP.h>
#include <set>

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

  // Times listed in the datetime parameter (a list or a repeating interval), the output
  // is limited to these times. Empty if the datetime is a single time or an interval.
  const std::set<Fmi::DateTime>& dateTimeList() const { return itsDateTimeList; }

  // Links of the response for the Link header, collected while processing the request
  Links responseLinks;

  std::vector<std::string> icaos;
  std::string requestWKT;
  std::string output_format;

  const std::string &mimeType() const { return mime_type; }

 protected:
  Spine::HTTP::Request req;  // this is used by Query
  static bool isAviProducer(const EDRProducerMetaData& avi_metadata, const std::string& producer);

 private:
  class CustomDimensions
  {
   public:
    CustomDimensions() = delete;
    CustomDimensions(const Spine::HTTP::Request& request);

    bool matches(const std::string &parameterName, const ParameterInfo &parameterInfo) const;

   private:
    std::vector<std::string> standard_names;
    std::vector<std::string> methods;
    std::vector<std::string> durations;
    std::vector<float> levels;
  };

  std::string parseEDRQuery(const State& state, const Config& config, const std::string& resource);
  std::string parseResourceParts2AndBeyond(const State& state,
                                           const std::vector<std::string>& resource_parts);
  std::string parseResourceParts3AndBeyond(const State& state,
                                           const std::vector<std::string>& resource_parts);
  std::string parseLocations(const State& state, const std::vector<std::string>& resource_parts);
  std::string parseInstances(const State& state, const std::vector<std::string>& resource_parts);
  void parseCoords(const EDRMetaData& emd, const std::string& coordinates);
  std::string parsePosition(const std::string& coords);
  std::string parseTrajectoryAndCorridor(const std::string& coords);
  void parseLocations(const EDRMetaData& emd, std::string& coords);
  void parseCube(const EDRMetaData& emd);
  void parseDateTime(const State& state, const std::string& producer, const EDRMetaData& emd);
  void parseDateTimeList(const std::string& datetime);
  void checkModelTimeStep(const EDRMetaData& emd, const Fmi::DateTime& t) const;
  void parseLimitAndOffset();
  void parseItemsQuery(const EDRMetaData& emd, const Config& config);
  void parseResolution(const EDRMetaData& emd, const Config& config);
  void parseResolutionZ(const EDRMetaData& emd, const Config& config);
  void parseCorridorResolution(std::size_t nx, std::size_t ny);
  std::string parseParameterNamesAndZ(const State& state,
                                      const EDRMetaData& emd,
                                      bool grid_producer,
                                      bool& noReqParams,
                                      const CustomDimensions& customDimensions);
  std::string parseParameterNames(const State& state,
                                  const EDRMetaData& emd,
                                  bool grid_producer,
                                  bool& noReqParams,
                                  const CustomDimensions& customDimensions,
                                  const std::string& z);
  std::string cleanParameterNames(const std::string& parameter_names,
                                  const EDRMetaData& emd,
                                  bool grid_producer,
                                  const std::string& z,
                                  const ParameterInfo &parameterInfo,
                                  const CustomDimensions& customDimensions) const;
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
  std::set<Fmi::DateTime> itsDateTimeList;
  std::string itsRequestXYWkt;  // the area or path in the requested CRS (x/y order)
  std::vector<double> itsInterpolatedLevels;  // resolution-z levels in the collection units
  std::string mime_type;
};

}  // namespace EDR
}  // namespace Plugin
}  // namespace SmartMet
