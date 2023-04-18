#include "EDRMetaData.h"
#ifndef WITHOUT_AVI
#include "AviCollection.h"
#endif
#include "EDRQuery.h"

#include <engines/grid/Engine.h>
#include <engines/querydata/Engine.h>
#include <macgyver/Exception.h>
#include <macgyver/StringConversion.h>
#include <macgyver/AnsiEscapeCodes.h>
#include <timeseries/TimeSeriesOutput.h>
#include <spine/Convenience.h>
#ifndef WITHOUT_OBSERVATION
#include <engines/observation/Engine.h>
#include <engines/observation/ObservableProperty.h>
#endif
#include <boost/any.hpp>
#include <engines/avi/Engine.h>

namespace SmartMet
{
namespace Plugin
{
namespace EDR
{
#define MAX_TIME_PERIODS 300
#define DEFAULT_PRECISION 4
static EDRProducerMetaData EMPTY_PRODUCER_METADATA;
class TimePeriod
{
public:
  TimePeriod(const boost::posix_time::ptime& t1, const boost::posix_time::ptime& t2) : period_start(t1), period_end(t2) {}
  void setStartTime(const boost::posix_time::ptime& t) { period_start = t; }
  void setEndTime(const boost::posix_time::ptime& t) { period_end = t; }
  const boost::posix_time::ptime& getStartTime() const { return period_start; }
  const boost::posix_time::ptime& getEndTime() const { return period_end; }
  int length() const 
  {
	if(period_start.is_not_a_date_time() || period_end.is_not_a_date_time())
	  return 0;
	return (period_end-period_start).total_seconds(); 
  }
private:
  boost::posix_time::ptime period_start = boost::posix_time::not_a_date_time;
  boost::posix_time::ptime period_end = boost::posix_time::not_a_date_time;
};
namespace
{
  std::set<std::string> get_collection_names(const EDRProducerMetaData& md)
  {
	std::set<std::string> collection_names;
	for(const auto& item : md)
	  collection_names.insert(item.first);
	
	return collection_names;
  }
  
  void report_duplicate_collection(const std::string& collection, const std::string& duplicate_source, const std::string& original_source)
  {
	std::cerr << (Spine::log_time_str() + ANSI_FG_MAGENTA + " [edr] Duplicate collection '" + collection + "' removed from " + duplicate_source + ", it has already been defined in " + original_source  + ANSI_FG_DEFAULT)
			  << std::endl;
  }
  
  const std::set<std::string> &get_supported_data_queries(const std::string &producer,
														  const SupportedDataQueries &sdq)
  {
	try
	  {
		if (sdq.find(producer) != sdq.end())
		  return sdq.at(producer);
		
		return sdq.at(DEFAULT_DATA_QUERIES);
	  }
	catch (...)
	  {
		throw Fmi::Exception::Trace(BCP, "Operation failed!");
	  }
  }
  
