// ======================================================================
/*!
 * \brief Configuration file API
 */
// ======================================================================

#pragma once

#include "Precision.h"
#include "Query.h"

#include <boost/utility.hpp>
#include <engines/gis/GeometryStorage.h>
#include <engines/grid/Engine.h>
#include <spine/Parameter.h>
#include <spine/TableFormatterOptions.h>
#include <libconfig.h++>
#include <string>

namespace SmartMet
{
namespace Plugin
{
namespace EDR
{
using Precisions = std::map<std::string, Precision>;
using SupportedQueries = std::map<std::string, std::set<std::string>>;  // producer -> queries
using ProducerKeywords = std::map<std::string, std::set<std::string>>;  // producer -> keywords

class Config : private boost::noncopyable
{
 public:
  explicit Config(const std::string &configfile);
  Config() = delete;

  const SupportedQueries &getSupportedQueries() const { return itsSupportedQueries; }
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

  const std::vector<uint> &defaultGridGeometries() { return itsDefaultGridGeometries; }

  const std::string &defaultWxmlVersion() const { return itsFormatterOptions.defaultWxmlVersion(); }
  const std::string &wxmlSchema() const { return itsFormatterOptions.wxmlSchema(); }
  const Spine::TableFormatterOptions &formatterOptions() const { return itsFormatterOptions; }
  const libconfig::Config &config() const { return itsConfig; }
  Engine::Gis::PostGISIdentifierVector getPostGISIdentifiers() const;

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

 private:
  libconfig::Config itsConfig;
  std::string itsDefaultPrecision = "normal";
  std::string itsDefaultProducerMappingName;
  std::string itsDefaultLanguage;
  std::string itsDefaultLocaleName;
  std::unique_ptr<std::locale> itsDefaultLocale;
  std::string itsDefaultTimeFormat = "iso";
  std::string itsDefaultUrl = "/edr";
  std::string itsDefaultMaxDistance = "60.0km";
  unsigned int itsExpirationTime = 60;  // seconds
  std::vector<std::string> itsParameterAliasFiles;
  std::vector<uint> itsDefaultGridGeometries;

  Spine::TableFormatterOptions itsFormatterOptions;
  Precisions itsPrecisions;

  std::map<std::string, Engine::Gis::postgis_identifier> postgis_identifiers;
  std::string itsDefaultPostGISIdentifierKey;
  bool itsObsEngineDisabled = false;
  bool itsGridEngineDisabled = false;
  std::string itsPrimaryForecastSource;
  bool itsPreventObsEngineDatabaseQuery = false;

  unsigned long long itsMaxTimeSeriesCacheSize = 10000;

  SupportedQueries itsSupportedQueries;  // producer->queries
  //  SupportedProducerLocations itsSupportedProducerLocations; //
  //  producer->locations
  ProducerKeywords itsProducerKeywords;  // producer->keywords

  bool itsMetaDataUpdatesDisabled = false;  // disable updates after initial update
  int itsMetaDataUpdateInterval = 30;       // scan interval in seconds

  // Private helper functions
  void add_default_precisions();
  void parse_config_precisions();
  void parse_config_precision(const std::string &name);
  void parse_config_locations();
  void parse_config_data_queries();
  void parse_config_geometry_tables();
  void parse_config_grid_geometries();
  void parse_config_parameter_aliases(const std::string &configfile);

};  // class Config

}  // namespace EDR
}  // namespace Plugin
}  // namespace SmartMet

// ======================================================================
