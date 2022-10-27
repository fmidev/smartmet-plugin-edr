#include "EDRMetaData.h"

#include <engines/querydata/Engine.h>
#include <engines/grid/Engine.h>
#include <macgyver/Exception.h>
#include <macgyver/StringConversion.h>
#include <timeseries/TimeSeriesOutput.h>
#ifndef WITHOUT_OBSERVATION
#include <engines/observation/Engine.h>
#include <engines/observation/ObservableProperty.h>
#endif

namespace SmartMet
{ 
namespace Plugin
{
namespace EDR
{

  EDRProducerMetaData get_edr_metadata_qd(const std::string &producer, const Engine::Querydata::Engine &qEngine, EDRMetaData *emd /*= nullptr*/) 
{
  try 
	{
	  Engine::Querydata::MetaQueryOptions opts;
	  if (!producer.empty())
		opts.setProducer(producer);
	  auto qd_meta_data = qEngine.getEngineMetadata(opts);
	  
	  /*
		if(qd_meta_data.empty())
		throw Fmi::Exception::Trace(BCP, "Failed to get metadata for querydata producer '" + producer + "'!");
	  */
	  
	  EDRProducerMetaData epmd;
	  
	  if(qd_meta_data.empty())
		return epmd;
	  
	  // Iterate QEngine metadata and add items into collection
	  for (const auto &qmd : qd_meta_data)
		{
		  EDRMetaData producer_emd;
		  
		  if (qmd.levels.size() > 1) 
			{
			  if (qmd.levels.front().type == "PressureLevel")
				producer_emd.vertical_extent.vrs =
				  "TODO: PARAMETRICCRS['WMO standard atmosphere layer "
				  "0',PDATUM['Mean Sea Level',ANCHOR['1013.25 hPa at "
				  "15\u00b0C']],CS[parametric,1§],AXIS['pressure "
				  "(hPa)',up],PARAMETRICUNIT['Hectopascal',100.0";
			  else if (qmd.levels.front().type == "HybridLevel")
				producer_emd.vertical_extent.vrs =
				  "TODO: PARAMETRICCRS['WMO Hybrid "
				  "Levels',CS[parametric,1],AXIS['Hybrid',up]]";
			  producer_emd.vertical_extent.level_type = qmd.levels.front().type;
			  for (const auto &item : qmd.levels)
				producer_emd.vertical_extent.levels.push_back(Fmi::to_string(item.value));
			}
		  
		  const auto &rangeLon = qmd.wgs84Envelope.getRangeLon();
		  const auto &rangeLat = qmd.wgs84Envelope.getRangeLat();
		  producer_emd.spatial_extent.bbox_xmin = rangeLon.getMin();
		  producer_emd.spatial_extent.bbox_ymin = rangeLat.getMin();
		  producer_emd.spatial_extent.bbox_xmax = rangeLon.getMax();
		  producer_emd.spatial_extent.bbox_ymax = rangeLat.getMax();
		  producer_emd.temporal_extent.origin_time = qmd.originTime;
		  producer_emd.temporal_extent.start_time = qmd.firstTime;
		  producer_emd.temporal_extent.end_time = qmd.lastTime;
		  producer_emd.temporal_extent.timestep = qmd.timeStep;
		  producer_emd.temporal_extent.timesteps= qmd.nTimeSteps;
		  
		  for (const auto &p : qmd.parameters)
			{
			  auto parameter_name = p.name;
			  boost::algorithm::to_lower(parameter_name);
			  producer_emd.parameter_names.insert(parameter_name);
			  producer_emd.parameters.insert(std::make_pair(parameter_name, edr_parameter(p.name, p.description)));
			}
		  epmd[qmd.producer].push_back(producer_emd);
		}
	  
	  if (!producer.empty() && emd)
		{
		  const auto &emds = epmd.begin()->second;
		  for (unsigned int i = 0; i < emds.size(); i++)
			{
			  if (i == 0 || emd->temporal_extent.origin_time < emds.at(i).temporal_extent.origin_time)
				*emd = emds.at(i);
			}
		}
	  
	  return epmd;
	}
  catch (...) 
	{
	  throw Fmi::Exception::Trace(BCP, "Operation failed!");
	}
}

