#pragma once

#include "CollectionInfo.h"
#include "EDRDefs.h"
#include "Engines.h"
#include "LocationInfo.h"
#include "ParameterInfo.h"
#include <macgyver/DateTime.h>
#include <list>
#include <map>
#include <optional>
#include <set>
#include <string>
#ifndef WITHOUT_OBSERVATION
#include <engines/observation/MetaData.h>
#include <engines/observation/ObservableProperty.h>
#endif
#include "Json.h"

namespace SmartMet
{
namespace Plugin
{
namespace EDR
{

static const std::string METAR("metar");
static const std::string SIGMET("sigmet");

// Parameter info from querydata-, observation-, grid-engine
struct edr_parameter
{
  edr_parameter(std::string n, std::string d) : name(std::move(n)), description(std::move(d)) {}
  edr_parameter(std::string n, std::string d, std::string u)
      : name(std::move(n)), description(std::move(d)), unit(std::move(u))
  {
  }
  edr_parameter(std::string n, std::string d, std::string u, std::string lbl)
      : name(std::move(n)), description(std::move(d)), unit(std::move(u)), label(std::move(lbl))
  {
  }
  std::string name;
  std::string description;
  std::string unit;
  std::string label;
};

struct edr_spatial_extent
{
  edr_spatial_extent() : crs("EPSG:4326"), geometryIds({}) {}

  double bbox_xmin = 0;
  double bbox_ymin = 0;
  double bbox_xmax = 0;
  double bbox_ymax = 0;
  std::string crs;
  std::map<std::string, std::optional<int>> geometryIds;
};

struct edr_temporal_extent_period
{
  edr_temporal_extent_period()
      : start_time(Fmi::DateTime::NOT_A_DATE_TIME), end_time(Fmi::DateTime::NOT_A_DATE_TIME)
  {
  }
  Fmi::DateTime start_time;
  Fmi::DateTime end_time;
  int timestep = 0;
  int timesteps = 0;
};

struct edr_temporal_extent
{
  edr_temporal_extent()
      : origin_time(Fmi::DateTime::NOT_A_DATE_TIME), trs("Temporal Reference System")
  {
  }
  Fmi::DateTime origin_time;
  std::string trs;
  std::vector<edr_temporal_extent_period> time_periods;
  std::vector<edr_temporal_extent_period> single_time_periods;
  std::set<int> time_steps;
};

using StationTemporalExtentMetaData = std::map<std::string, edr_temporal_extent>;

struct edr_vertical_extent
{
  edr_vertical_extent() : vrs("Vertical Reference System") {}
  std::string vrs;
  std::string level_type;
  std::vector<std::string> levels;
  Engine::Observation::ObsLevelType obs_level_type;
  bool is_level_range = false;
};

struct EDRMetaData
{
  edr_spatial_extent spatial_extent;
  edr_temporal_extent temporal_extent;
  edr_vertical_extent vertical_extent;

  Engine::Observation::StationMetaData stationMetaData;
  StationTemporalExtentMetaData stationTemporalExtentMetaData;

  std::set<std::string> parameter_names;
  std::map<std::string, edr_parameter> parameters;
  std::map<std::string, int> parameter_precisions;
  std::set<std::string> data_queries;    // Supported data_queries, defined in config file
  std::set<std::string> output_formats;  // Supported output_formats, defined in config file
  std::string default_output_format;     // Default output format, defined in config file
  const SupportedLocations* locations = nullptr;    // Supported locations, default keyword synop_fi
                                                    // can be overwritten in configuration file
  const ParameterInfo* parameter_info = nullptr;    // Info about parameters from config file
  const CollectionInfo* collection_info = nullptr;  // Info about collections from config file
  CollectionInfo collection_info_engine;            // Info about colections from engine
  std::string language = "en";                      // Language from configuration file
  SourceEngine metadata_source = SourceEngine::Undefined;
  Fmi::DateTime latest_data_update_time;
  int getPrecision(const std::string& parameter_name) const;
  bool isObsProducer() const { return metadata_source == SourceEngine::Observation; }
  bool isGridProducer() const { return metadata_source == SourceEngine::Grid; }
  bool isAviProducer() const { return metadata_source == SourceEngine::Avi; }
  bool sourceHasInstances() const
  {
    return !(data_queries.empty() || isObsProducer() || isAviProducer());
  }

