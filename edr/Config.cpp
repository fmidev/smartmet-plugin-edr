// ======================================================================
/*!
 * \brief Implementation of Config
 */
// ======================================================================

#include "Config.h"
#include "Precision.h"
#include <boost/foreach.hpp>
#include <grid-files/common/GeneralFunctions.h>
#include <grid-files/common/ShowFunction.h>
#include <macgyver/Exception.h>
#include <macgyver/StringConversion.h>
#include <spine/Convenience.h>
#include <spine/Exceptions.h>
#include <ogr_geometry.h>
#include <stdexcept>

#define FUNCTION_TRACE FUNCTION_TRACE_OFF

using namespace std;

namespace SmartMet
{
namespace Plugin
{
namespace EDR
{
// ----------------------------------------------------------------------
/*!
 * \brief Add default precisions if none were configured
 */
// ----------------------------------------------------------------------

void Config::add_default_precisions()
{
  try
  {
    Precision prec;
    itsPrecisions.insert(Precisions::value_type("double", prec));
    itsDefaultPrecision = "double";
  }
  catch (...)
  {
    throw Fmi::Exception::Trace(BCP, "Operation failed!");
  }
}

// ----------------------------------------------------------------------
/*!
 * \brief Parse a parameter function setting
 */
// ----------------------------------------------------------------------

TS::FunctionId get_function_id(const string &configName)
{
  try
  {
    if (configName == "mean")
      return TS::FunctionId::Mean;

    if (configName == "max")
      return TS::FunctionId::Maximum;

    if (configName == "min")
      return TS::FunctionId::Minimum;

    if (configName == "median")
      return TS::FunctionId::Median;

    if (configName == "sum")
      return TS::FunctionId::Sum;

    if (configName == "sdev")
      return TS::FunctionId::StandardDeviation;

    if (configName == "trend")
      return TS::FunctionId::Trend;

    if (configName == "change")
      return TS::FunctionId::Change;

    if (configName == "count")
      return TS::FunctionId::Count;

    if (configName == "percentage")
      return TS::FunctionId::Percentage;

    return TS::FunctionId::NullFunction;
  }
  catch (...)
  {
    throw Fmi::Exception::Trace(BCP, "Operation failed!");
  }
}

// ----------------------------------------------------------------------
/*!
 * \brief Parse a named precision setting
 */
// ----------------------------------------------------------------------

void Config::parse_config_precision(const string &name)
{
  try
  {
    string optname = "precision." + name;

    if (!itsConfig.exists(optname))
      throw Fmi::Exception(BCP,
                           string("Precision settings for ") + name +
                               " are missing from pointforecast configuration");

    libconfig::Setting &settings = itsConfig.lookup(optname);

    if (!settings.isGroup())
      throw Fmi::Exception(BCP,
                           "Precision settings for point forecasts must "
                           "be stored in groups delimited by {}: line " +
                               Fmi::to_string(settings.getSourceLine()));

    Precision prec;

    for (int i = 0; i < settings.getLength(); ++i)
    {
      string paramname = settings[i].getName();

      try
      {
        int value = settings[i];

        if (paramname == "default")
          prec.default_precision = value;
        else
          prec.parameter_precisions.insert(Precision::Map::value_type(paramname, value));
      }
      catch (...)
      {
        Spine::Exceptions::handle("EDR plugin");
      }
    }

    // This line may crash if prec.parameters is empty
    // Looks like a bug in g++

    itsPrecisions.insert(Precisions::value_type(name, prec));
  }
  catch (...)
  {
    throw Fmi::Exception::Trace(BCP, "Operation failed!");
  }
}

// ----------------------------------------------------------------------
/*!
 * \brief Parse precisions listed in precision.enabled
 */
// ----------------------------------------------------------------------

void Config::parse_config_precisions()
{
  try
  {
    if (!itsConfig.exists("precision"))
      add_default_precisions();  // set sensible defaults
    else
    {
      // Require available precisions in

      if (!itsConfig.exists("precision.enabled"))
        throw Fmi::Exception(BCP,
                             "precision.enabled missing from pointforecast congiguration file");

      libconfig::Setting &enabled = itsConfig.lookup("precision.enabled");
      if (!enabled.isArray())
      {
        throw Fmi::Exception(BCP,
                             "precision.enabled must be an array in "
                             "pointforecast configuration file line " +
                                 Fmi::to_string(enabled.getSourceLine()));
      }

      for (int i = 0; i < enabled.getLength(); ++i)
      {
        const char *name = enabled[i];
        if (i == 0)
          itsDefaultPrecision = name;
        parse_config_precision(name);
      }

      if (itsPrecisions.empty())
        throw Fmi::Exception(BCP, "No precisions defined in pointforecast precision: datablock!");
    }
  }
  catch (...)
  {
    throw Fmi::Exception::Trace(BCP, "Operation failed!");
  }
}

string parse_config_key(const char *str1 = nullptr,
                        const char *str2 = nullptr,
                        const char *str3 = nullptr)
{
  try
  {
    string string1(str1 ? str1 : "");
    string string2(str2 ? str2 : "");
    string string3(str3 ? str3 : "");

    string retval(string1 + string2 + string3);

    return retval;
  }
  catch (...)
  {
    throw Fmi::Exception::Trace(BCP, "Operation failed!");
  }
}

// ----------------------------------------------------------------------
/*!
 * \brief Parse locations settings
 */
// ----------------------------------------------------------------------

void Config::parse_config_locations()
{
  try
  {
    if (itsConfig.exists("locations"))
    {
      if (itsConfig.exists("locations.default"))
      {
        const libconfig::Setting &defaultKeywords = itsConfig.lookup("locations.default");
        if (!defaultKeywords.isArray())
          throw Fmi::Exception(BCP, "Configured value of 'locations.default' must be an array");
        for (int i = 0; i < defaultKeywords.getLength(); i++)
          itsProducerKeywords[DEFAULT_PRODUCER_KEY].insert(defaultKeywords[i]);
      }
      if (itsConfig.exists("locations.override"))
      {
        const libconfig::Setting &overriddenProducerKeywords =
            itsConfig.lookup("locations.override");
        for (int i = 0; i < overriddenProducerKeywords.getLength(); i++)
        {
          std::string producer = overriddenProducerKeywords[i].getName();
          const libconfig::Setting &overriddenKeywords =
              itsConfig.lookup("locations.override." + producer);
          if (!overriddenKeywords.isArray())
            throw Fmi::Exception(BCP,
                                 "Configured overridden locations for "
                                 "collection must be an array");
          for (int j = 0; j < overriddenKeywords.getLength(); j++)
            itsProducerKeywords[producer].insert(overriddenKeywords[j]);
        }
      }
    }
  }
  catch (const libconfig::SettingNotFoundException &e)
  {
    throw Fmi::Exception(BCP, "Setting not found").addParameter("Setting path", e.getPath());
  }
  catch (...)
  {
    throw Fmi::Exception(BCP, "Operation failed!", nullptr);
  }
}

void Config::parse_config_data_queries()
{
  try
  {
    if (itsConfig.exists("data_queries"))
    {
      if (itsConfig.exists("data_queries.default"))
      {
        const libconfig::Setting &defaultQueries = itsConfig.lookup("data_queries.default");
        if (!defaultQueries.isArray())
          throw Fmi::Exception(BCP, "Configured value of 'data_queries.default' must be an array");
        for (int i = 0; i < defaultQueries.getLength(); i++)
          itsSupportedQueries[DEFAULT_DATA_QUERIES].insert(defaultQueries[i]);
      }

      if (itsConfig.exists("data_queries.override"))
      {
        const libconfig::Setting &overriddenQueries = itsConfig.lookup("data_queries.override");

        for (int i = 0; i < overriddenQueries.getLength(); i++)
        {
          std::string producer = overriddenQueries[i].getName();
          const libconfig::Setting &overrides =
              itsConfig.lookup("data_queries.override." + producer);
          if (!overrides.isArray())
            throw Fmi::Exception(BCP,
                                 "Configured overridden data_queries for "
                                 "producer must be an array");
          for (int j = 0; j < overrides.getLength(); j++)
            itsSupportedQueries[producer].insert(overrides[j]);
        }
      }
    }

    if (itsSupportedQueries.find(DEFAULT_DATA_QUERIES) == itsSupportedQueries.end())
    {
      itsSupportedQueries[DEFAULT_DATA_QUERIES].insert("position");
      itsSupportedQueries[DEFAULT_DATA_QUERIES].insert("radius");
      itsSupportedQueries[DEFAULT_DATA_QUERIES].insert("area");
      itsSupportedQueries[DEFAULT_DATA_QUERIES].insert("locations");
    }
  }
  catch (const libconfig::SettingNotFoundException &e)
  {
    throw Fmi::Exception(BCP, "Setting not found").addParameter("Setting path", e.getPath());
  }
  catch (...)
  {
    throw Fmi::Exception(BCP, "Operation failed!", nullptr);
  }
}

// ----------------------------------------------------------------------
/*!
 * \brief Parse geometry table settings
 */
// ----------------------------------------------------------------------

void Config::parse_config_geometry_tables()
{
  try
  {
    if (itsConfig.exists("geometry_tables"))
    {
      Engine::Gis::postgis_identifier default_postgis_id;
      std::string default_source_name;
      std::string default_server;
      std::string default_schema;
      std::string default_table;
      std::string default_field;
      itsConfig.lookupValue("geometry_tables.name", default_source_name);
      itsConfig.lookupValue("geometry_tables.server", default_server);
      itsConfig.lookupValue("geometry_tables.schema", default_schema);
      itsConfig.lookupValue("geometry_tables.table", default_table);
      itsConfig.lookupValue("geometry_tables.field", default_field);
      if (!default_schema.empty() && !default_table.empty() && !default_field.empty())
      {
        default_postgis_id.source_name = default_source_name;
        default_postgis_id.pgname = default_server;
        default_postgis_id.schema = default_schema;
        default_postgis_id.table = default_table;
        default_postgis_id.field = default_field;
        postgis_identifiers.insert(make_pair(default_postgis_id.key(), default_postgis_id));
      }

      if (itsConfig.exists("geometry_tables.additional_tables"))
      {
        libconfig::Setting &additionalTables =
            itsConfig.lookup("geometry_tables.additional_tables");

        for (int i = 0; i < additionalTables.getLength(); i++)
        {
          libconfig::Setting &tableConfig = additionalTables[i];
          std::string source_name;
          std::string server = (default_server.empty() ? "" : default_server);
          std::string schema = (default_schema.empty() ? "" : default_schema);
          std::string table = (default_table.empty() ? "" : default_table);
          std::string field = (default_field.empty() ? "" : default_field);
          tableConfig.lookupValue("name", source_name);
          tableConfig.lookupValue("server", server);
          tableConfig.lookupValue("schema", schema);
          tableConfig.lookupValue("table", table);
          tableConfig.lookupValue("field", field);

          Engine::Gis::postgis_identifier postgis_id;
          postgis_id.source_name = source_name;
          postgis_id.pgname = server;
          postgis_id.schema = schema;
          postgis_id.table = table;
          postgis_id.field = field;

          if (schema.empty() || table.empty() || field.empty())
            throw Fmi::Exception(BCP,
                                 "Configuration file error. Some of the following fields "
                                 "missing: server, schema, table, field!");

          postgis_identifiers.insert(std::make_pair(postgis_id.key(), postgis_id));
        }
      }
    }
  }
  catch (const libconfig::SettingNotFoundException &e)
  {
    throw Fmi::Exception(BCP, "Setting not found").addParameter("Setting path", e.getPath());
  }
  catch (...)
  {
    throw Fmi::Exception(BCP, "Operation failed!", nullptr);
  }
}

void Config::parse_config_grid_geometries()
{
  const libconfig::Setting &gridGeometries = itsConfig.lookup("defaultGridGeometries");
  if (!gridGeometries.isArray())
    throw Fmi::Exception(BCP, "Configured value of 'defaultGridGeometries' must be an array");

  for (int i = 0; i < gridGeometries.getLength(); ++i)
  {
    uint geomId = gridGeometries[i];
    itsDefaultGridGeometries.push_back(geomId);
  }
}

void Config::parse_config_parameter_aliases(const std::string &configfile)
{
  const libconfig::Setting &aliasFiles = itsConfig.lookup("parameterAliasFiles");

  if (!aliasFiles.isArray())
    throw Fmi::Exception(BCP, "Configured value of 'parameterAliasFiles' must be an array");

  boost::filesystem::path path(configfile);

  for (int i = 0; i < aliasFiles.getLength(); ++i)
  {
    std::string st = aliasFiles[i];
    if (!st.empty())
    {
      if (st[0] == '/')
        itsParameterAliasFiles.push_back(st);
      else
        itsParameterAliasFiles.push_back(path.parent_path().string() + "/" + st);
    }
  }

  itsAliasFileCollection.init(itsParameterAliasFiles);
}

// ----------------------------------------------------------------------
/*!
 * \brief Constructor
 */
// ----------------------------------------------------------------------

Config::Config(const string &configfile)
{
  try
  {
    if (configfile.empty())
      throw Fmi::Exception(BCP, "EDR configuration file cannot be empty");

    boost::filesystem::path p = configfile;
    p.remove_filename();
    itsConfig.setIncludeDir(p.c_str());

    itsConfig.readFile(configfile.c_str());

    // Metadata update settings
    itsConfig.lookupValue("metadata_updates_disabled", itsMetaDataUpdatesDisabled);
    itsConfig.lookupValue("metadata_update_interval", itsMetaDataUpdateInterval);

    // Obligatory settings
    itsDefaultLocaleName = itsConfig.lookup("locale").c_str();
    itsDefaultLanguage = itsConfig.lookup("language").c_str();

    if (itsDefaultLocaleName.empty())
      throw Fmi::Exception(BCP, "Default locale name cannot be empty");

    if (itsDefaultLanguage.empty())
      throw Fmi::Exception(BCP, "Default language code cannot be empty");

    // Optional settings
    itsConfig.lookupValue("timeformat", itsDefaultTimeFormat);
    itsConfig.lookupValue("url", itsDefaultUrl);
    itsConfig.lookupValue("expires", itsExpirationTime);
    itsConfig.lookupValue("observation_disabled", itsObsEngineDisabled);
    itsConfig.lookupValue("gridengine_disabled", itsGridEngineDisabled);
    itsConfig.lookupValue("primaryForecastSource", itsPrimaryForecastSource);
    itsConfig.lookupValue("prevent_observation_database_query", itsPreventObsEngineDatabaseQuery);

    if (itsConfig.exists("maxdistance"))
    {
      double value = 0;
      itsConfig.lookupValue("maxdistance", value);
      itsDefaultMaxDistance = Fmi::to_string(value) + "km";
    }

	// Request limits
	int maxlocations = 0;
	int maxparameters = 0;
	int maxtimes = 0;
	int maxlevels = 0;
	int maxelements = 0;
	itsConfig.lookupValue("request_limits.maxlocations", maxlocations);
	itsConfig.lookupValue("request_limits.maxparameters", maxparameters);
	itsConfig.lookupValue("request_limits.maxtimes", maxtimes);
	itsConfig.lookupValue("request_limits.maxlevels", maxlevels);
	itsConfig.lookupValue("request_limits.maxelements", maxelements);
	itsRequestLimits.maxlocations = maxlocations;
	itsRequestLimits.maxparameters = maxparameters;
	itsRequestLimits.maxtimes = maxtimes;
	itsRequestLimits.maxlevels = maxlevels;
	itsRequestLimits.maxelements = maxelements;

    // TODO: Remove deprecated settings detection
    using Spine::log_time_str;
    if (itsConfig.exists("cache.memory_bytes"))
      std::cerr << log_time_str() << " Warning: cache.memory_bytes setting is deprecated\n";
    if (itsConfig.exists("cache.filesystem_bytes"))
      std::cerr << log_time_str() << " Warning: cache.filesystem_bytes setting is deprecated\n";
    if (itsConfig.exists("cache.directory"))
      std::cerr << log_time_str() << " Warning: cache.directory setting is deprecated\n";

    itsConfig.lookupValue("cache.timeseries_size", itsMaxTimeSeriesCacheSize);
    itsFormatterOptions = Spine::TableFormatterOptions(itsConfig);

    parse_config_precisions();

    parse_config_locations();

    // If keywords not defined -> set default keyword
    if (itsProducerKeywords.empty())
      itsProducerKeywords[DEFAULT_PRODUCER_KEY].insert(DEFAULT_LOCATIONS_KEYWORD);

    parse_config_data_queries();

    parse_config_geometry_tables();

    // We construct the default locale only once from the string,
    // creating it from scratch for every request is very expensive
    itsDefaultLocale.reset(new std::locale(itsDefaultLocaleName.c_str()));

    itsLastAliasCheck = time(nullptr);
    try
    {
      itsConfig.lookupValue("defaultProducerMappingName", itsDefaultProducerMappingName);
      parse_config_grid_geometries();
      parse_config_parameter_aliases(configfile);
    }
    catch (const libconfig::SettingNotFoundException &e)
    {
      // throw Fmi::Exception(BCP, "Setting not found").addParameter("Setting
      // path", e.getPath());
    }
  }
  catch (const libconfig::SettingNotFoundException &e)
  {
    throw Fmi::Exception(BCP, "Setting not found").addParameter("Setting path", e.getPath());
  }
  catch (...)
  {
    throw Fmi::Exception(BCP, "Operation failed!", nullptr);
  }
}

// ----------------------------------------------------------------------
/*!
 * \brief Get the precision settings for the given name
 */
// ----------------------------------------------------------------------

const Precision &Config::getPrecision(const string &name) const
{
  try
  {
    auto p = itsPrecisions.find(name);
    if (p != itsPrecisions.end())
      return p->second;

    throw Fmi::Exception(BCP, "Unknown precision '" + name + "'!");
  }
  catch (...)
  {
    throw Fmi::Exception::Trace(BCP, "Operation failed!");
  }
}

Engine::Gis::PostGISIdentifierVector Config::getPostGISIdentifiers() const
{
  try
  {
    Engine::Gis::PostGISIdentifierVector ret;
    for (const auto &item : postgis_identifiers)
      ret.push_back(item.second);
    return ret;
  }
  catch (...)
  {
    throw Fmi::Exception::Trace(BCP, "Operation failed!");
  }
}

unsigned long long Config::maxTimeSeriesCacheSize() const
{
  return itsMaxTimeSeriesCacheSize;
}

}  // namespace EDR
}  // namespace Plugin
}  // namespace SmartMet

// ======================================================================
