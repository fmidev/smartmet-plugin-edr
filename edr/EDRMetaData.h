#pragma once

#include "CollectionInfo.h"
#include "EDRDefs.h"
#include "Engines.h"
#include "LocationInfo.h"
#include "ParameterInfo.h"
#include <optional>
#include <macgyver/DateTime.h>
#include <list>
#include <map>
#include <set>
#include <string>
#ifndef WITHOUT_OBSERVATION
#include <engines/observation/ObservableProperty.h>
#endif

namespace SmartMet
{
namespace Plugin
{
namespace EDR
{

// Parameter info from querydata-, observation-, grid-engine
struct edr_parameter
{
  edr_parameter(std::string n, std::string d) : name(std::move(n)), description(std::move(d)) {}
  edr_parameter(std::string n, std::string d, std::string u)
      : name(std::move(n)), description(std::move(d)), unit(std::move(u))
  {
  }
  edr_parameter(std::string n, std::string d, std::string u, std::string l)
      : name(std::move(n)), description(std::move(d)), unit(std::move(u)), label(std::move(l))
  {
  }
  std::string name;
  std::string description;
  std::string unit;
  std::string label;
};

struct edr_spatial_extent
{
  edr_spatial_extent()
      : bbox_xmin(0.0), bbox_ymin(0.0), bbox_xmax(0.0), bbox_ymax(0.0), crs("EPSG:4326")
  {
  }
  double bbox_xmin;
  double bbox_ymin;
  double bbox_xmax;
  double bbox_ymax;
  std::string crs;
};

struct edr_temporal_extent_period
{
  edr_temporal_extent_period()
      : start_time(Fmi::DateTime::NOT_A_DATE_TIME),
        end_time(Fmi::DateTime::NOT_A_DATE_TIME),
        timestep(0),
        timesteps(0)
  {
  }
  Fmi::DateTime start_time;
  Fmi::DateTime end_time;
  int timestep;
  int timesteps;
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
  std::set<int> time_steps;
};

struct edr_vertical_extent
{
  edr_vertical_extent() : vrs("Vertical Reference System") {}
  std::string vrs;
  std::string level_type;
  std::vector<std::string> levels;
};

struct EDRMetaData
{
  edr_spatial_extent spatial_extent;
  edr_temporal_extent temporal_extent;
  edr_vertical_extent vertical_extent;
  std::set<std::string> parameter_names;
  std::map<std::string, edr_parameter> parameters;
  std::map<std::string, int> parameter_precisions;
  std::set<std::string> data_queries;    // Supported data_queries, defined in config file
  std::set<std::string> output_formats;  // Supported output_formats, defined in config file
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
  bool sourceHasInstances() const {
      return !(data_queries.empty() || isObsProducer() || isAviProducer());
  }
};

using EDRProducerMetaData =
    std::map<std::string, std::vector<EDRMetaData>>;  // producer-> meta data

EDRProducerMetaData get_edr_metadata_qd(const Engine::Querydata::Engine& qEngine,
                                        const std::string& default_language,
                                        const ParameterInfo* pinfo,
                                        const CollectionInfoContainer& cic,
                                        const SupportedDataQueries& sdq,
                                        const SupportedOutputFormats& sofs,
                                        const SupportedProducerLocations& spl);
EDRProducerMetaData get_edr_metadata_grid(const Engine::Grid::Engine& gEngine,
                                          const std::string& default_language,
                                          const ParameterInfo* pinfo,
                                          const CollectionInfoContainer& cic,
                                          const SupportedDataQueries& sdq,
                                          const SupportedOutputFormats& sofs,
                                          const SupportedProducerLocations& spl);
#ifndef WITHOUT_OBSERVATION
EDRProducerMetaData get_edr_metadata_obs(
    Engine::Observation::Engine& obsEngine,
    const std::string& default_language,
    const ParameterInfo* pinfo,
    const std::map<std::string, const SmartMet::Engine::Observation::ObservableProperty*>&
        observable_properties,
    const CollectionInfoContainer& cic,
    const SupportedDataQueries& sdq,
    const SupportedOutputFormats& sofs,
    const SupportedProducerLocations& spl,
    const ProducerParameters& prodParams,
    unsigned int observation_period);
#endif
#ifndef WITHOUT_AVI
EDRProducerMetaData get_edr_metadata_avi(const Engine::Avi::Engine& aviEngine,
                                         const AviCollections& aviCollections,
                                         const std::string& default_language,
                                         const ParameterInfo* pinfo,
                                         const CollectionInfoContainer& cic,
                                         const SupportedDataQueries& sdq,
                                         const SupportedOutputFormats& sofs,
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
