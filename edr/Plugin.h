// ======================================================================
/*!
 * \brief Smartmet TimeSeries plugin interface
 */
// ======================================================================

#pragma once

#include "Config.h"
#include "EDRMetaData.h"
#include "EDRQuery.h"
#include "LocationInfo.h"
#include "LonLatDistance.h"
#include "ObsParameter.h"
#include "Query.h"
#include "QueryLevelDataCache.h"
#include <boost/shared_ptr.hpp>
#include <boost/utility.hpp>
#include <engines/gis/GeometryStorage.h>
#include <engines/grid/Engine.h>
#include <grid-content/queryServer/definition/QueryStreamer.h>
#include <macgyver/TimeZones.h>
#include <newbase/NFmiPoint.h>
#include <newbase/NFmiSvgPath.h>
#include <spine/HTTP.h>
#include <spine/Reactor.h>
#include <spine/SmartMetPlugin.h>
#include <timeseries/TimeSeriesInclude.h>
//#include <json/json.h>
#include "Json.h"
#include <map>
#include <queue>
#include <string>
#ifndef WITHOUT_AVI
#include <engines/avi/Engine.h>
#endif

namespace SmartMet
{
namespace Engine
{
namespace Querydata
{
class Engine;
}
namespace Geonames
{
class Engine;
}
namespace Gis
{
class Engine;
}
namespace Grid
{
class Engine;
}
}  // namespace Engine

namespace Plugin
{
namespace EDR
{
class State;
class PluginImpl;
class GridInterface;

using ObsParameters = std::vector<ObsParameter>;

using EDRProducerMetaData = std::map<std::string, std::vector<EDRMetaData>>;

struct SettingsInfo
{
  Engine::Observation::Settings settings;
  bool is_area = false;
  std::string area_name;

  SettingsInfo(Engine::Observation::Settings s, bool isa, std::string an)
      : settings(std::move(s)), is_area(isa), area_name(std::move(an))
  {
  }
};

class Plugin : public SmartMetPlugin, private boost::noncopyable
{
 public:
  Plugin(Spine::Reactor *theReactor, const char *theConfig);
  ~Plugin() override;

  Plugin() = delete;
  Plugin(const Plugin &other) = delete;
  Plugin &operator=(const Plugin &other) = delete;
  Plugin(Plugin &&other) = delete;
  Plugin &operator=(Plugin &&other) = delete;

  const std::string &getPluginName() const override;
  int getRequiredAPIVersion() const override;
  bool queryIsFast(const Spine::HTTP::Request &theRequest) const override;

  bool ready() const;

  // Get timezone information
  const Fmi::TimeZones &getTimeZones() const { return itsGeoEngine->getTimeZones(); }
  // Get the engines
  const Engine::Querydata::Engine &getQEngine() const { return *itsQEngine; }
  const Engine::Geonames::Engine &getGeoEngine() const { return *itsGeoEngine; }
  const Engine::Grid::Engine *getGridEngine() const { return itsGridEngine; }
#ifndef WITHOUT_AVI
  const Engine::Avi::Engine *getAviEngine() const { return itsAviEngine; }
  const EDRProducerMetaData &getAviMetaData() const;
#endif
#ifndef WITHOUT_OBSERVATION
  // May return null
  Engine::Observation::Engine *getObsEngine() const { return itsObsEngine; }
#endif
  EDRMetaData getProducerMetaData(const std::string &producer) const;
  bool isValidCollection(const std::string &producer) const;

 protected:
  void init() override;
  void shutdown() override;
  void requestHandler(Spine::Reactor &theReactor,
                      const Spine::HTTP::Request &theRequest,
                      Spine::HTTP::Response &theResponse) override;

 private:
  std::size_t hash_value(const State &state,
                         Query masterquery,
                         const Spine::HTTP::Request &request);

  void query(const State &theState,
             const Spine::HTTP::Request &req,
             Spine::HTTP::Response &response);

  boost::shared_ptr<std::string> processQuery(const State &state,
                                              Spine::Table &data,
                                              Query &masterquery,
                                              const QueryServer::QueryStreamer_sptr &queryStreamer,
                                              size_t &product_hash);

  Json::Value processMetaDataQuery(const EDRQuery &edr_query);

  void processQEngineQuery(const State &state,
                           Query &query,
                           TS::OutputData &outputData,
                           const AreaProducers &areaproducers,
                           const ProducerDataPeriod &producerDataPeriod);
  void fetchStaticLocationValues(Query &query,
                                 Spine::Table &data,
                                 unsigned int column_index,
                                 unsigned int row_index);

  void fetchQEngineValues(const State &state,
                          const TS::ParameterAndFunctions &paramfunc,
                          const Spine::TaggedLocation &tloc,
                          Query &query,
                          const AreaProducers &areaproducers,
                          const ProducerDataPeriod &producerDataPeriod,
                          QueryLevelDataCache &queryLevelDataCache,
                          TS::OutputData &outputData,
                          TS::TimeSeriesGenerator::LocalTimeList &tlist);

  bool processGridEngineQuery(const State &state,
                              Query &masterquery,
                              TS::OutputData &outputData,
                              const QueryServer::QueryStreamer_sptr &queryStreamer,
                              const AreaProducers &areaproducers,
                              const ProducerDataPeriod &producerDataPeriod);

#ifndef WITHOUT_OBSERVATION
  bool isObsProducer(const std::string &producer) const;

