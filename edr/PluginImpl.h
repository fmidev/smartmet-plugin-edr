// ======================================================================
/*!
 * \brief Smartmet EDR plugin implementation (reloadable portion)
 */
// ======================================================================

#pragma once

#include "Config.h"
#include "EDRMetaData.h"
#include "Engines.h"
#include "LocationInfo.h"
#include "ParameterInfo.h"
#include "Query.h"
#include "State.h"
#include <engines/gis/GeometryStorage.h>
#include <macgyver/AsyncTask.h>
#include <macgyver/AtomicSharedPtr.h>
#include <macgyver/CacheStats.h>
#include <macgyver/DateTime.h>
#include <macgyver/TimeZones.h>
#include <spine/ConfigTools.h>
#include <spine/HTTP.h>
#include <timeseries/TimeSeriesInclude.h>
#include <map>
#include <memory>
#include <set>
#include <string>

#ifndef WITHOUT_OBSERVATION
#include <engines/observation/ObservableProperty.h>
#endif

namespace SmartMet
{
namespace Spine
{
class Reactor;
}  // namespace Spine

namespace Plugin
{
namespace EDR
{

class State;
class QEngineQuery;
class ObsEngineQuery;
class GridEngineQuery;
class AviEngineQuery;
class QueryProcessingHub;

class PluginImpl
{
 public:
  PluginImpl() = delete;
  PluginImpl(const PluginImpl&) = delete;
  PluginImpl& operator=(const PluginImpl&) = delete;
  PluginImpl(PluginImpl&&) = delete;
  PluginImpl& operator=(PluginImpl&&) = delete;

  PluginImpl(Spine::Reactor* theReactor, const char* theConfigFile);
  ~PluginImpl();

  void init();
  void shutdown();

  // Returns true if the config file has been modified since this impl was created
  bool isReloadRequired() const;

  void requestHandler(Spine::Reactor& theReactor,
                      const Spine::HTTP::Request& theRequest,
                      Spine::HTTP::Response& theResponse);
  void timeSeriesRequestHandler(Spine::Reactor& theReactor,
                                const Spine::HTTP::Request& theRequest,
                                Spine::HTTP::Response& theResponse);

  const Engines& getEngines() const { return itsEngines; }
  const Fmi::TimeZones& getTimeZones() const;
  EDRMetaData getProducerMetaData(const std::string& producer) const;
  bool isValidCollection(const std::string& producer) const;
  bool isValidInstance(const std::string& producer, const std::string& instanceId) const;
  const ParameterInfo& getConfigParameterInfo() const { return itsConfigParameterInfo; }
  Fmi::Cache::CacheStatistics getCacheStats() const;
  void grouplocations(Spine::HTTP::Request& theRequest) const;

#ifndef WITHOUT_AVI
  const EDRProducerMetaData& getAviMetaData() const;
#endif

 private:
  void query(const State& state,
             const Spine::HTTP::Request& req,
             Spine::HTTP::Response& response);
  void timeSeriesQuery(const State& state,
                       const Spine::HTTP::Request& req,
                       Spine::HTTP::Response& response);
  void metaDataUpdateLoop();
  void updateMetaData(bool initial_phase);
  void updateSupportedLocations();
  void updateParameterInfo();
  void checkNewDataAndNotify(const std::shared_ptr<EngineMetaData>& new_emd) const;
  std::map<std::string, Fmi::DateTime> getNotificationTimes(SourceEngine source_engine,
                                                             EngineMetaData& new_emd,
                                                             const Fmi::DateTime& now) const;

  Spine::Reactor* itsReactor = nullptr;
  const char* itsConfigFile = nullptr;

  Config itsConfig;
  std::string itsConfigHash;
  bool itsReady = false;

  Engines itsEngines;
  std::set<std::string> itsObsEngineStationTypes;

  mutable std::unique_ptr<TS::TimeSeriesGeneratorCache> itsTimeSeriesCache;
  Engine::Gis::GeometryStorage itsGeometryStorage;
  std::unique_ptr<Fmi::AsyncTask> itsMetaDataUpdateTask;

  SupportedProducerLocations itsSupportedLocations;
  ParameterInfo itsConfigParameterInfo;
  Fmi::AtomicSharedPtr<EngineMetaData> itsMetaData;

#ifndef WITHOUT_OBSERVATION
  std::shared_ptr<std::vector<Engine::Observation::ObservableProperty>> itsObservableProperties;
  std::map<std::string, const Engine::Observation::ObservableProperty*> itsObservablePropertiesMap;
#endif

  friend class Plugin;
  friend class QEngineQuery;
  friend class ObsEngineQuery;
  friend class GridEngineQuery;
  friend class AviEngineQuery;
  friend class QueryProcessingHub;

};  // class PluginImpl

}  // namespace EDR
}  // namespace Plugin
}  // namespace SmartMet

// ======================================================================