  const std::set<std::string> &get_supported_output_formats(const std::string &producer,
															const SupportedOutputFormats &sofs)
  {
	try
	  {
		if (sofs.find(producer) != sofs.end())
		  return sofs.at(producer);
		
		return sofs.at(DEFAULT_OUTPUT_FORMATS);
  }
	catch (...)
	  {
		throw Fmi::Exception::Trace(BCP, "Operation failed!");
	  }
  }
} // namespace
  
EDRProducerMetaData get_edr_metadata_qd(const Engine::Querydata::Engine &qEngine,
                                        const std::string &default_language,
                                        const ParameterInfo *pinfo,
                                        const CollectionInfoContainer& cic,
                                        const SupportedDataQueries &sdq,
                                        const SupportedOutputFormats &sofs,
                                        const SupportedProducerLocations &spl)
{
  try
  {
    Engine::Querydata::MetaQueryOptions opts;
    auto qd_meta_data = qEngine.getEngineMetadata(opts);

    EDRProducerMetaData epmd;

    if (qd_meta_data.empty())
      return epmd;

    // Iterate QEngine metadata and add items into collection
    for (const auto &qmd : qd_meta_data)
    {
      EDRMetaData producer_emd;

      if (qmd.levels.size() > 1)
      {
        producer_emd.vertical_extent.vrs =
            ("Vertical Reference System: " + qmd.levels.front().type);
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
	  
	  edr_temporal_extent temporal_extent;
      temporal_extent.origin_time = qmd.originTime;
	  edr_temporal_extent_period temporal_extent_period;
      temporal_extent_period.start_time = qmd.firstTime;
      temporal_extent_period.end_time = qmd.lastTime;
      temporal_extent_period.timestep = qmd.timeStep;
      temporal_extent_period.timesteps = qmd.nTimeSteps;
	  temporal_extent.time_periods.push_back(temporal_extent_period);
      producer_emd.temporal_extent = temporal_extent;

      for (const auto &p : qmd.parameters)
      {
        auto parameter_name = p.name;
        boost::algorithm::to_lower(parameter_name);
        producer_emd.parameter_names.insert(parameter_name);
        producer_emd.parameters.insert(
            std::make_pair(parameter_name, edr_parameter(p.name, p.description)));
      }
      producer_emd.parameter_precisions["__DEFAULT_PRECISION__"] = DEFAULT_PRECISION;
      producer_emd.language = default_language;
      producer_emd.parameter_info = pinfo;
      producer_emd.collection_info = &cic.getInfo(Q_ENGINE, qmd.producer);
      producer_emd.data_queries = get_supported_data_queries(qmd.producer, sdq);
      producer_emd.output_formats = get_supported_output_formats(qmd.producer, sofs);

      auto producer_key =
          (spl.find(qmd.producer) != spl.end() ? qmd.producer : DEFAULT_PRODUCER_KEY);
      if (spl.find(producer_key) != spl.end())
        producer_emd.locations = &spl.at(producer_key);
      epmd[qmd.producer].push_back(producer_emd);
    }

    return epmd;
  }
  catch (...)
  {
    throw Fmi::Exception::Trace(BCP, "Operation failed!");
  }
}

EDRProducerMetaData get_edr_metadata_grid(const Engine::Grid::Engine &gEngine,
                                          const std::string &default_language,
                                          const ParameterInfo *pinfo,
										  const CollectionInfoContainer& cic,
                                          const SupportedDataQueries &sdq,
                                          const SupportedOutputFormats &sofs,
                                          const SupportedProducerLocations &spl)
{
  try
  {
    EDRProducerMetaData epmd;

    auto grid_meta_data = gEngine.getEngineMetadata("");

    // Iterate Grid metadata and add items into collection
    for (const auto &gmd : grid_meta_data)
    {
	
      EDRMetaData producer_emd;

      if (gmd.levels.size() > 1)
      {
        producer_emd.vertical_extent.vrs =
            (Fmi::to_string(gmd.levelId) + ";" + gmd.levelName + ";" + gmd.levelDescription);

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
          11;PRESSURE_DELTA;Level at specified pressure difference from ground
          to level; 12;MAXTHETAE;Level where maximum equivalent potential
          temperature is found; 13;HEIGHT_LAYER;Layer between two metric heights
          above ground; 14;DEPTH_LAYER;Layer between two depths below land
          surface; 15;ISOTHERMAL;Isothermal level, temperature in 1/100 K;
          16;MAXWIND;Maximum wind level;
        */
        producer_emd.vertical_extent.level_type = gmd.levelName;
        for (const auto &level : gmd.levels)
          producer_emd.vertical_extent.levels.push_back(Fmi::to_string(level));
      }

      producer_emd.spatial_extent.bbox_ymin =
          std::min(gmd.latlon_bottomLeft.y(), gmd.latlon_bottomRight.y());
      producer_emd.spatial_extent.bbox_xmax =
          std::max(gmd.latlon_bottomRight.x(), gmd.latlon_topRight.x());
      producer_emd.spatial_extent.bbox_ymax =
          std::max(gmd.latlon_topLeft.y(), gmd.latlon_topRight.y());
	  
      auto begin_iter = gmd.times.begin();
      auto end_iter = gmd.times.end();
      end_iter--;
      const auto &end_time = *end_iter;
      const auto &start_time = *begin_iter;
	  if(gmd.times.size() > 1)
		begin_iter++;
      const auto &start_time_plus_one = *begin_iter;
      auto timestep_duration = (boost::posix_time::from_iso_string(start_time_plus_one) -
                                boost::posix_time::from_iso_string(start_time));
	  edr_temporal_extent temporal_extent;
      temporal_extent.origin_time = boost::posix_time::from_iso_string(gmd.analysisTime);
	  edr_temporal_extent_period temporal_extent_period;
      temporal_extent_period.start_time = boost::posix_time::from_iso_string(start_time);
      temporal_extent_period.end_time = boost::posix_time::from_iso_string(end_time);
      temporal_extent_period.timestep = (timestep_duration.total_seconds() / 60);
      temporal_extent_period.timesteps = gmd.times.size();
	  temporal_extent.time_periods.push_back(temporal_extent_period);
      producer_emd.temporal_extent = temporal_extent;

      for (const auto &p : gmd.parameters)
      {
        auto parameter_name = p.parameterName;
        boost::algorithm::to_lower(parameter_name);
        producer_emd.parameter_names.insert(parameter_name);
        producer_emd.parameters.insert(std::make_pair(
            parameter_name,
            edr_parameter(parameter_name, p.parameterDescription, p.parameterUnits)));
      }

      std::string producerId = (gmd.producerName + "." + Fmi::to_string(gmd.geometryId) + "." +
                                Fmi::to_string(gmd.levelId));

	  producer_emd.collection_info_engine.title = ("Producer: " + gmd.producerName + "; Geometry id: " +  Fmi::to_string(gmd.geometryId) + "; Level id: " + Fmi::to_string(gmd.levelId));
	  producer_emd.collection_info_engine.description = gmd.producerDescription;
	  producer_emd.collection_info_engine.keywords.insert(gmd.producerName);

      boost::algorithm::to_lower(producerId);

      producer_emd.parameter_precisions["__DEFAULT_PRECISION__"] = DEFAULT_PRECISION;
      producer_emd.language = default_language;
      producer_emd.parameter_info = pinfo;
      producer_emd.collection_info = &cic.getInfo(GRID_ENGINE, gmd.producerName);

      producer_emd.data_queries = get_supported_data_queries(gmd.producerName, sdq);
      producer_emd.output_formats = get_supported_output_formats(gmd.producerName, sofs);
      auto producer_key =
          (spl.find(gmd.producerName) != spl.end() ? gmd.producerName : DEFAULT_PRODUCER_KEY);
      if (spl.find(producer_key) != spl.end())
        producer_emd.locations = &spl.at(producer_key);

      epmd[producerId].push_back(producer_emd);
    }

    return epmd;
  }
  catch (...)
  {
    throw Fmi::Exception::Trace(BCP, "Operation failed!");
  }
}

#ifndef WITHOUT_OBSERVATION
EDRProducerMetaData get_edr_metadata_obs(Engine::Observation::Engine &obsEngine,
                                         const std::string &default_language,
                                         const ParameterInfo *pinfo,
										 const CollectionInfoContainer& cic,
                                         const SupportedDataQueries &sdq,
                                         const SupportedOutputFormats &sofs,
                                         const SupportedProducerLocations &spl,
										 unsigned int observation_period)

{
  try
  {
    std::map<std::string, Engine::Observation::MetaData> observation_meta_data;

    std::set<std::string> producers = obsEngine.getValidStationTypes();

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
	  edr_temporal_extent temporal_extent;
      temporal_extent.origin_time = boost::posix_time::second_clock::universal_time();
	  edr_temporal_extent_period temporal_extent_period;
	  auto time_of_day = obs_md.period.last().time_of_day();
	  auto end_date = obs_md.period.last().date();
	  // In order to get rid of fractions of a second in end_time
	  boost::posix_time::ptime end_time(end_date, boost::posix_time::time_duration(time_of_day.hours(), time_of_day.minutes(), 0));
	  if(observation_period > 0)
		temporal_extent_period.start_time = (end_time - boost::posix_time::hours(observation_period));
	  else
		temporal_extent_period.start_time = obs_md.period.begin();
      temporal_extent_period.end_time = end_time;
      temporal_extent_period.timestep = obs_md.timestep;
      temporal_extent_period.timesteps = (obs_md.period.length().minutes() / obs_md.timestep);
	  temporal_extent.time_periods.push_back(temporal_extent_period);
      producer_emd.temporal_extent = temporal_extent;

      for (const auto &p : obs_md.parameters)
      {
        params[0] = p;
        /*
        // This is slow and not used any more,
        std::string description = p;
        auto observedProperties =
        obsEngine.observablePropertyQuery(params, "en");
        if (observedProperties->size() > 0)
          description = observedProperties->at(0).observablePropertyLabel;
        */

        const auto &description = p;
        auto parameter_name = p;
        boost::algorithm::to_lower(parameter_name);
        producer_emd.parameter_names.insert(parameter_name);
        producer_emd.parameters.insert(
            std::make_pair(parameter_name, edr_parameter(p, description)));
      }

      producer_emd.parameter_precisions["__DEFAULT_PRECISION__"] = DEFAULT_PRECISION;
      producer_emd.language = default_language;
      producer_emd.parameter_info = pinfo;
      producer_emd.collection_info = &cic.getInfo(OBS_ENGINE, producer);
      producer_emd.data_queries = get_supported_data_queries(producer, sdq);
      producer_emd.output_formats = get_supported_output_formats(producer, sofs);
      auto producer_key = (spl.find(producer) != spl.end() ? producer : DEFAULT_PRODUCER_KEY);
      if (spl.find(producer_key) != spl.end())
        producer_emd.locations = &spl.at(producer_key);

      epmd[producer].push_back(producer_emd);
    }

    return epmd;
  }
  catch (...)
  {
    throw Fmi::Exception::Trace(BCP, "Operation failed!");
  }
}
#endif

#ifndef WITHOUT_AVI
std::vector<std::string> time_periods(const std::set<boost::local_time::local_date_time>& timesteps)
{
  std::vector<std::string> ret;
  if(timesteps.size() == 0)
	return ret;
  if(timesteps.size() == 1)
	{
	  ret.push_back(Fmi::to_iso_string(timesteps.begin()->utc_time()));
	  return ret;
	}

  // Insert time periods into vector
  std::vector<TimePeriod> time_periods;
  auto it_previous = timesteps.begin();
  auto it = timesteps.begin();
  it++;
  while(it != timesteps.end())
	{
	  time_periods.push_back({it_previous->utc_time(), it->utc_time()});
	  it_previous++;
	  it++;
	}

  // Iterate vector and find out periods with even timesteps
  TimePeriod tp = time_periods.at(0);
  int first_period_length = tp.length();
  bool multiple_periods = false;
  for(unsigned int i = 1; i < time_periods.size(); i++)
	{
	  auto tp_current = time_periods.at(i);
	  if(first_period_length != tp_current.length() || i == time_periods.size() - 1)
		{
		  if(multiple_periods)
			{
			  if(i == time_periods.size() - 1)
				tp.setEndTime(tp_current.getEndTime());
			  ret.push_back(Fmi::to_iso_string(tp.getStartTime())+"Z/"+Fmi::to_iso_string(tp.getEndTime())+"Z/"+Fmi::to_string(first_period_length));
			  multiple_periods = false;
			}
		  else
			{
			  ret.push_back(Fmi::to_iso_string(tp.getStartTime()));
			  ret.push_back(Fmi::to_iso_string(tp.getEndTime()));
			  if(i == time_periods.size() - 1)
				ret.push_back(Fmi::to_iso_string(tp_current.getEndTime()));
			}
		  i++;
		  if(i < time_periods.size() - 1)
			{
			  tp = time_periods.at(i);
			  first_period_length = tp.length();
			}
		}
	  else
		{
		  tp.setEndTime(tp_current.getEndTime());
		  multiple_periods = true;
		}
	}

  return ret;
}

// Merge time periods when possible (even timesteps)
edr_temporal_extent get_temporal_extent(const std::set<boost::local_time::local_date_time>& timesteps)
{
  edr_temporal_extent ret;

  auto timper = time_periods(timesteps);

  if(timesteps.size() < 2)
	return ret;

  ret.origin_time = boost::posix_time::second_clock::universal_time();

  // Insert time periods into vector
  std::vector<TimePeriod> time_periods;
  auto it_previous = timesteps.begin();
  auto it = timesteps.begin();
  it++;
  while(it != timesteps.end())
	{
	  time_periods.push_back({it_previous->utc_time(), it->utc_time()});
	  it_previous++;
	  it++;
	}

  // Iterate vector and find out periods with even timesteps
  TimePeriod tp = time_periods.at(0);
  int first_period_length = tp.length();
  bool multiple_periods = false;
  std::vector<edr_temporal_extent_period> merged_time_periods;
  for(unsigned int i = 1; i < time_periods.size(); i++)
	{
	  auto tp_current = time_periods.at(i);
	  if(first_period_length != tp_current.length() || i == time_periods.size() - 1)
		{
		  if(multiple_periods)
			{
			  if(i == time_periods.size() - 1)
				tp.setEndTime(tp_current.getEndTime());
			  edr_temporal_extent_period etep;
			  etep.start_time = tp.getStartTime();
			  etep.end_time = tp.getEndTime();
			  etep.timestep = first_period_length / 60; // seconds to minutes
			  etep.timesteps = ((etep.end_time-etep.start_time).total_seconds() / first_period_length);
			  merged_time_periods.push_back(etep);
			  multiple_periods = false;
			}
		  else
			{
			  merged_time_periods.clear();
			  break;
			}
		  i++;
		  if(i < time_periods.size() - 1)
			{
			  tp = time_periods.at(i);
			  first_period_length = tp.length();
			}
		}
	  else
		{
		  tp.setEndTime(tp_current.getEndTime());
		  multiple_periods = true;
		}
	}

  if(merged_time_periods.empty())
	{
	  if(timesteps.size() > MAX_TIME_PERIODS)
		{
		  // Insert only starttime and endtime
		  edr_temporal_extent_period etep;
		  etep.start_time = time_periods.front().getStartTime();
		  etep.end_time = time_periods.back().getEndTime();
		  etep.timestep = 0;
		  etep.timesteps = 0;
		  ret.time_periods.push_back(etep);	  
		}
	  else
		{
		  // Insert all timesteps
		  for(const auto& t : timesteps)
			{
			  edr_temporal_extent_period etep;
			  etep.start_time = t.utc_time();
			  etep.timestep = 0;			  
			  etep.timesteps = 0;
			  ret.time_periods.push_back(etep);	  
			}
		}
	}
  else
	{
	  ret.time_periods = merged_time_periods;	  
	}

  return ret;
}

