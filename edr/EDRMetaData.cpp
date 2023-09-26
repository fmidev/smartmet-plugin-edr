#include "EDRMetaData.h"
#ifndef WITHOUT_AVI
#include "AviCollection.h"
#include <engines/avi/Engine.h>
#endif
#include "EDRQuery.h"

#include <engines/grid/Engine.h>
#include <engines/querydata/Engine.h>
#include <engines/querydata/MetaQueryOptions.h>
#include <macgyver/AnsiEscapeCodes.h>
#include <macgyver/Exception.h>
#include <macgyver/StringConversion.h>
#include <spine/Convenience.h>
#include <timeseries/TimeSeriesOutput.h>
#ifndef WITHOUT_OBSERVATION
#include <engines/observation/Engine.h>
#include <engines/observation/ObservableProperty.h>
#endif
#include <boost/any.hpp>

namespace SmartMet
{
namespace Plugin
{
namespace EDR
{
#define MAX_TIME_PERIODS 300
#define DEFAULT_PRECISION 4
static EDRProducerMetaData EMPTY_PRODUCER_METADATA;
static boost::posix_time::ptime NOT_A_DATE_TIME;
class TimePeriod
{
 public:
  TimePeriod(const boost::posix_time::ptime &t1, const boost::posix_time::ptime &t2)
      : period_start(t1), period_end(t2)
  {
  }
  void setStartTime(const boost::posix_time::ptime &t) { period_start = t; }
  void setEndTime(const boost::posix_time::ptime &t) { period_end = t; }
  const boost::posix_time::ptime &getStartTime() const { return period_start; }
  const boost::posix_time::ptime &getEndTime() const { return period_end; }
  int length() const
  {
    if (period_start.is_not_a_date_time() || period_end.is_not_a_date_time())
      return 0;
    return (period_end - period_start).total_seconds();
  }

