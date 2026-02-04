// ======================================================================
/*!
 * \brief Smartmet EDR plugin interface
 */
// ======================================================================

#pragma once

#include "Config.h"
#include "EDRMetaData.h"
#include "Engines.h"
#include "Query.h"
#include "State.h"
#include <macgyver/AtomicSharedPtr.h>

namespace SmartMet
{
namespace Plugin
{
namespace EDR
{
class PluginImpl;

class Plugin : public SmartMetPlugin
{
 public:
  Plugin() = delete;
  Plugin(const Plugin& other) = delete;
  Plugin& operator=(const Plugin& other) = delete;
  Plugin(Plugin&& other) = delete;
  Plugin& operator=(Plugin&& other) = delete;
  Plugin(Spine::Reactor* theReactor, const char* theConfig);
  ~Plugin() override;

  const std::string& getPluginName() const override;
  int getRequiredAPIVersion() const override;

  bool ready() const;

  // Get timezone information
  const Fmi::TimeZones& getTimeZones() const;
  // Get the engines
  const Engines& getEngines() const { return itsEngines; }

#ifndef WITHOUT_AVI
  const EDRProducerMetaData& getAviMetaData() const;
#endif
  EDRMetaData getProducerMetaData(const std::string& producer) const;
  bool isValidCollection(const std::string& producer) const;
  bool isValidInstance(const std::string& producer, const std::string& instanceId) const;

  const ParameterInfo &getConfigParameterInfo() const { return itsConfigParameterInfo; }

 protected:
  void init() override;
  void shutdown() override;
  void requestHandler(Spine::Reactor& theReactor,
                      const Spine::HTTP::Request& theRequest,
                      Spine::HTTP::Response& theResponse) override;

 private:
  void query(const State& theState,
             const Spine::HTTP::Request& req,
             Spine::HTTP::Response& response);

  Fmi::Cache::CacheStatistics getCacheStats() const override;

  void grouplocations(Spine::HTTP::Request& theRequest) const;

  const std::string itsModuleName;
  Config itsConfig;
  bool itsReady = false;

  Spine::Reactor* itsReactor = nullptr;

  // All engines used ny timeseries plugin
  Engines itsEngines;

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
  void checkNewDataAndNotify(const std::shared_ptr<EngineMetaData>& new_emd) const;
  std::map<std::string, Fmi::DateTime> getNotificationTimes(SourceEngine source_engine,
                                                            EngineMetaData& new_emd,
                                                            const Fmi::DateTime& now) const;

  // Locations info is read once at startup, the used subsequent queries
  SupportedProducerLocations itsSupportedLocations;

  // Parameter info read at startup from config
  ParameterInfo itsConfigParameterInfo;

  // Metadata for producers (producer name -> metadata) This must be a shared
  // pointer and must be used via atomic_load and atomic_store, since CTPP::CDT
  // is not thread safe.

  Fmi::AtomicSharedPtr<EngineMetaData> itsMetaData;

#ifndef WITHOUT_OBSERVATION
  // Observable properties read from observation engine once
  std::shared_ptr<std::vector<Engine::Observation::ObservableProperty>> itsObservableProperties;
  // Parameter name -> observable property
  std::map<std::string, const Engine::Observation::ObservableProperty*> itsObservablePropertiesMap;
#endif

  friend class QEngineQuery;
  friend class ObsEngineQuery;
  friend class GridEngineQuery;
  friend class QueryProcessingHub;

};  // class Plugin

}  // namespace EDR
}  // namespace Plugin
}  // namespace SmartMet

// ======================================================================
