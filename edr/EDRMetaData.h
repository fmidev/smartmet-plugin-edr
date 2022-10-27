#pragma once

#include <string>
#include <set>
#include <map>
#include <boost/date_time/posix_time/ptime.hpp>
#include <boost/optional.hpp>

namespace SmartMet
{
namespace Engine {
namespace Querydata {
class Engine;
}
namespace Grid {
class Engine;
}
#ifndef WITHOUT_OBSERVATION
namespace Observation {
class Engine;
}
#endif
} // namespace Engine
namespace Plugin
{
namespace EDR
{
  struct edr_parameter
  {
    edr_parameter(const std::string& n, const std::string& d, const std::string& u = "") : name(n), description(d), unit(u) {}
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
    std::string crs {"EPSG:4326"};
  };
  
  struct edr_temporal_extent
  {
    boost::posix_time::ptime origin_time{boost::posix_time::not_a_date_time};
    boost::posix_time::ptime start_time;
    boost::posix_time::ptime end_time;
    int timestep;
    int timesteps;
    std::string trs {"TODO: Temporal Reference System"};
  };
  
  struct edr_vertical_extent
  {
    std::string level_type;
    std::vector<std::string> levels;
    std::string vrs {"TODO: Vertical Reference System"};
  };
  
  struct EDRMetaData
  {
    edr_spatial_extent spatial_extent;
    edr_temporal_extent temporal_extent;
    edr_vertical_extent vertical_extent;
    std::set<std::string> parameter_names;
    std::map<std::string, edr_parameter> parameters;
    std::map<std::string, int> parameter_precisions;
    int getPrecision(const std::string& parameter_name) const;
  };

  using EDRProducerMetaData = std::map<std::string, std::vector<EDRMetaData>>; // producer-> meta data

  EDRProducerMetaData get_edr_metadata_qd(const std::string &producer, const Engine::Querydata::Engine &qEngine, EDRMetaData *emd = nullptr);
  EDRProducerMetaData get_edr_metadata_grid(const std::string &producer, const Engine::Grid::Engine &gEngine, EDRMetaData *emd = nullptr);
#ifndef WITHOUT_OBSERVATION
  EDRProducerMetaData get_edr_metadata_obs(const std::string &producer, Engine::Observation::Engine &obsEngine, EDRMetaData *emd = nullptr);
#endif

  struct EngineMetaData
  {
	EDRProducerMetaData querydata;
	EDRProducerMetaData grid;
	EDRProducerMetaData observation;
  };

}  // namespace EDR
}  // namespace Plugin
}  // namespace SmartMet