  void processObsEngineQuery(const State &state,
                             Query &query,
                             TS::OutputData &outputData,
                             const AreaProducers &areaproducers,
                             const ProducerDataPeriod &producerDataPeriod,
                             const ObsParameters &obsParameters);
  void fetchObsEngineValuesForArea(const State &state,
                                   const std::string &producer,
                                   const ObsParameters &obsParameters,
                                   const std::string &areaName,
                                   Engine::Observation::Settings &settings,
                                   Query &query,
                                   TS::OutputData &outputData);
  void fetchObsEngineValuesForPlaces(const State &state,
                                     const std::string &producer,
                                     const ObsParameters &obsParameters,
                                     Engine::Observation::Settings &settings,
                                     Query &query,
                                     TS::OutputData &outputData);

  void getCommonObsSettings(Engine::Observation::Settings &settings,
                            const std::string &producer,
                            Query &query) const;
  void getObsSettings(std::vector<SettingsInfo> &settingsVector,
                      const std::string &producer,
                      const ProducerDataPeriod &producerDataPeriod,
                      const boost::posix_time::ptime &now,
                      const ObsParameters &obsParameters,
                      Query &query) const;

  bool resolveAreaStations(const Spine::LocationPtr &location,
                           const std::string &producer,
                           Query &query,
                           Engine::Observation::Settings &settings,
                           std::string &name) const;
  void resolveParameterSettings(const ObsParameters &obsParameters,
                                const Query &query,
                                const std::string &producer,
                                Engine::Observation::Settings &settings,
                                unsigned int &aggregationIntervalBehind,
                                unsigned int &aggregationIntervalAhead) const;
  void resolveTimeSettings(const std::string &producer,
                           const ProducerDataPeriod &producerDataPeriod,
                           const boost::posix_time::ptime &now,
                           Query &query,
                           unsigned int aggregationIntervalBehind,
                           unsigned int aggregationIntervalAhead,
                           Engine::Observation::Settings &settings) const;
  std::vector<ObsParameter> getObsParameters(const Query &query) const;
#endif
#ifndef WITHOUT_AVI
  void checkAviEngineQuery(const Query &query,
                           const std::vector<EDRMetaData> &edrMetaData,
                           bool locationCheck,
                           SmartMet::Engine::Avi::QueryOptions &queryOptions);
  void storeAviData(const State &state,
                    SmartMet::Engine::Avi::StationQueryData &aviData,
                    TS::OutputData &outputData);
  void processAviEngineQuery(const State &state,
                             const Query &query,
                             const EDRProducerMetaData &edrProducerMetaData,
                             const std::string &producer,
                             TS::OutputData &outputData);
#endif

  TS::TimeSeriesGenerator::LocalTimeList generateQEngineQueryTimes(
      const Query &query, const std::string &paramname) const;

  Spine::LocationPtr getLocationForArea(const Spine::TaggedLocation &tloc,
                                        const Query &query,
                                        NFmiSvgPath *svgPath = nullptr) const;
  void checkInKeywordLocations(Query &masterquery);

  Spine::LocationPtr getLocationForArea(const Spine::TaggedLocation &tloc,
                                        int radius,
                                        const Query &query,
                                        NFmiSvgPath *svgPath = nullptr) const;
  void resolveAreaLocations(Query &query, const State &state, const AreaProducers &areaproducers);

  Fmi::Cache::CacheStatistics getCacheStats() const override;

  void grouplocations(Spine::HTTP::Request &theRequest);

  const std::string itsModuleName;
  Config itsConfig;
  bool itsReady = false;

  Spine::Reactor *itsReactor = nullptr;
  Engine::Querydata::Engine *itsQEngine = nullptr;
  Engine::Geonames::Engine *itsGeoEngine = nullptr;
  Engine::Gis::Engine *itsGisEngine = nullptr;
  Engine::Grid::Engine *itsGridEngine = nullptr;
  std::unique_ptr<GridInterface> itsGridInterface;
#ifndef WITHOUT_AVI
  Engine::Avi::Engine *itsAviEngine = nullptr;
#endif
#ifndef WITHOUT_OBSERVATION
  Engine::Observation::Engine *itsObsEngine = nullptr;
#endif

  // station types (producers) supported by observation
  std::set<std::string> itsObsEngineStationTypes;

  // Cached time series
  mutable std::unique_ptr<TS::TimeSeriesGeneratorCache> itsTimeSeriesCache;

  // Geometries and their svg-representations are stored here
  Engine::Gis::GeometryStorage itsGeometryStorage;

  std::unique_ptr<Fmi::AsyncTask> itsMetaDataUpdateTask;

  void metaDataUpdateLoop();
  void updateMetaData(bool initial_phase);
  void updateSupportedLocations();
  void updateParameterInfo();

  // Locations info is read once at startup, the used subsequent queries
  SupportedProducerLocations itsSupportedLocations;

  // Parameter info read at startup from config
  ParameterInfo itsConfigParameterInfo;

  // Metadata for producers (producer name -> metadata) This must be a shared
  // pointer and must be used via atomic_load and atomic_store, since CTPP::CDT
  // is not thread safe.

  boost::atomic_shared_ptr<EngineMetaData> itsMetaData;
};  // class Plugin

}  // namespace EDR
}  // namespace Plugin
}  // namespace SmartMet

// ======================================================================
