#include "EDRMetaData.h"
#include "AviCollection.h"
#include "EDRQuery.h"

#include <engines/grid/Engine.h>
#include <engines/querydata/Engine.h>
#include <macgyver/Exception.h>
#include <macgyver/StringConversion.h>
#include <timeseries/TimeSeriesOutput.h>
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
#define DEFAULT_PRECISION 4
static EDRProducerMetaData EMPTY_PRODUCER_METADATA;

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

EDRProducerMetaData get_edr_metadata_qd(const Engine::Querydata::Engine &qEngine,
                                        const std::string &default_language,
                                        const ParameterInfo *pinfo,
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
      producer_emd.temporal_extent.origin_time = qmd.originTime;
      producer_emd.temporal_extent.start_time = qmd.firstTime;
      producer_emd.temporal_extent.end_time = qmd.lastTime;
      producer_emd.temporal_extent.timestep = qmd.timeStep;
      producer_emd.temporal_extent.timesteps = qmd.nTimeSteps;

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
      producer_emd.temporal_extent.origin_time =
          boost::posix_time::from_iso_string(gmd.analysisTime);
      auto begin_iter = gmd.times.begin();
      auto end_iter = gmd.times.end();
      end_iter--;
      const auto &end_time = *end_iter;
      const auto &start_time = *begin_iter;
      begin_iter++;
      const auto &start_time_plus_one = *begin_iter;
      auto timestep_duration = (boost::posix_time::from_iso_string(start_time_plus_one) -
                                boost::posix_time::from_iso_string(start_time));

      producer_emd.temporal_extent.start_time = boost::posix_time::from_iso_string(start_time);
      producer_emd.temporal_extent.end_time = boost::posix_time::from_iso_string(end_time);
      producer_emd.temporal_extent.timestep = (timestep_duration.total_seconds() / 60);
      producer_emd.temporal_extent.timesteps = gmd.times.size();

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
      boost::algorithm::to_lower(producerId);

      producer_emd.parameter_precisions["__DEFAULT_PRECISION__"] = DEFAULT_PRECISION;
      producer_emd.language = default_language;
      producer_emd.parameter_info = pinfo;
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
                                         const SupportedDataQueries &sdq,
                                         const SupportedOutputFormats &sofs,
                                         const SupportedProducerLocations &spl)

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
      producer_emd.temporal_extent.origin_time = boost::posix_time::second_clock::universal_time();
      producer_emd.temporal_extent.start_time = obs_md.period.begin();
      producer_emd.temporal_extent.end_time = obs_md.period.last();
      producer_emd.temporal_extent.timestep = obs_md.timestep;
      producer_emd.temporal_extent.timesteps = (obs_md.period.length().minutes() / obs_md.timestep);

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
    EDRMetaData edrMetaData;

    auto aviMetaData = getAviEngineMetadata(aviEngine, aviCollections);
    edrMetaData.isAviProducer = true;

    for (const auto &amd : aviMetaData)
    {
      auto const &bbox = *(amd.getBBox());

      edrMetaData.spatial_extent.bbox_xmin = *bbox.getMinX();
      edrMetaData.spatial_extent.bbox_ymin = *bbox.getMinY();
      edrMetaData.spatial_extent.bbox_xmax = *bbox.getMaxX();
      edrMetaData.spatial_extent.bbox_ymax = *bbox.getMaxY();

      // METAR's issue times are 20 and 50, for others round endtime to the start of hour
      //
      // TODO: Period length/adjustment and timestep from config

      auto now = boost::posix_time::second_clock::universal_time();
      auto timeOfDay = now.time_of_day();
      uint timeStep = 60;
      uint minutes = 0;
      auto producer = amd.getProducer();

      if (producer == "METAR")
      {
        if (timeOfDay.minutes() < 20)
        {
          now -= time_duration(1, 0, 0);
          timeOfDay = now.time_of_day();
        }
        else
          minutes = ((timeOfDay.minutes() < 50) ? 20 : 50);

        timeStep = 30;
      }

      // TODO: Set 30 day period (from config)
      //
      // UKMO's test site seems to have some limit in interval handling
      //
      // 30 day period with 30min step results e.g. into R1440/2023-01-06T13:20:00Z/PT30M,
      // which ends up 21d 6h time range at test site, ending 2023-01-27T19:20Z

      ptime endTime(now.date(), time_duration(timeOfDay.hours(), minutes, 0));
      //    ptime startTime = endTime - boost::posix_time::hours(30 * 24);
      ptime startTime = endTime - boost::posix_time::hours(21 * 24);

      edrMetaData.temporal_extent.start_time = startTime;
      edrMetaData.temporal_extent.end_time = endTime;
      edrMetaData.temporal_extent.timestep = timeStep;
      //    edrMetaData.temporal_extent.timesteps = (30 * 24 * (60 / timeStep));
      edrMetaData.temporal_extent.timesteps = (21 * 24 * (60 / timeStep));

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
      li.name = Fmi::to_string(station.getId());
      li.type = "avid";

      sls[li.id] = li;
    }

    spl[amd.getProducer()] = sls;
  }
}

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
      //		const EDRProducerMetaData& pmd = item.second;
      update_location_info(item.second, spl);
    }

    /*
update_location_info(emd.querydata, spl);
update_location_info(emd.grid, spl);
update_location_info(emd.observation, spl);
update_location_info(emd.avi, spl);
    */
  }
  catch (...)
  {
    throw Fmi::Exception::Trace(BCP, "Operation failed!");
  }
}
#endif

EngineMetaData::EngineMetaData()
{
  itsUpdateTime = std::chrono::system_clock::to_time_t(std::chrono::system_clock::now());
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

}  // namespace EDR
}  // namespace Plugin
}  // namespace SmartMet