 private:
  boost::posix_time::ptime period_start = boost::posix_time::not_a_date_time;
  boost::posix_time::ptime period_end = boost::posix_time::not_a_date_time;
};
namespace
{
std::set<std::string> get_collection_names(const EDRProducerMetaData &md)
{
  std::set<std::string> collection_names;
  for (const auto &item : md)
    collection_names.insert(item.first);

  return collection_names;
}

void report_duplicate_collection(const std::string &collection,
                                 const std::string &duplicate_source,
                                 const std::string &original_source)
{
  std::cerr << (Spine::log_time_str() + ANSI_FG_MAGENTA + " [edr] Duplicate collection '" +
                collection + "' removed from " + duplicate_source +
                ", it has already been defined in " + original_source + ANSI_FG_DEFAULT)
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

// Remove duplicate collection on secondary engine's metadata
void remove_duplicate_collection(const std::string &collection_name,
                                 const std::string &secondary_engine,
                                 const std::string &primary_engine,
                                 std::set<std::string> &secondary_engine_collection_names,
                                 EDRProducerMetaData &secondary_engine_metadata,
                                 bool report_removal)
{
  // If collection with the same name found in secondary engine, remove it
  if (secondary_engine_metadata.find(collection_name) != secondary_engine_metadata.end())
  {
    secondary_engine_metadata.erase(collection_name);
    secondary_engine_collection_names.erase(collection_name);
    // Report that 'collection_name' has been removed from 'secondary_engine'
    if (report_removal)
      report_duplicate_collection(collection_name, secondary_engine, primary_engine);
  }
}

}  // namespace

const boost::posix_time::ptime &get_latest_data_update_time(const EDRProducerMetaData &pmd,
                                                            const std::string &producer)
{
  if (pmd.find(producer) != pmd.end())
  {
    // Latest update time is same for all metadata instances of the same producer
    return pmd.at(producer).front().latest_data_update_time;
  }
  return NOT_A_DATE_TIME;
}

EDRProducerMetaData get_edr_metadata_qd(const Engine::Querydata::Engine &qEngine,
                                        const std::string &default_language,
                                        const ParameterInfo *pinfo,
                                        const CollectionInfoContainer &cic,
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

    std::map<std::string, boost::posix_time::ptime> latest_update_times;
    // Iterate QEngine metadata and add items into collection
    for (const auto &qmd : qd_meta_data)
    {
      if (!cic.isVisibleCollection(SourceEngine::Querydata, qmd.producer))
        continue;

      EDRMetaData producer_emd;
      producer_emd.metadata_source = SourceEngine::Querydata;

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
      producer_emd.collection_info = &cic.getInfo(SourceEngine::Querydata, qmd.producer);
      producer_emd.data_queries = get_supported_data_queries(qmd.producer, sdq);
      producer_emd.output_formats = get_supported_output_formats(qmd.producer, sofs);

      auto producer_key =
          (spl.find(qmd.producer) != spl.end() ? qmd.producer : DEFAULT_PRODUCER_KEY);
      if (spl.find(producer_key) != spl.end())
        producer_emd.locations = &spl.at(producer_key);
      epmd[qmd.producer].push_back(producer_emd);
      // Update latest data update time
      if (latest_update_times.find(qmd.producer) == latest_update_times.end() ||
          latest_update_times.at(qmd.producer) < qmd.originTime)
        latest_update_times[qmd.producer] = qmd.originTime;
    }
    for (auto &item : epmd)
    {
      for (auto &producer_meta_data : item.second)
        producer_meta_data.latest_data_update_time = latest_update_times.at(item.first);
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
                                          const CollectionInfoContainer &cic,
                                          const SupportedDataQueries &sdq,
                                          const SupportedOutputFormats &sofs,
                                          const SupportedProducerLocations &spl)
{
  try
  {
    EDRProducerMetaData epmd;

    auto grid_meta_data = gEngine.getEngineMetadata("");

    std::map<std::string, boost::posix_time::ptime> latest_update_times;
    // Iterate Grid metadata and add items into collection
    for (auto &gmd : grid_meta_data)
    {
      std::string producerId = (gmd.producerName + "." + Fmi::to_string(gmd.geometryId) + "." +
                                Fmi::to_string(gmd.levelId));

      if (!cic.isVisibleCollection(SourceEngine::Grid, producerId))
        continue;

      EDRMetaData producer_emd;
      producer_emd.metadata_source = SourceEngine::Grid;

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
      if (gmd.times.size() > 1)
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
            edr_parameter(
                parameter_name, p.parameterDescription, p.parameterUnits, p.parameterUnits)));
      }

      producer_emd.collection_info_engine.title =
          ("Producer: " + gmd.producerName + "; Geometry id: " + Fmi::to_string(gmd.geometryId) +
           "; Level id: " + Fmi::to_string(gmd.levelId));
      producer_emd.collection_info_engine.description = gmd.producerDescription;
      producer_emd.collection_info_engine.keywords.insert(gmd.producerName);

      boost::algorithm::to_lower(producerId);

      producer_emd.parameter_precisions["__DEFAULT_PRECISION__"] = DEFAULT_PRECISION;
      producer_emd.language = default_language;
      producer_emd.parameter_info = pinfo;
      producer_emd.collection_info = &cic.getInfo(SourceEngine::Grid, gmd.producerName);

      producer_emd.data_queries = get_supported_data_queries(gmd.producerName, sdq);
      producer_emd.output_formats = get_supported_output_formats(gmd.producerName, sofs);
      auto producer_key =
          (spl.find(gmd.producerName) != spl.end() ? gmd.producerName : DEFAULT_PRODUCER_KEY);
      if (spl.find(producer_key) != spl.end())
        producer_emd.locations = &spl.at(producer_key);

      epmd[producerId].push_back(producer_emd);
      // Update latest data update time
      auto origin_time = boost::posix_time::from_iso_string(gmd.analysisTime);
      if (latest_update_times.find(producerId) == latest_update_times.end() ||
          latest_update_times.at(producerId) < origin_time)
        latest_update_times[producerId] = origin_time;
    }
    for (auto &item : epmd)
    {
      for (auto &producer_meta_data : item.second)
        producer_meta_data.latest_data_update_time = latest_update_times.at(item.first);
    }

