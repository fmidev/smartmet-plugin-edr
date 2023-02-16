#pragma once

#include "LocationInfo.h"
#include "ParameterInfo.h"
#include "EDRDefs.h"
#include <boost/date_time/posix_time/ptime.hpp>
#include <boost/optional.hpp>
#include <map>
#include <set>
#include <list>
#include <string>

namespace SmartMet
{
namespace Engine
{
namespace Querydata
{
class Engine;
}
namespace Grid
{
class Engine;
}
#ifndef WITHOUT_OBSERVATION
namespace Observation
{
class Engine;
}
#endif
namespace Avi
{
class Engine;
}
}  // namespace Engine
namespace Plugin
{
namespace EDR
{
// Parameter infor from querydata-, observation-, grid-engine
struct edr_parameter
{
  edr_parameter(std::string n, std::string d) : name(std::move(n)), description(std::move(d)) {}
  edr_parameter(std::string n, std::string d, std::string u)
      : name(std::move(n)), description(std::move(d)), unit(std::move(u))
  {
  }
  std::string name;
  std::string description;
  std::string unit;
};

struct edr_spatial_extent
{
  double bbox_xmin;
  double bbox_ymin;
  double bbox_xmax;
  double bbox_ymax;
  std::string crs{"EPSG:4326"};
};

struct edr_temporal_extent
{
  boost::posix_time::ptime origin_time{boost::posix_time::not_a_date_time};
  boost::posix_time::ptime start_time;
  boost::posix_time::ptime end_time;
  int timestep;
  int timesteps;
  std::string trs{"Temporal Reference System"};
};

struct edr_vertical_extent
{
  std::string level_type;
  std::vector<std::string> levels;
  std::string vrs{"Vertical Reference System"};
};

struct EDRMetaData
{
  edr_spatial_extent spatial_extent;
  edr_temporal_extent temporal_extent;
  edr_vertical_extent vertical_extent;
  std::set<std::string> parameter_names;
  std::map<std::string, edr_parameter> parameters;
  std::map<std::string, int> parameter_precisions;
  std::set<std::string> data_queries;  // Supported data_queries, defined in config file
  std::set<std::string> output_formats;  // Supported output_formats, defined in config file
  const SupportedLocations *locations{nullptr};  // Supported locations, default keyword synop_fi
                                                 // can be overwirtten in configuration file
  const ParameterInfo *parameter_info{nullptr};  // Info about parameters from config file
  std::string language{"en"};                    // Language from configuration file
  int getPrecision(const std::string &parameter_name) const;
  bool isAviProducer = false;
};

using EDRProducerMetaData =
    std::map<std::string, std::vector<EDRMetaData>>;  // producer-> meta data

EDRProducerMetaData get_edr_metadata_qd(const Engine::Querydata::Engine &qEngine,
										const std::string& default_language,
										const ParameterInfo *pinfo,
										const SupportedDataQueries& sdq,
										const SupportedOutputFormats& sofs,
										const SupportedProducerLocations &spl);
EDRProducerMetaData get_edr_metadata_grid(const Engine::Grid::Engine &gEngine,
										  const std::string& default_language,
										  const ParameterInfo *pinfo,
										  const SupportedDataQueries& sdq,
										  const SupportedOutputFormats& sofs,
										  const SupportedProducerLocations &spl);
#ifndef WITHOUT_OBSERVATION
EDRProducerMetaData get_edr_metadata_obs(Engine::Observation::Engine &obsEngine,
										  const std::string& default_language,
										  const ParameterInfo *pinfo,
										  const SupportedDataQueries& sdq,
										  const SupportedOutputFormats& sofs,
										  const SupportedProducerLocations &spl);
#endif
EDRProducerMetaData get_edr_metadata_avi(const Engine::Avi::Engine &aviEngine,
										 const AviCollections &aviCollections,
										 const std::string& default_language,
										 const ParameterInfo *pinfo,
										 const SupportedDataQueries& sdq,
										 const SupportedOutputFormats& sofs,
										 const SupportedProducerLocations &spl);

void load_locations_avi(const Engine::Avi::Engine &aviEngine,
                        const AviCollections &aviCollections,
                        SupportedProducerLocations &spl);

class EngineMetaData
{
public:
  EngineMetaData();
  void addMetaData(const std::string& source_name, const EDRProducerMetaData& metadata);
  const EDRProducerMetaData& getMetaData(const std::string& source_name) const;
  const std::map<std::string, EDRProducerMetaData>& getMetaData() const;
  const std::time_t& getUpdateTime() const { return itsUpdateTime; }
  bool isValidCollection(const std::string& collection_name) const;
  bool isValidCollection(const std::string& source_name, const std::string& collection_name) const;

private:
  std::map<std::string, EDRProducerMetaData> itsMetaData;
  std::time_t itsUpdateTime;
};

void update_location_info(EngineMetaData &emd, const SupportedProducerLocations &spl);

}  // namespace EDR
}  // namespace Plugin
}  // namespace SmartMet
