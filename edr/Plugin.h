// ======================================================================
/*!
 * \brief Smartmet TimeSeries plugin interface
 */
// ======================================================================

#pragma once

#include "Config.h"
#include "LonLatDistance.h"
#include "ObsParameter.h"
#include "Query.h"
#include "QueryLevelDataCache.h"
#include "EDRMetaData.h"
#include "EDRQuery.h"
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
#include <json/json.h>
#include <map>
#include <queue>
#include <string>

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

struct SettingsInfo
{
  Engine::Observation::Settings settings;
  bool is_area = false;
  std::string area_name;

  SettingsInfo(const Engine::Observation::Settings& s, bool isa, const std::string& an)
      : settings(s), is_area(isa), area_name(an)
  {
  }
};

class Plugin : public SmartMetPlugin, private boost::noncopyable
{
 public:
  Plugin(Spine::Reactor* theReactor, const char* theConfig);
  virtual ~Plugin();

  const std::string& getPluginName() const override;
  int getRequiredAPIVersion() const override;
  bool queryIsFast(const Spine::HTTP::Request& theRequest) const override;

  bool ready() const;

  // Get timezone information
  const Fmi::TimeZones& getTimeZones() const { return itsGeoEngine->getTimeZones(); }
  // Get the engines
  const Engine::Querydata::Engine& getQEngine() const { return *itsQEngine; }
  const Engine::Geonames::Engine& getGeoEngine() const { return *itsGeoEngine; }
  const Engine::Grid::Engine* getGridEngine() const { return itsGridEngine; }

#ifndef WITHOUT_OBSERVATION
  // May return null
  Engine::Observation::Engine* getObsEngine() const { return itsObsEngine; }
#endif

 protected:
  void init() override;
  void shutdown() override;
  void requestHandler(Spine::Reactor& theReactor,
                      const Spine::HTTP::Request& theRequest,
                      Spine::HTTP::Response& theResponse) override;

 private:
  Plugin();

  std::size_t hash_value(const State& state,
                         Query masterquery,
                         const Spine::HTTP::Request& request);

  void query(const State& theState,
             const Spine::HTTP::Request& req,
             Spine::HTTP::Response& response);

  boost::shared_ptr<std::string> processQuery(const State& state,
                                              Spine::Table& data,
                                              Query& masterquery,
                                              const QueryServer::QueryStreamer_sptr& queryStreamer,
                                              size_t& product_hash);

  EDRMetaData getProducerMetaData(const std::string& producer);
  Json::Value processMetaDataQuery(const EDRQuery& edr_query);

  void processQEngineQuery(const State& state,
                           Query& query,
                           TS::OutputData& outputData,
                           const AreaProducers& areaproducers,
                           const ProducerDataPeriod& producerDataPeriod);
  void fetchStaticLocationValues(Query& query,
                                 Spine::Table& data,
                                 unsigned int column_index,
                                 unsigned int row_index);

  void fetchQEngineValues(const State& state,
                          const TS::ParameterAndFunctions& paramfunc,
                          const Spine::TaggedLocation& tloc,
                          Query& query,
                          const AreaProducers& areaproducers,
                          const ProducerDataPeriod& producerDataPeriod,
                          QueryLevelDataCache& queryLevelDataCache,
                          TS::OutputData& outputData);

  bool processGridEngineQuery(const State& state,
                              Query& masterquery,
                              TS::OutputData& outputData,
                              QueryServer::QueryStreamer_sptr queryStreamer,
                              const AreaProducers& areaproducers,
                              const ProducerDataPeriod& producerDataPeriod);

#ifndef WITHOUT_OBSERVATION
  bool isObsProducer(const std::string& producer) const;

  void processObsEngineQuery(const State& state,
                             Query& query,
                             TS::OutputData& outputData,
                             const AreaProducers& areaproducers,
                             const ProducerDataPeriod& producerDataPeriod,
                             const ObsParameters& obsParameters);
  void fetchObsEngineValuesForArea(const State& state,
                                   const std::string& producer,
                                   const ObsParameters& obsParameters,
                                   const std::string& areaName,
                                   Engine::Observation::Settings& settings,
                                   Query& query,
                                   TS::OutputData& outputData);
  void fetchObsEngineValuesForPlaces(const State& state,
                                     const std::string& producer,
                                     const ObsParameters& obsParameters,
                                     Engine::Observation::Settings& settings,
                                     Query& query,
                                     TS::OutputData& outputData);

  void getCommonObsSettings(Engine::Observation::Settings& settings,
                            const std::string& producer,
                            Query& query) const;
  void getObsSettings(std::vector<SettingsInfo>& settingsVector,
                      const std::string& producer,
                      const ProducerDataPeriod& producerDataPeriod,
                      const boost::posix_time::ptime& now,
                      const ObsParameters& obsParameters,
                      Query& query) const;

  bool resolveAreaStations(const Spine::LocationPtr& location,
                           const std::string& producer,
                           Query& query,
                           Engine::Observation::Settings& settings,
                           std::string& name) const;
  void resolveParameterSettings(const ObsParameters& obsParameters,
                                const Query& query,
                                const std::string& producer,
                                Engine::Observation::Settings& settings,
                                unsigned int& aggregationIntervalBehind,
                                unsigned int& aggregationIntervalAhead) const;
  void resolveTimeSettings(const std::string& producer,
                           const ProducerDataPeriod& producerDataPeriod,
                           const boost::posix_time::ptime& now,
                           Query& query,
                           unsigned int aggregationIntervalBehind,
                           unsigned int aggregationIntervalAhead,
                           Engine::Observation::Settings& settings) const;
  std::vector<ObsParameter> getObsParameters(const Query& query) const;
#endif

  TS::TimeSeriesGenerator::LocalTimeList generateQEngineQueryTimes(
      const Query& query, const std::string& paramname) const;

  Spine::LocationPtr getLocationForArea(const Spine::TaggedLocation& tloc,
                                        const Query& query,
                                        NFmiSvgPath* svgPath = nullptr) const;
  void checkInKeywordLocations(Query& masterquery);

  Spine::LocationPtr getLocationForArea(const Spine::TaggedLocation& tloc,
                                        int radius,
                                        const Query& query,
                                        NFmiSvgPath* svgPath = nullptr) const;
  void resolveAreaLocations(Query& query, const State& state, const AreaProducers& areaproducers);

  Fmi::Cache::CacheStatistics getCacheStats() const override;

  void grouplocations(Spine::HTTP::Request& theRequest);

  const std::string itsModuleName;
  Config itsConfig;
  bool itsReady = false;

  Spine::Reactor* itsReactor = nullptr;
  Engine::Querydata::Engine* itsQEngine = nullptr;
  Engine::Geonames::Engine* itsGeoEngine = nullptr;
  Engine::Gis::Engine* itsGisEngine = nullptr;
  Engine::Grid::Engine* itsGridEngine = nullptr;
  std::unique_ptr<GridInterface> itsGridInterface;
#ifndef WITHOUT_OBSERVATION
  Engine::Observation::Engine* itsObsEngine = nullptr;
#endif

  // station types (producers) supported by observation
  std::set<std::string> itsObsEngineStationTypes;

  // Cached time series
  mutable std::unique_ptr<TS::TimeSeriesGeneratorCache> itsTimeSeriesCache;

  // Geometries and their svg-representations are stored here
  Engine::Gis::GeometryStorage itsGeometryStorage;
};  // class Plugin

}  // namespace EDR
}  // namespace Plugin
}  // namespace SmartMet

// ======================================================================