  edr_temporal_extent getAviTemporalExtent(const Engine::Avi::Engine &aviEngine, const std::string& message_type, int period_length)
{
  edr_temporal_extent ret;

  SmartMet::Engine::Avi::QueryOptions queryOptions;

  queryOptions.itsDistinctMessages = true;
  queryOptions.itsFilterMETARs = true;
  queryOptions.itsExcludeSPECIs = false;
  queryOptions.itsLocationOptions.itsCountries.push_back("FI");

  queryOptions.itsParameters.push_back("icao");
  queryOptions.itsParameters.push_back("messagetime");
  queryOptions.itsParameters.push_back("messagetype");

  queryOptions.itsValidity = SmartMet::Engine::Avi::Accepted;
  queryOptions.itsMessageTypes.push_back(message_type);
  queryOptions.itsMessageFormat = TAC_FORMAT;
	  
  auto now = boost::posix_time::second_clock::universal_time();
  auto start_of_period = (now - boost::posix_time::hours(period_length*24)); // from config file avi.period_length (30 days default)
  std::string startTime = boost::posix_time::to_iso_string(start_of_period);
  std::string endTime = boost::posix_time::to_iso_string(now);
  queryOptions.itsTimeOptions.itsStartTime = "timestamptz '" + startTime + "Z'";
  queryOptions.itsTimeOptions.itsEndTime = "timestamptz '" + endTime + "Z'";
  queryOptions.itsTimeOptions.itsTimeFormat = "iso";
  queryOptions.itsDistinctMessages = true;
  queryOptions.itsMaxMessageStations = -1;
  queryOptions.itsMaxMessageRows = -1;
  queryOptions.itsTimeOptions.itsQueryValidRangeMessages = true;
  // Finnish TAC METAR filtering (ignore messages not starting with 'METAR')
  queryOptions.itsFilterMETARs = (queryOptions.itsMessageFormat == TAC_FORMAT);
  // Finnish SPECIs are ignored (https://jira.fmi.fi/browse/BRAINSTORM-2472)
  queryOptions.itsExcludeSPECIs = true;

  auto aviData = aviEngine.queryStationsAndMessages(queryOptions);

  std::set<boost::local_time::local_date_time> timesteps;
  for (auto stationId : aviData.itsStationIds)
	{
	  auto timeIter = aviData.itsValues[stationId]["messagetime"].cbegin();
	  
	  while(timeIter != aviData.itsValues[stationId]["messagetime"].end()) 
		{
		  boost::local_time::local_date_time timestep = boost::get<boost::local_time::local_date_time>(*timeIter);

		  timesteps.insert(timestep);
		  timeIter++;
		}
	}

  ret = get_temporal_extent(timesteps);

  return ret;
}

std::list<AviMetaData> getAviEngineMetadata(const Engine::Avi::Engine &aviEngine,
                                            const AviCollections &aviCollections)
{
  std::list<AviMetaData> aviMetaData;

  for (auto const &aviCollection : aviCollections)
  {
    AviMetaData amd(aviCollection.getBBox(),
                    aviCollection.getName(),
                    aviCollection.getMessageTypes(),
                    aviCollection.getLocationCheck());
    SmartMet::Engine::Avi::QueryOptions queryOptions;
    std::string icaoColumnName("icao");
    std::string latColumnName("latitude");
    std::string lonColumnName("longitude");

    queryOptions.itsParameters.push_back(icaoColumnName);
    queryOptions.itsParameters.push_back(latColumnName);
    queryOptions.itsParameters.push_back(lonColumnName);

    if (!aviCollection.getCountries().empty())
      queryOptions.itsLocationOptions.itsCountries.insert(
          queryOptions.itsLocationOptions.itsCountries.begin(),
          aviCollection.getCountries().begin(),
          aviCollection.getCountries().end());
    else if (!aviCollection.getIcaos().empty())
      queryOptions.itsLocationOptions.itsIcaos.insert(
          queryOptions.itsLocationOptions.itsIcaos.begin(),
          aviCollection.getIcaos().begin(),
          aviCollection.getIcaos().end());
    else if (!amd.getBBox())
      throw Fmi::Exception(BCP, "Internal error: No metadata query location parameters set");
    else
    {
      auto const &bbox = *amd.getBBox();

      auto west = *bbox.getMinX();
      auto east = *bbox.getMaxX();
      auto south = *bbox.getMinY();
      auto north = *bbox.getMaxY();

      queryOptions.itsLocationOptions.itsBBoxes.push_back(
          SmartMet::Engine::Avi::BBox(west, east, south, north));
    }

    auto stationData = aviEngine.queryStations(queryOptions);
    bool useDataBBox = (!amd.getBBox());
    bool first = true;
    double minX = 0;
    double minY = 0;
    double maxX = 0;
    double maxY = 0;

    for (auto stationId : stationData.itsStationIds)
    {
      auto &columns = stationData.itsValues[stationId];
      auto const &icaoCode = *(columns[icaoColumnName].cbegin());
      auto const &latitude = *(columns[latColumnName].cbegin());
      auto const &longitude = *(columns[lonColumnName].cbegin());

      // Icao code filtering to ignore e.g. ILxx stations

      auto icao = boost::get<std::string>(icaoCode);

      if (aviCollection.filter(icao))
        continue;

      auto lat = boost::get<double>(latitude);
      auto lon = boost::get<double>(longitude);

      if (useDataBBox)
      {
        if (first)
        {
          minY = maxY = lat;
          minX = maxX = lon;

          first = false;
        }
        else
        {
          if (lat < minY)
            minY = lat;
          else if (lat > maxY)
            maxY = lat;

          if (lon < minX)
            minX = lon;
          else if (lon > maxX)
            maxX = lon;
        }
      }

      amd.addStation(
          AviStation(stationId, icao, boost::get<double>(latitude), boost::get<double>(longitude)));
    }

    if (!amd.getStations().empty())
    {
      if (useDataBBox)
        amd.setBBox(AviBBox(minX, minY, maxX, maxY));

      aviMetaData.push_back(amd);
    }
  }

  return aviMetaData;
}

EDRProducerMetaData get_edr_metadata_avi(const Engine::Avi::Engine &aviEngine,
                                         const AviCollections &aviCollections,
                                         const std::string &default_language,
                                         const ParameterInfo *pinfo,
										 const CollectionInfoContainer& cic,
                                         const SupportedDataQueries &sdq,
                                         const SupportedOutputFormats &sofs,
                                         const SupportedProducerLocations &spl)
{
  using boost::posix_time::hours;
  using boost::posix_time::ptime;
  using boost::posix_time::time_duration;

  try
  {
    EDRProducerMetaData edrProducerMetaData;

    auto aviMetaData = getAviEngineMetadata(aviEngine, aviCollections);

    for (const auto &amd : aviMetaData)
    {
	  EDRMetaData edrMetaData;
	  edrMetaData.isAviProducer = true;
      auto const &bbox = *(amd.getBBox());

      edrMetaData.spatial_extent.bbox_xmin = *bbox.getMinX();
      edrMetaData.spatial_extent.bbox_ymin = *bbox.getMinY();
      edrMetaData.spatial_extent.bbox_xmax = *bbox.getMaxX();
      edrMetaData.spatial_extent.bbox_ymax = *bbox.getMaxY();

	  /*
	  edrMetaData.title = amd.getTitle();
	  edrMetaData.description = amd.getDescription();
	  edrMetaData.keywords = amd.getKeywords();
	  */
      // METAR's issue times are 20 and 50, for others round endtime to the start of hour
      //
      // TODO: Period length/adjustment and timestep from config

      auto producer = amd.getProducer();
	  
	  const AviCollection& avi_collection = get_avi_collection(producer, aviCollections);

	  edrMetaData.temporal_extent = getAviTemporalExtent(aviEngine, producer, avi_collection.getPeriodLength());

      for (const auto &p : amd.getParameters())
      {
        auto parameter_name = p.getName();
        boost::algorithm::to_lower(parameter_name);
        edrMetaData.parameter_names.insert(parameter_name);
        edrMetaData.parameters.insert(
            std::make_pair(parameter_name, edr_parameter(parameter_name, p.getDescription())));
      }

      edrMetaData.parameter_precisions["__DEFAULT_PRECISION__"] = DEFAULT_PRECISION;
      edrMetaData.language = default_language;
      edrMetaData.parameter_info = pinfo;
      edrMetaData.collection_info = &cic.getInfo(AVI_ENGINE, producer);
      edrMetaData.data_queries = get_supported_data_queries(producer, sdq);
      edrMetaData.output_formats = get_supported_output_formats(producer, sofs);
      auto producer_key = (spl.find(producer) != spl.end() ? producer : DEFAULT_PRODUCER_KEY);
      if (spl.find(producer_key) != spl.end())
        edrMetaData.locations = &spl.at(producer_key);

      edrProducerMetaData[producer].push_back(edrMetaData);
    }

    return edrProducerMetaData;
  }
  catch (...)
  {
    throw Fmi::Exception::Trace(BCP, "Operation failed!");
  }
}

void load_locations_avi(const Engine::Avi::Engine &aviEngine,
                        const AviCollections &aviCollections,
                        SupportedProducerLocations &spl)
{
  auto aviMetaData = getAviEngineMetadata(aviEngine, aviCollections);

  for (const auto &amd : aviMetaData)
  {
    SupportedLocations sls;

    for (const auto &station : amd.getStations())
    {
      // TODO: Station name needed ?

      location_info li;
      li.id = station.getIcao();
      li.longitude = station.getLongitude();
      li.latitude = station.getLatitude();
      li.name = ("ICAO: "+station.getIcao()+"; stationId: "+Fmi::to_string(station.getId()));
      li.type = "ICAO";
      li.keyword = amd.getProducer();

      sls[li.id] = li;
    }

    spl[amd.getProducer()] = sls;
  }
}
#endif

int EDRMetaData::getPrecision(const std::string &parameter_name) const
{
  try
  {
    auto pname = parameter_name;
    boost::algorithm::to_lower(pname);

    if (parameter_precisions.find(pname) != parameter_precisions.end())
      return parameter_precisions.at(pname);

    return parameter_precisions.at("__DEFAULT_PRECISION__");
  }
  catch (...)
  {
    throw Fmi::Exception::Trace(BCP, "Operation failed!");
  }
}

#ifdef LATER
void update_location_info(EDRProducerMetaData &pmd, const SupportedProducerLocations &spl)
{
  try
  {
    for (auto &item : pmd)
    {
      const auto &producer = item.first;
      //
      auto &md_vector = item.second;
      auto producer_key = (spl.find(producer) != spl.end() ? producer : DEFAULT_PRODUCER_KEY);
      if (spl.find(producer_key) != spl.end())
      {
        for (auto &md : md_vector)
          md.locations = &spl.at(producer_key);
      }
    }
  }
  catch (...)
  {
    throw Fmi::Exception::Trace(BCP, "Operation failed!");
  }
}

void update_location_info(EngineMetaData &emd, const SupportedProducerLocations &spl)
{
  try
  {
    auto &metadata = emd.getMetaData();
    for (auto &item : metadata)
    {
      update_location_info(item.second, spl);
    }
  }
  catch (...)
  {
    throw Fmi::Exception::Trace(BCP, "Operation failed!");
  }
}
#endif

EngineMetaData::EngineMetaData() : itsUpdateTime(std::chrono::system_clock::to_time_t(std::chrono::system_clock::now()))
{  
}

void EngineMetaData::addMetaData(const std::string &source_name,
                                 const EDRProducerMetaData &metadata)
{
  try
  {
    itsMetaData[source_name] = metadata;
  }
  catch (...)
  {
    throw Fmi::Exception::Trace(BCP, "Operation failed!");
  }
}

const std::map<std::string, EDRProducerMetaData> &EngineMetaData::getMetaData() const
{
  return itsMetaData;
}

const EDRProducerMetaData &EngineMetaData::getMetaData(const std::string &source_name) const
{
  try
  {
    if (itsMetaData.find(source_name) != itsMetaData.end())
      return itsMetaData.at(source_name);

    return EMPTY_PRODUCER_METADATA;
  }
  catch (...)
  {
    throw Fmi::Exception::Trace(BCP, "Operation failed!");
  }
}

bool EngineMetaData::isValidCollection(const std::string &collection_name) const
{
  try
  {
    for (const auto &item : itsMetaData)
    {
      if (item.second.find(collection_name) != item.second.end())
        return true;
    }

    return false;
  }
  catch (...)
  {
    throw Fmi::Exception::Trace(BCP, "Operation failed!");
  }
}

bool EngineMetaData::isValidCollection(const std::string &source_name,
                                       const std::string &collection_name) const
{
  try
  {
    if (itsMetaData.find(source_name) == itsMetaData.end())
      return false;

    const auto &md = itsMetaData.at(source_name);

    return (md.find(collection_name) != md.end());
  }
  catch (...)
  {
    throw Fmi::Exception::Trace(BCP, "Operation failed!");
  }
}

void EngineMetaData::removeDuplicates(bool report_removal)
{
  try
	{
	  // Priority order: 1)querydata, 2)grid, 3)observations, 4)avi
	  auto qd_collections = get_collection_names(getMetaData(Q_ENGINE));
	  auto grid_collections = get_collection_names(getMetaData(GRID_ENGINE));
	  auto obs_collections = get_collection_names(getMetaData(OBS_ENGINE));
	  auto avi_collections = get_collection_names(getMetaData(AVI_ENGINE));
	  
	  auto& grid_metadata = const_cast<EDRProducerMetaData&>(getMetaData(GRID_ENGINE));
	  auto& obs_metadata = const_cast<EDRProducerMetaData&>(getMetaData(OBS_ENGINE));
	  auto& avi_metadata = const_cast<EDRProducerMetaData&>(getMetaData(AVI_ENGINE));
	  for(const auto& collection : qd_collections)
		{
		  // Remove collections that already are found in querydata
		  if(grid_metadata.find(collection) != grid_metadata.end())
			{
			  grid_metadata.erase(collection);
			  grid_collections.erase(collection);
			  if(report_removal)
				report_duplicate_collection(collection, GRID_ENGINE, Q_ENGINE);
			}
		  if(obs_metadata.find(collection) != obs_metadata.end())
			{
			  obs_metadata.erase(collection);
			  obs_collections.erase(collection);
			  if(report_removal)
				report_duplicate_collection(collection, OBS_ENGINE, Q_ENGINE);
			}
		  if(avi_metadata.find(collection) != avi_metadata.end())
			{
			  avi_metadata.erase(collection);
			  avi_collections.erase(collection);
			  if(report_removal)
				report_duplicate_collection(collection, AVI_ENGINE, Q_ENGINE);
			}
		}
	  
	  for(const auto& collection : grid_collections)
		{
		  // Remove collections that already are found in qrid engine
		  if(obs_metadata.find(collection) != obs_metadata.end())
			{
			  obs_metadata.erase(collection);
			  obs_collections.erase(collection);
			  if(report_removal)
				report_duplicate_collection(collection, OBS_ENGINE, GRID_ENGINE);
			}
		  if(avi_metadata.find(collection) != avi_metadata.end())
			{
			  avi_metadata.erase(collection);
			  avi_collections.erase(collection);
			  if(report_removal)
				report_duplicate_collection(collection, AVI_ENGINE, GRID_ENGINE);
			}
		}
	  
	  for(const auto& collection : obs_collections)
		{
		  // Remove collections that already are found in observation engine
		  if(avi_metadata.find(collection) != avi_metadata.end())
			{
			  avi_metadata.erase(collection);
			  avi_collections.erase(collection);
			  if(report_removal)
				report_duplicate_collection(collection,AVI_ENGINE, OBS_ENGINE);
			}
		}
  }
  catch (...)
  {
    throw Fmi::Exception::Trace(BCP, "Operation failed!");
  }
}

}  // namespace EDR
}  // namespace Plugin
}  // namespace SmartMet