  EDRProducerMetaData get_edr_metadata_grid(const std::string &producer, const Engine::Grid::Engine &gEngine, EDRMetaData *emd /*= nullptr*/)
{
  try 
	{    
	  EDRProducerMetaData epmd;
	  
	  std::vector<std::string> parts;
	  boost::algorithm::split(parts, producer, boost::algorithm::is_any_of("."));
	  
	  auto producerName = parts[0];
	  auto geometryId = (parts.size() > 1 ? parts[1] : "");
	  auto levelId = (parts.size() > 2 ? parts[2] : "");
	  
	  auto grid_meta_data = gEngine.getEngineMetadata(producerName.c_str());    
	  
	  /*
		if(grid_meta_data.empty())
		throw Fmi::Exception::Trace(BCP, "Failed to get metadata for grid producer '" + producer + "'!");
	  */
    
	  // Iterate Grid metadata and add items into collection
	  for (const auto &gmd : grid_meta_data) 
		{		  
		  if(!geometryId.empty())
			{
			  if(!boost::iequals(Fmi::to_string(gmd.geometryId), geometryId))
				continue;
			}
		  if(!levelId.empty())
			{
			  if(!boost::iequals(Fmi::to_string(gmd.levelId), levelId))
				continue;
			}
		  
		  EDRMetaData producer_emd;
		  
		  if (gmd.levels.size() > 1) {
			producer_emd.vertical_extent.vrs = (Fmi::to_string(gmd.levelId)+";"+gmd.levelName+";"+gmd.levelDescription);
		
			/*
			  1;GROUND;Ground or water surface;
			  2;PRESSURE;Pressure level;
			  3;HYBRID;Hybrid level;
			  4;ALTITUDE;Altitude;
			  5;TOP;Top of atmosphere;
			  6;HEIGHT;Height above ground in meters;
			  7;MEANSEA;Mean sea level;
			  8;ENTATM;Entire atmosphere;
			  9;GROUND_DEPTH;Layer between two depths below land surface;
			  10;DEPTH;Depth below some surface;
			  11;PRESSURE_DELTA;Level at specified pressure difference from ground to level;
			  12;MAXTHETAE;Level where maximum equivalent potential temperature is found;
			  13;HEIGHT_LAYER;Layer between two metric heights above ground;
			  14;DEPTH_LAYER;Layer between two depths below land surface;
			  15;ISOTHERMAL;Isothermal level, temperature in 1/100 K;
			  16;MAXWIND;Maximum wind level;
			*/
			producer_emd.vertical_extent.level_type = gmd.levelName;
			for (const auto &level : gmd.levels)
			  producer_emd.vertical_extent.levels.push_back(Fmi::to_string(level));
		  }
		  
		  
		  producer_emd.spatial_extent.bbox_ymin = std::min(gmd.latlon_bottomLeft.y(), gmd.latlon_bottomRight.y());
		  producer_emd.spatial_extent.bbox_xmax = std::max(gmd.latlon_bottomRight.x(), gmd.latlon_topRight.x());
		  producer_emd.spatial_extent.bbox_ymax = std::max(gmd.latlon_topLeft.y(), gmd.latlon_topRight.y());
		  producer_emd.temporal_extent.origin_time = boost::posix_time::from_iso_string(gmd.analysisTime);
		  auto begin_iter = gmd.times.begin();
		  auto end_iter = gmd.times.end();
		  end_iter--;
		  const auto& end_time = *end_iter;
		  const auto& start_time = *begin_iter;
		  begin_iter++;
		  const auto& start_time_plus_one = *begin_iter;
		  auto timestep_duration = (boost::posix_time::from_iso_string(start_time_plus_one) - boost::posix_time::from_iso_string(start_time));
		  
		  producer_emd.temporal_extent.start_time = boost::posix_time::from_iso_string(start_time);
		  producer_emd.temporal_extent.end_time =  boost::posix_time::from_iso_string(end_time);
		  producer_emd.temporal_extent.timestep = (timestep_duration.total_seconds() / 60);
		  producer_emd.temporal_extent.timesteps= gmd.times.size();
		  
		  for (const auto &p : gmd.parameters) 
			{
			  auto parameter_name = p.parameterName;
			  boost::algorithm::to_lower(parameter_name);
			  producer_emd.parameter_names.insert(parameter_name);
			  producer_emd.parameters.insert(std::make_pair(parameter_name, edr_parameter(parameter_name, p.parameterDescription, p.parameterUnits)));
			}
		  std::string producerId = (gmd.producerName + "." + Fmi::to_string(gmd.geometryId) + "." + Fmi::to_string(gmd.levelId));
		  boost::algorithm::to_lower(producerId);
		  
		  epmd[producerId].push_back(producer_emd);      
		}
	  
	  if (!producer.empty() && emd) 
		{
		  const auto &emds = epmd.begin()->second;
		  for (unsigned int i = 0; i < emds.size(); i++) 
			{
			  if (i == 0 || emd->temporal_extent.origin_time <
				  emds.at(i).temporal_extent.origin_time)
				*emd = emds.at(i);
			}
		}	  
	  return epmd;
	} 
  catch (...)
	{
	  throw Fmi::Exception::Trace(BCP, "Operation failed!");
	}
}

#ifndef WITHOUT_OBSERVATION
EDRProducerMetaData get_edr_metadata_obs(const std::string &producer, Engine::Observation::Engine &obsEngine, EDRMetaData *emd /*= nullptr*/) 
{
  try 
	{
	  std::map<std::string, Engine::Observation::MetaData> observation_meta_data;
	  
	  std::set<std::string> producers;
	  if (!producer.empty())
		producers.insert(producer);
	  else
		producers = obsEngine.getValidStationTypes();
	  
	  for (const auto &prod : producers)
		observation_meta_data.insert(std::make_pair(prod, obsEngine.metaData(prod)));
	  
	  EDRProducerMetaData epmd;
	  
	  // Iterate Observation engine metadata and add item into collection
	  std::vector<std::string> params{""};
	  for (const auto &item : observation_meta_data)
		{
		  const auto &producer = item.first;
		  const auto &obs_md = item.second;
		  
		  EDRMetaData producer_emd;
		  
		  producer_emd.spatial_extent.bbox_xmin = obs_md.bbox.xMin;
		  producer_emd.spatial_extent.bbox_ymin = obs_md.bbox.yMin;
		  producer_emd.spatial_extent.bbox_xmax = obs_md.bbox.xMax;
		  producer_emd.spatial_extent.bbox_ymax = obs_md.bbox.yMax;
		  producer_emd.temporal_extent.origin_time =
			boost::posix_time::second_clock::universal_time();
		  producer_emd.temporal_extent.start_time = obs_md.period.begin();
		  producer_emd.temporal_extent.end_time = obs_md.period.last();
		  producer_emd.temporal_extent.timestep = obs_md.timestep;
		  producer_emd.temporal_extent.timesteps = (obs_md.period.length().minutes() / obs_md.timestep);
		  
		  for (const auto &p : obs_md.parameters)
			{
			  params[0] = p;
			  std::string description = p;
			  /*
			  // This is slow and not used any more, 
			  auto observedProperties =
			  obsEngine.observablePropertyQuery(params, "en");
			  if (observedProperties->size() > 0)
			  description = observedProperties->at(0).observablePropertyLabel;
			  */
			  
			  auto parameter_name = p;
			  boost::algorithm::to_lower(parameter_name);
			  producer_emd.parameter_names.insert(parameter_name);
			  producer_emd.parameters.insert(std::make_pair(parameter_name, edr_parameter(p, description)));
			}
		  
		  epmd[producer].push_back(producer_emd);
		}
	  
	  if (!producer.empty() && emd)
		{
		  const auto &emds = epmd.begin()->second;
		  for (unsigned int i = 0; i < emds.size(); i++)
			{
			  if (i == 0 || emd->temporal_extent.origin_time < emds.at(i).temporal_extent.origin_time)
				*emd = emds.at(i);
			}
		}
	  
	  return epmd;
  }
  catch (...)
	{
	  throw Fmi::Exception::Trace(BCP, "Operation failed!");
	}
}
#endif

int EDRMetaData::getPrecision(const std::string& parameter_name) const
{
  try 
    {
      auto pname = parameter_name;
      boost::algorithm::to_lower(pname);

      if(parameter_precisions.find(pname) != parameter_precisions.end())
	return parameter_precisions.at(pname);
      
      return parameter_precisions.at("__DEFAULT_PRECISION__");
    }
  catch (...)
    {
      throw Fmi::Exception::Trace(BCP, "Operation failed!");
    }
}

}  // namespace EDR
}  // namespace Plugin
}  // namespace SmartMet