    return epmd;
  }
  catch (...)
  {
    throw Fmi::Exception::Trace(BCP, "Operation failed!");
  }
}

#ifndef WITHOUT_OBSERVATION

std::set<std::string> get_producer_parameters(const std::string &producer,
                                              const Engine::Observation::ProducerMeasurandInfo &pmi)
{
  try
  {
    std::set<std::string> ret;

    if (pmi.find(producer) != pmi.end())
    {
      const auto &mi = pmi.at(producer);
      for (const auto &item : mi)
        ret.insert(item.first);
    }

    return ret;
  }
  catch (...)
  {
    throw Fmi::Exception::Trace(BCP, "Operation failed!");
  }
}

bool is_external_producer(const std::string &producer)
{
  try
  {
    return (producer == "netatmo" || producer == "roadcloud" || producer == "teconer" ||
            producer == "fmi_iot");
  }
  catch (...)
  {
    throw Fmi::Exception::Trace(BCP, "Operation failed!");
  }
}

const Engine::Observation::measurand_info *get_measurand_info(
    const std::string &producer,
    const std::string &parameter_name,
    const Engine::Observation::ProducerMeasurandInfo &producer_measurand_info)
{
  try
  {
    const Engine::Observation::measurand_info *mi = nullptr;
    if (producer_measurand_info.find(producer) != producer_measurand_info.end())
    {
      const auto &measurands = producer_measurand_info.at(producer);
      if (measurands.find(parameter_name) != measurands.end())
        mi = &measurands.at(parameter_name);
    }
    return mi;
  }
  catch (...)
  {
    throw Fmi::Exception::Trace(BCP, "Operation failed!");
  }
}

void get_label_unit_description(
    const std::string &parameter_name,
    const std::map<std::string, const Engine::Observation::ObservableProperty *>
        &observable_properties,
    const Engine::Observation::measurand_info *mi,
    const std::string &default_language,
    std::string &label,
    std::string &unit,
    std::string &description)
{
  try
  {
    if (observable_properties.find(parameter_name) != observable_properties.end())
    {
      const auto &properties = observable_properties.at(parameter_name);
      description = properties->observablePropertyLabel;
      label = properties->uom;
      unit = properties->uom;
      /*
            auto prop_desc =
            (properties->measurandId+" -- "+ properties->measurandCode+" -- "+
            properties->observablePropertyId+" -- "+
            properties->observablePropertyLabel+" -- "+
            properties->uom+" -- "+
            properties->statisticalMeasureId+" -- "+
            properties->statisticalFunction+" -- "+
            properties->aggregationTimePeriod+" -- "+
            properties->gmlId);
            std::cout << "desc: " << prop_desc << std::endl;
      */
    }
    else if (mi)
    {
      description = mi->get_description(default_language);
      label = mi->get_label(default_language);
    }
  }
  catch (...)
  {
    throw Fmi::Exception::Trace(BCP, "Operation failed!");
  }
}

void process_parameters(const std::string &producer,
                        const std::set<std::string> &params,
                        const std::map<std::string, const Engine::Observation::ObservableProperty *>
                            &observable_properties,
                        const Engine::Observation::ProducerMeasurandInfo &producer_measurand_info,
                        const std::string &default_language,
                        EDRMetaData &producer_emd)
{
  try
  {
    for (const auto &p : params)
    {
      auto parameter_name = p;
      boost::algorithm::to_lower(parameter_name);
      std::string description = p;
      std::string label = p;
      std::string unit = "";

      // Exclude external producers
      if (!is_external_producer(producer))
      {
        const Engine::Observation::measurand_info *mi =
            get_measurand_info(producer, parameter_name, producer_measurand_info);

        get_label_unit_description(
            parameter_name, observable_properties, mi, default_language, label, unit, description);
      }
      producer_emd.parameter_names.insert(parameter_name);
      producer_emd.parameters.insert(
          std::make_pair(parameter_name, edr_parameter(p, description, unit, label)));
    }
  }
  catch (...)
  {
    throw Fmi::Exception::Trace(BCP, "Operation failed!");
  }
}

EDRProducerMetaData get_edr_metadata_obs(
    Engine::Observation::Engine &obsEngine,
    const std::string &default_language,
    const ParameterInfo *pinfo,
    const std::map<std::string, const Engine::Observation::ObservableProperty *>
        &observable_properties,
    const CollectionInfoContainer &cic,
    const SupportedDataQueries &sdq,
    const SupportedOutputFormats &sofs,
    const SupportedProducerLocations &spl,
    unsigned int observation_period)

{
  try
  {
    std::map<std::string, Engine::Observation::MetaData> observation_meta_data;
    auto producers = obsEngine.getValidStationTypes();

    for (const auto &prod : producers)
    {
      if (cic.isVisibleCollection(SourceEngine::Observation, prod))
        observation_meta_data.insert(std::make_pair(prod, obsEngine.metaData(prod)));
    }

    const auto &producer_measurand_info = obsEngine.getMeasurandInfo();

    EDRProducerMetaData epmd;

    // Iterate Observation engine metadata and add item into collection
    for (const auto &item : observation_meta_data)
    {
      const auto &producer = item.first;
      const auto &obs_md = item.second;
      auto params = obs_md.parameters;
      auto measurand_params = get_producer_parameters(producer, producer_measurand_info);

      params.insert(measurand_params.begin(), measurand_params.end());

      EDRMetaData producer_emd;
      producer_emd.metadata_source = SourceEngine::Observation;

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
      boost::posix_time::ptime end_time(
          end_date,
          boost::posix_time::time_duration(time_of_day.hours(), time_of_day.minutes(), 0));
      if (observation_period > 0)
        temporal_extent_period.start_time =
            (end_time - boost::posix_time::hours(observation_period));
      else
        temporal_extent_period.start_time = obs_md.period.begin();
      temporal_extent_period.end_time = end_time;
      temporal_extent_period.timestep = obs_md.timestep;
      temporal_extent_period.timesteps = (obs_md.period.length().minutes() / obs_md.timestep);
      temporal_extent.time_periods.push_back(temporal_extent_period);
      producer_emd.temporal_extent = temporal_extent;

      process_parameters(producer,
                         params,
                         observable_properties,
                         producer_measurand_info,
                         default_language,
                         producer_emd);

      producer_emd.parameter_precisions["__DEFAULT_PRECISION__"] = DEFAULT_PRECISION;
      producer_emd.language = default_language;
      producer_emd.parameter_info = pinfo;
      producer_emd.collection_info = &cic.getInfo(SourceEngine::Observation, producer);
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
std::vector<TimePeriod> get_time_periods(
    const std::set<boost::local_time::local_date_time> &timesteps)
{
  try
  {
    std::vector<TimePeriod> time_periods;

    auto it_previous = timesteps.begin();
    auto it = timesteps.begin();
    it++;
    while (it != timesteps.end())
    {
      time_periods.push_back({it_previous->utc_time(), it->utc_time()});
      it_previous++;
      it++;
    }

    return time_periods;
  }
  catch (...)
  {
    throw Fmi::Exception::Trace(BCP, "Operation failed!");
  }
}

bool parse_merged_time_periods(const std::vector<TimePeriod> &time_periods,
                               const TimePeriod &tp_current,
                               TimePeriod &tp,
                               unsigned int &i,
                               std::vector<edr_temporal_extent_period> &merged_time_periods,
                               int &first_period_length,
                               bool &multiple_periods)
{
  try
  {
    if (first_period_length != tp_current.length() || i == time_periods.size() - 1)
    {
      if (multiple_periods)
      {
        if (i == time_periods.size() - 1)
          tp.setEndTime(tp_current.getEndTime());
        edr_temporal_extent_period etep;
        etep.start_time = tp.getStartTime();
        etep.end_time = tp.getEndTime();
        etep.timestep = first_period_length / 60;  // seconds to minutes
        etep.timesteps = ((etep.end_time - etep.start_time).total_seconds() / first_period_length);
        merged_time_periods.push_back(etep);
        multiple_periods = false;
      }
      else
      {
        merged_time_periods.clear();
        return true;
      }
      i++;
      if (i < time_periods.size() - 1)
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
    return false;
  }
  catch (...)
  {
    throw Fmi::Exception::Trace(BCP, "Operation failed!");
  }
}

std::vector<edr_temporal_extent_period> get_merged_time_periods(
    const std::vector<TimePeriod> &time_periods)
{
  try
  {
    std::vector<edr_temporal_extent_period> merged_time_periods;

    TimePeriod tp = time_periods.at(0);
    int first_period_length = tp.length();
    bool multiple_periods = false;
    for (unsigned int i = 1; i < time_periods.size(); i++)
    {
      const auto tp_current = time_periods.at(i);
      bool break_here = parse_merged_time_periods(time_periods,
                                                  tp_current,
                                                  tp,
                                                  i,
                                                  merged_time_periods,
                                                  first_period_length,
                                                  multiple_periods);
      if (break_here)
        break;
    }

    return merged_time_periods;
  }
  catch (...)
  {
    throw Fmi::Exception::Trace(BCP, "Operation failed!");
  }
}

void parse_temporal_extent(const std::set<boost::local_time::local_date_time> &timesteps,
                           const std::vector<TimePeriod> &time_periods,
                           edr_temporal_extent &t_extent)
{
  try
  {
    if (timesteps.size() > MAX_TIME_PERIODS)
    {
      // Insert only starttime and endtime
      edr_temporal_extent_period etep;
      etep.start_time = time_periods.front().getStartTime();
      etep.end_time = time_periods.back().getEndTime();
      etep.timestep = 0;
      etep.timesteps = 0;
      t_extent.time_periods.push_back(etep);
    }
    else
    {
      // Insert all timesteps
      for (const auto &t : timesteps)
      {
        edr_temporal_extent_period etep;
        etep.start_time = t.utc_time();
        etep.timestep = 0;
        etep.timesteps = 0;
        t_extent.time_periods.push_back(etep);
      }
    }
  }
  catch (...)
  {
    throw Fmi::Exception::Trace(BCP, "Operation failed!");
  }
}

// Merge time periods when possible (even timesteps)
edr_temporal_extent get_temporal_extent(
    const std::set<boost::local_time::local_date_time> &timesteps)
{
  try
  {
    edr_temporal_extent ret;

    if (timesteps.size() < 2)
      return ret;

    ret.origin_time = boost::posix_time::second_clock::universal_time();

    // Insert time periods into vector
    std::vector<TimePeriod> time_periods = get_time_periods(timesteps);

    // Iterate vector and find out periods with even timesteps
    std::vector<edr_temporal_extent_period> merged_time_periods =
        get_merged_time_periods(time_periods);

    if (merged_time_periods.empty())
    {
      parse_temporal_extent(timesteps, time_periods, ret);
    }
    else
    {
      ret.time_periods = merged_time_periods;
    }

    return ret;
  }
  catch (...)
  {
    throw Fmi::Exception::Trace(BCP, "Operation failed!");
  }
}

edr_temporal_extent getAviTemporalExtent(const Engine::Avi::Engine &aviEngine,
                                         const std::string &message_type,
                                         int period_length)
{
  try
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
    auto start_of_period =
        (now - boost::posix_time::hours(
                   period_length * 24));  // from config file avi.period_length (30 days default)
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

      while (timeIter != aviData.itsValues[stationId]["messagetime"].end())
      {
        boost::local_time::local_date_time timestep =
            boost::get<boost::local_time::local_date_time>(*timeIter);

        timesteps.insert(timestep);
        timeIter++;
      }
    }

    ret = get_temporal_extent(timesteps);

    return ret;
  }
  catch (...)
  {
    throw Fmi::Exception::Trace(BCP, "Operation failed!");
  }
}

void setAviQueryLocationOptions(const AviCollection &aviCollection,
                                const AviMetaData &amd,
                                SmartMet::Engine::Avi::QueryOptions &queryOptions)
{
  try
  {
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
  }
  catch (...)
  {
    throw Fmi::Exception::Trace(BCP, "Operation failed!");
  }
}

void setAviStations(const Engine::Avi::StationQueryData &stationData,
                    const AviCollection &aviCollection,
                    const std::string &icaoColumnName,
                    const std::string &latColumnName,
                    const std::string &lonColumnName,
                    AviMetaData &amd)
{
  try
  {
    bool useDataBBox = (!amd.getBBox());
    bool first = true;
    double minX = 0;
    double minY = 0;
    double maxX = 0;
    double maxY = 0;

    for (auto stationId : stationData.itsStationIds)
    {
      auto const &columns = stationData.itsValues.at(stationId);
      auto const &icaoCode = *(columns.at(icaoColumnName).cbegin());
      auto const &latitude = *(columns.at(latColumnName).cbegin());
      auto const &longitude = *(columns.at(lonColumnName).cbegin());

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
          minY = std::min(minY, lat);
          maxY = std::max(maxY, lat);

          minX = std::min(minX, lon);
          maxX = std::max(maxX, lon);
        }
      }

      amd.addStation(
          AviStation(stationId, icao, boost::get<double>(latitude), boost::get<double>(longitude)));
    }

    if (!amd.getStations().empty() && useDataBBox)
      amd.setBBox(AviBBox(minX, minY, maxX, maxY));
  }
  catch (...)
  {
    throw Fmi::Exception::Trace(BCP, "Operation failed!");
  }
}

std::list<AviMetaData> getAviEngineMetadata(const Engine::Avi::Engine &aviEngine,
                                            const AviCollections &aviCollections,
                                            const CollectionInfoContainer &cic)
{
  try
  {
    std::list<AviMetaData> aviMetaData;

    for (auto const &aviCollection : aviCollections)
    {
      if (!cic.isVisibleCollection(SourceEngine::Avi, aviCollection.getName()))
        continue;

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

      setAviQueryLocationOptions(aviCollection, amd, queryOptions);

      auto stationData = aviEngine.queryStations(queryOptions);

      setAviStations(stationData, aviCollection, icaoColumnName, latColumnName, lonColumnName, amd);
      if (!amd.getStations().empty())
        aviMetaData.push_back(amd);
    }

    return aviMetaData;
  }
  catch (...)
  {
    throw Fmi::Exception::Trace(BCP, "Operation failed!");
  }
}

EDRProducerMetaData get_edr_metadata_avi(const Engine::Avi::Engine &aviEngine,
                                         const AviCollections &aviCollections,
                                         const std::string &default_language,
                                         const ParameterInfo *pinfo,
                                         const CollectionInfoContainer &cic,
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

    auto aviMetaData = getAviEngineMetadata(aviEngine, aviCollections, cic);

    for (const auto &amd : aviMetaData)
    {
      EDRMetaData edrMetaData;
      edrMetaData.metadata_source = SourceEngine::Avi;
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

      const AviCollection &avi_collection = get_avi_collection(producer, aviCollections);

      edrMetaData.temporal_extent =
          getAviTemporalExtent(aviEngine, producer, avi_collection.getPeriodLength());

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
      edrMetaData.collection_info = &cic.getInfo(SourceEngine::Avi, producer);
      edrMetaData.data_queries = get_supported_data_queries(producer, sdq);
      edrMetaData.output_formats = get_supported_output_formats(producer, sofs);
      auto producer_key = (spl.find(producer) != spl.end() ? producer : DEFAULT_PRODUCER_KEY);
      if (spl.find(producer_key) != spl.end())
        edrMetaData.locations = &spl.at(producer_key);

      if (!edrMetaData.temporal_extent.time_periods.empty())
        edrMetaData.latest_data_update_time =
            edrMetaData.temporal_extent.time_periods.back().end_time;
      edrProducerMetaData[producer].push_back(edrMetaData);
    }

    return edrProducerMetaData;
  }
  catch (...)
  {
    throw Fmi::Exception::Trace(BCP, "Operation failed!");
  }
}

SupportedLocations get_supported_locations(const AviMetaData &amd,
                                           const AviCollections &aviCollections,
                                           SupportedProducerLocations &spl,
                                           const CollectionInfoContainer &cic)

{
  try
  {
    SupportedLocations sls;

    for (const auto &station : amd.getStations())
    {
      // TODO: Station name needed ?

      location_info li;
      li.id = station.getIcao();
      li.longitude = station.getLongitude();
      li.latitude = station.getLatitude();
      li.name = ("ICAO: " + station.getIcao() + "; stationId: " + Fmi::to_string(station.getId()));
      li.type = "ICAO";
      li.keyword = amd.getProducer();

      sls[li.id] = li;
    }

    return sls;
  }
  catch (...)
  {
    throw Fmi::Exception::Trace(BCP, "Operation failed!");
  }
}

void load_locations_avi(const Engine::Avi::Engine &aviEngine,
                        const AviCollections &aviCollections,
                        SupportedProducerLocations &spl,
                        const CollectionInfoContainer &cic)

{
  try
  {
    auto aviMetaData = getAviEngineMetadata(aviEngine, aviCollections, cic);

    for (const auto &amd : aviMetaData)
    {
      SupportedLocations sls = get_supported_locations(amd, aviCollections, spl, cic);

      spl[amd.getProducer()] = sls;
    }
  }
  catch (...)
  {
    throw Fmi::Exception::Trace(BCP, "Operation failed!");
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

EngineMetaData::EngineMetaData()
    : itsMetaDataUpdateTime(std::chrono::system_clock::to_time_t(std::chrono::system_clock::now()))
{
}

void EngineMetaData::addMetaData(SourceEngine source_engine, const EDRProducerMetaData &metadata)
{
  try
  {
    itsMetaData[source_engine] = metadata;
  }
  catch (...)
  {
    throw Fmi::Exception::Trace(BCP, "Operation failed!");
  }
}

const std::map<SourceEngine, EDRProducerMetaData> &EngineMetaData::getMetaData() const
{
  return itsMetaData;
}

const EDRProducerMetaData &EngineMetaData::getMetaData(SourceEngine source_engine) const
{
  try
  {
    if (itsMetaData.find(source_engine) != itsMetaData.end())
      return itsMetaData.at(source_engine);

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

bool EngineMetaData::isValidCollection(SourceEngine source_engine,
                                       const std::string &collection_name) const
{
  try
  {
    if (itsMetaData.find(source_engine) == itsMetaData.end())
      return false;

    const auto &md = itsMetaData.at(source_engine);

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
    auto qd_collections = get_collection_names(getMetaData(SourceEngine::Querydata));
    auto grid_collections = get_collection_names(getMetaData(SourceEngine::Grid));
    auto obs_collections = get_collection_names(getMetaData(SourceEngine::Observation));
    auto avi_collections = get_collection_names(getMetaData(SourceEngine::Avi));

    auto &grid_metadata = const_cast<EDRProducerMetaData &>(getMetaData(SourceEngine::Grid));
    auto &obs_metadata = const_cast<EDRProducerMetaData &>(getMetaData(SourceEngine::Observation));
    auto &avi_metadata = const_cast<EDRProducerMetaData &>(getMetaData(SourceEngine::Avi));
    for (const auto &collection : qd_collections)
    {
      // Remove collections that already are found in querydata
      remove_duplicate_collection(
          collection, GRID_ENGINE, Q_ENGINE, grid_collections, grid_metadata, report_removal);
      remove_duplicate_collection(
          collection, OBS_ENGINE, Q_ENGINE, obs_collections, obs_metadata, report_removal);
      remove_duplicate_collection(
          collection, AVI_ENGINE, Q_ENGINE, avi_collections, avi_metadata, report_removal);
    }

    for (const auto &collection : grid_collections)
    {
      // Remove collections that already are found in grid engine
      remove_duplicate_collection(
          collection, OBS_ENGINE, GRID_ENGINE, obs_collections, obs_metadata, report_removal);
      remove_duplicate_collection(
          collection, AVI_ENGINE, GRID_ENGINE, avi_collections, avi_metadata, report_removal);
    }

    for (const auto &collection : obs_collections)
    {
      // Remove collections that already are found in observation engine
      remove_duplicate_collection(
          collection, AVI_ENGINE, OBS_ENGINE, avi_collections, avi_metadata, report_removal);
    }
  }
  catch (...)
  {
    throw Fmi::Exception::Trace(BCP, "Operation failed!");
  }
}

const boost::posix_time::ptime &EngineMetaData::getLatestDataUpdateTime(
    SourceEngine source_engine, const std::string &producer) const
{
  const auto &producer_meta_data = getMetaData(source_engine);

  return get_latest_data_update_time(producer_meta_data, producer);
}

void EngineMetaData::setLatestDataUpdateTime(SourceEngine source_engine,
                                             const std::string &producer,
                                             const boost::posix_time::ptime &t)
{
  if (itsMetaData.find(source_engine) != itsMetaData.end())
  {
    auto &engine_meta_data = itsMetaData.at(source_engine);
    if (engine_meta_data.find(producer) != engine_meta_data.end())
    {
      auto &producer_meta_data = engine_meta_data.at(producer);
      for (auto &md : producer_meta_data)
        md.latest_data_update_time = t;
    }
  }
}

std::string get_engine_name(SourceEngine source_engine)
{
  if (source_engine == SourceEngine::Querydata)
    return Q_ENGINE;
  if (source_engine == SourceEngine::Observation)
    return OBS_ENGINE;
  if (source_engine == SourceEngine::Grid)
    return GRID_ENGINE;
  if (source_engine == SourceEngine::Avi)
    return AVI_ENGINE;

  return "";
}

}  // namespace EDR
}  // namespace Plugin
}  // namespace SmartMet
