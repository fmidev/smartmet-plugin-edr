// ======================================================================
/*!
 * \brief Configuration file API
 */
// ======================================================================

#pragma once

#include "AviCollection.h"
#include "CollectionInfo.h"
#include "EDRAPI.h"
#include "EDRDefs.h"
#include "Precision.h"
#include "ProducerParameters.h"

#include <boost/utility.hpp>
#include <engines/gis/GeometryStorage.h>
#include <engines/grid/Engine.h>
#include <spine/Parameter.h>
#include <spine/TableFormatterOptions.h>
#include <timeseries/RequestLimits.h>
#include <libconfig.h++>
#include <string>

namespace SmartMet
{
namespace Plugin
{
namespace EDR
{
class Config : private boost::noncopyable
{
 public:
  ~Config() = default;
  explicit Config(const std::string &configfile);
  Config() = delete;
  Config(const Config &other) = delete;
  Config &operator=(const Config &other) = delete;
  Config(Config &&other) = delete;
  Config &operator=(Config &&other) = delete;

  const SmartMet::TimeSeries::RequestLimits &requestLimits() const { return itsRequestLimits; };
  const SupportedOutputFormats &allSupportedOutputFormats() const
  {
    return itsSupportedOutputFormats;
  }
  unsigned int getObservationPeriod() const { return itsObservationPeriod; }
  const SupportedDataQueries &allSupportedDataQueries() const { return itsSupportedDataQueries; }
  const std::set<std::string> &getSupportedOutputFormats(const std::string &producer) const;
  const std::set<std::string> &getSupportedDataQueries(const std::string &producer) const;
  const ProducerKeywords &getProducerKeywords() { return itsProducerKeywords; }
  const Precision &getPrecision(const std::string &name) const;

  const std::string &defaultPrecision() const { return itsDefaultPrecision; }
  const std::string &defaultProducerMappingName() const { return itsDefaultProducerMappingName; }
  const std::string &defaultLanguage() const { return itsDefaultLanguage; }
  // You can copy the locale, not modify it!
  const std::locale &defaultLocale() const { return *itsDefaultLocale; }
  const std::string &defaultLocaleName() const { return itsDefaultLocaleName; }
  const std::string &defaultTimeFormat() const { return itsDefaultTimeFormat; }
  const std::string &defaultUrl() const { return itsDefaultUrl; }
  const std::string &defaultMaxDistance() const { return itsDefaultMaxDistance; }
  const std::string &defaultWxmlTimeString() const
  {
    return itsFormatterOptions.defaultWxmlTimeString();
  }

  const std::vector<uint> &defaultGridGeometries() const { return itsDefaultGridGeometries; }

  const std::string &defaultWxmlVersion() const { return itsFormatterOptions.defaultWxmlVersion(); }
  const std::string &wxmlSchema() const { return itsFormatterOptions.wxmlSchema(); }
  const Spine::TableFormatterOptions &formatterOptions() const { return itsFormatterOptions; }
  const libconfig::Config &config() const { return itsConfig; }
  Engine::Gis::PostGISIdentifierVector getPostGISIdentifiers() const;

  bool aviEngineDisabled() const { return itsAviEngineDisabled; }
  bool obsEngineDisabled() const { return itsObsEngineDisabled; }
  bool gridEngineDisabled() const { return itsGridEngineDisabled; }
  std::string primaryForecastSource() const { return itsPrimaryForecastSource; }
  bool obsEngineDatabaseQueryPrevented() const { return itsPreventObsEngineDatabaseQuery; }

  unsigned long long maxTimeSeriesCacheSize() const;

  unsigned int expirationTime() const { return itsExpirationTime; }
  bool metaDataUpdatesDisabled() const { return itsMetaDataUpdatesDisabled; }
  int metaDataUpdateInterval() const { return itsMetaDataUpdateInterval; }

  QueryServer::AliasFileCollection itsAliasFileCollection;
  time_t itsLastAliasCheck;

  const AviCollections &getAviCollections() const { return itsAviCollections; }
  const EDRAPI &getEDRAPI() const { return itsEDRAPI; }
  const CollectionInfoContainer &getCollectionInfo() const { return itsCollectionInfo; }
  const ProducerParameters &getProducerParameters() const { return itsProducerParameters; }

 private:
  libconfig::Config itsConfig;
  std::string itsDefaultPrecision = "normal";
  std::string itsDefaultProducerMappingName;
  std::string itsDefaultLanguage;
  std::string itsDefaultLocaleName;
  std::unique_ptr<std::locale> itsDefaultLocale;
  std::string itsDefaultTimeFormat = "iso";
  std::string itsDefaultUrl = "/edr/";
  std::string itsDefaultMaxDistance = "60.0km";
  unsigned int itsExpirationTime = 60;  // seconds
  std::vector<std::string> itsParameterAliasFiles;
  std::vector<uint> itsDefaultGridGeometries;

  Spine::TableFormatterOptions itsFormatterOptions;
  Precisions itsPrecisions;

  std::map<std::string, Engine::Gis::postgis_identifier> postgis_identifiers;
  std::string itsDefaultPostGISIdentifierKey;
  bool itsAviEngineDisabled = false;
  bool itsObsEngineDisabled = false;
  bool itsGridEngineDisabled = false;
  std::string itsPrimaryForecastSource;
  bool itsPreventObsEngineDatabaseQuery = false;
  unsigned int itsObservationPeriod = 0;

  unsigned long long itsMaxTimeSeriesCacheSize = 10000;
  SmartMet::TimeSeries::RequestLimits itsRequestLimits;

  SupportedOutputFormats itsSupportedOutputFormats;  // producer->output format
  SupportedDataQueries itsSupportedDataQueries;      // producer->queries
  ProducerKeywords itsProducerKeywords;              // producer->keywords

  bool itsMetaDataUpdatesDisabled = false;  // disable updates after initial update
  int itsMetaDataUpdateInterval = 30;       // scan interval in seconds

  AviCollections itsAviCollections;
  CollectionInfoContainer itsCollectionInfo;
  ProducerParameters itsProducerParameters;
  mutable EDRAPI itsEDRAPI;

  // Private helper functions
  void add_default_precisions();
  void parse_config_precisions();
  void parse_config_precision(const std::string &name);
  void parse_config_locations();
  void parse_config_data_queries();
  void parse_config_output_formats();
  void parse_config_geometry_tables();
  void parse_config_avi_collections();
  void parse_config_avi_collection_countries(AviCollection &aviCollection, const std::string &path);
  void parse_config_avi_collection_bbox(AviCollection &aviCollection, const std::string &path);
  void parse_config_avi_collection_icaos(AviCollection &aviCollection, const std::string &path);
  void parse_config_avi_collection_icaofilters(AviCollection &aviCollection,
                                               const std::string &path);
  void parse_config_grid_geometries();
  void parse_config_parameter_aliases(const std::string &configfile);
  void parse_config_api_settings();
  void parse_config_collection_info();
  void process_collection_info(SourceEngine source_engine);
  void parse_visible_collections(const SourceEngine &source_engine);

};  // class Config

}  // namespace EDR
}  // namespace Plugin
}  // namespace SmartMet

// ======================================================================
