#pragma once

#include <string>
#include <set>
#include <map>
#include <boost/date_time/posix_time/ptime.hpp>
#include <boost/optional.hpp>

namespace SmartMet
{ 
namespace Plugin
{
namespace EDR
{
  struct edr_parameter
  {
  edr_parameter(const std::string& n, const std::string& d) : name(n), description(d) {}
    std::string name;
    std::string description;
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
  };

  using EDRProducerMetaData = std::map<std::string, std::vector<EDRMetaData>>; // producer-> meta data
  
}  // namespace EDR
}  // namespace Plugin
}  // namespace SmartMet