  const Engine::Avi::Engine* aviEngine = nullptr;
  bool getGeometry(const std::string& item, Json::Value& geometry) const;
};

using EDRProducerMetaData =
    std::map<std::string, std::vector<EDRMetaData>>;  // producer-> meta data

EDRProducerMetaData get_edr_metadata_qd(const Engine::Querydata::Engine& qEngine,
                                        const ProducerLicenses& licenses,
                                        const std::string& default_language,
                                        const ParameterInfo* pinfo,
                                        const CollectionInfoContainer& cic,
                                        const SupportedDataQueries& sdq,
                                        const SupportedOutputFormats& sofs,
                                        const DefaultOutputFormats& defs,
                                        const SupportedProducerLocations& spl);
EDRProducerMetaData get_edr_metadata_grid(const Engine::Grid::Engine& gEngine,
                                          const ProducerLicenses& licenses,
                                          const std::string& default_language,
                                          const ParameterInfo* pinfo,
                                          const CollectionInfoContainer& cic,
                                          const SupportedDataQueries& sdq,
                                          const SupportedOutputFormats& sofs,
                                          const DefaultOutputFormats& defs,
                                          const SupportedProducerLocations& spl);
#ifndef WITHOUT_OBSERVATION
EDRProducerMetaData get_edr_metadata_obs(
    Engine::Observation::Engine& obsEngine,
    const ProducerLicenses& licenses,
    const std::string& default_language,
    const ParameterInfo* pinfo,
    const std::map<std::string, const SmartMet::Engine::Observation::ObservableProperty*>&
        observable_properties,
    const CollectionInfoContainer& cic,
    const SupportedDataQueries& sdq,
    const SupportedOutputFormats& sofs,
    const DefaultOutputFormats& defs,
    const SupportedProducerLocations& spl,
    const ProducerParameters& prodParams,
    unsigned int observation_period);
#endif
#ifndef WITHOUT_AVI
class Config;
EDRProducerMetaData get_edr_metadata_avi(const Engine::Avi::Engine& aviEngine,
                                         const Config& config,
                                         const ProducerLicenses& licenses,
                                         const std::string& default_language,
                                         const ParameterInfo* pinfo,
                                         const CollectionInfoContainer& cic,
                                         const SupportedDataQueries& sdq,
                                         const SupportedOutputFormats& sofs,
                                         const DefaultOutputFormats& defs,
                                         const SupportedProducerLocations& spl);

void load_locations_avi(const Engine::Avi::Engine& aviEngine,
                        const AviCollections& aviCollections,
                        SupportedProducerLocations& spl,
                        const CollectionInfoContainer& cic);
#endif

const Fmi::DateTime& get_latest_data_update_time(const EDRProducerMetaData& pmd,
                                                 const std::string& producer);

class EngineMetaData
{
 public:
  EngineMetaData();
  void addMetaData(SourceEngine source_engine, const EDRProducerMetaData& metadata);
  const EDRProducerMetaData& getMetaData(SourceEngine source_engine) const;
  const std::map<SourceEngine, EDRProducerMetaData>& getMetaData() const;
  const std::time_t& getMetaDataUpdateTime() const { return itsMetaDataUpdateTime; }
  const Fmi::DateTime& getLatestDataUpdateTime(SourceEngine source_engine,
                                               const std::string& producer) const;
  void setLatestDataUpdateTime(SourceEngine source_engine,
                               const std::string& producer,
                               const Fmi::DateTime& t);
  bool isValidCollection(const std::string& collection_name) const;
  bool isValidCollection(SourceEngine source_engine, const std::string& collection_name) const;
  void removeDuplicates(bool report_removal);

 private:
  std::map<SourceEngine, EDRProducerMetaData> itsMetaData;  // Source engine -> metadata
  std::time_t itsMetaDataUpdateTime;
};

}  // namespace EDR
}  // namespace Plugin
}  // namespace SmartMet
