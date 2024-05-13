// ======================================================================
/*!
 * \brief Implementation of Config
 */
// ======================================================================

#include "Config.h"
#include "EDRQuery.h"
#include "Precision.h"
#include <boost/foreach.hpp>
#include <grid-files/common/GeneralFunctions.h>
#include <grid-files/common/ShowFunction.h>
#include <macgyver/Exception.h>
#include <macgyver/StringConversion.h>
#include <spine/ConfigTools.h>
#include <spine/Convenience.h>
#include <spine/Exceptions.h>
#include <timeseries/TimeSeriesInclude.h>
#include <mutex>
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
namespace
{
void check_setting_isstring(const libconfig::Setting &setting, const std::string &name)
{
  if (setting.getType() != libconfig::Setting::Type::TypeString)
    throw Fmi::Exception(BCP, "Configuration file error. " + name + " must be a string");
}

void check_setting_isgroup(const libconfig::Setting &setting, const std::string &name)
{
  if (!setting.isGroup())
    throw Fmi::Exception(BCP, "Configuration file error. " + name + " must be an object");
}

void check_setting_isarray(const libconfig::Setting &setting, const std::string &name)
{
  if (!setting.isArray())
    throw Fmi::Exception(BCP, "Configuration file error. " + name + " must be an array");
}

}  // namespace

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
        throw Fmi::Exception(BCP, "precision.enabled missing from EDR congiguration file");

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
    if (!itsConfig.exists("locations"))
      return;

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
      const libconfig::Setting &overriddenProducerKeywords = itsConfig.lookup("locations.override");
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
          itsSupportedDataQueries[DEFAULT_DATA_QUERIES].insert(defaultQueries[i]);
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
            itsSupportedDataQueries[producer].insert(overrides[j]);
        }
      }
    }

    if (itsSupportedDataQueries.find(DEFAULT_DATA_QUERIES) == itsSupportedDataQueries.end())
    {
      itsSupportedDataQueries[DEFAULT_DATA_QUERIES].insert("position");
      itsSupportedDataQueries[DEFAULT_DATA_QUERIES].insert("radius");
      itsSupportedDataQueries[DEFAULT_DATA_QUERIES].insert("area");
      itsSupportedDataQueries[DEFAULT_DATA_QUERIES].insert("locations");
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

void Config::parse_config_output_formats()
{
  try
  {
    if (itsConfig.exists("output_formats"))
    {
      if (itsConfig.exists("output_formats.default"))
      {
        const libconfig::Setting &defaultOutputFormats = itsConfig.lookup("output_formats.default");
        if (!defaultOutputFormats.isArray())
          throw Fmi::Exception(BCP,
                               "Configured value of 'output_formats.default' must be an array");
        for (int i = 0; i < defaultOutputFormats.getLength(); i++)
          itsSupportedOutputFormats[DEFAULT_OUTPUT_FORMATS].insert(defaultOutputFormats[i]);
      }

      if (itsConfig.exists("output_formats.override"))
      {
        const libconfig::Setting &overriddenOutputFormats =
            itsConfig.lookup("output_formats.override");

        for (int i = 0; i < overriddenOutputFormats.getLength(); i++)
        {
          std::string producer = overriddenOutputFormats[i].getName();
          const libconfig::Setting &overrides =
              itsConfig.lookup("output_formats.override." + producer);
          if (!overrides.isArray())
            throw Fmi::Exception(BCP,
                                 "Configured overridden output_formats for "
                                 "producer must be an array");
          for (int j = 0; j < overrides.getLength(); j++)
            itsSupportedOutputFormats[producer].insert(overrides[j]);
        }
      }
    }

    if (itsSupportedOutputFormats.find(DEFAULT_OUTPUT_FORMATS) == itsSupportedOutputFormats.end())
    {
      itsSupportedOutputFormats[DEFAULT_OUTPUT_FORMATS].insert(COVERAGE_JSON_FORMAT);
      itsSupportedOutputFormats[DEFAULT_OUTPUT_FORMATS].insert(GEO_JSON_FORMAT);
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
    if (!itsConfig.exists("geometry_tables"))
      return;

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
      libconfig::Setting &additionalTables = itsConfig.lookup("geometry_tables.additional_tables");

      for (int i = 0; i < additionalTables.getLength(); i++)
      {
        libconfig::Setting &tableConfig = additionalTables[i];

        Engine::Gis::postgis_identifier postgis_id;
        postgis_id.pgname = default_server;
        postgis_id.schema = default_schema;
        postgis_id.table = default_table;
        postgis_id.field = default_field;

        tableConfig.lookupValue("name", postgis_id.source_name);
        tableConfig.lookupValue("server", postgis_id.pgname);
        tableConfig.lookupValue("schema", postgis_id.schema);
        tableConfig.lookupValue("table", postgis_id.table);
        tableConfig.lookupValue("field", postgis_id.field);

        if (postgis_id.schema.empty() || postgis_id.table.empty() || postgis_id.field.empty())
          throw Fmi::Exception(BCP,
                               "Configuration file error. Some of the following fields "
                               "missing: server, schema, table, field!");

        postgis_identifiers.insert(std::make_pair(postgis_id.key(), postgis_id));
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

void Config::parse_config_api_settings()
{
  try
  {
    std::string api_key = "api";
    std::string api_items_key = "api.items";

    if (!itsConfig.exists(api_key))
      return;

    if (!itsConfig.exists(api_key + ".items"))
      throw Fmi::Exception(BCP, "Configuration file error. EDR api settings must contain 'items'!");

    libconfig::Setting &edr_api = itsConfig.lookup(api_key);

    std::string templatedir = "/usr/share/smartmet/edr";
    edr_api.lookupValue("templatedir", templatedir);
    Fmi::trim(templatedir);

    if (templatedir.empty())
      throw Fmi::Exception(
          BCP, "Configuration file error. Settings 'api.templatedir' can not be empty!");

    libconfig::Setting &edr_api_items = itsConfig.lookup(api_items_key);

    if (!edr_api_items.isList())
      throw Fmi::Exception(
          BCP, "Configuration file error. Settings 'api.items' must be a list of objects");

    APISettings conf_api_settings;

    for (int i = 0; i < edr_api_items.getLength(); i++)
    {
      std::string path(api_items_key + ".[" + Fmi::to_string(i) + "]");
      libconfig::Setting &api_item = edr_api_items[i];

      if (!api_item.isGroup())
        throw Fmi::Exception(BCP, "Configuration file error. " + path + " must be an object");

      try
      {
        std::string url_setting;
        std::string template_setting;
        api_item.lookupValue("url", url_setting);
        Fmi::trim(url_setting);
        if ((url_setting.size() == 0) || (url_setting.front() != '/'))
          url_setting = itsDefaultUrl + "/" + url_setting;
        if (url_setting.size() < 2)
          throw Fmi::Exception(BCP, "url '" + url_setting + "' is not valid");
        if (url_setting.back() == '/')
          url_setting.pop_back();
        if (conf_api_settings.count(url_setting) > 0)
          throw Fmi::Exception(BCP, "url '" + url_setting + "' is duplicate");
        api_item.lookupValue("template", template_setting);
        Fmi::trim(template_setting);
        if (template_setting.empty())
          throw Fmi::Exception(BCP, "template cannot be empty");
        conf_api_settings[url_setting] = template_setting;
      }
      catch (const std::exception &e)
      {
        throw Fmi::Exception(BCP, "Configuration file error. " + path + " " + e.what());
      }
    }
    itsEDRAPI.setSettings(templatedir, conf_api_settings);
  }
  catch (...)
  {
    throw Fmi::Exception(BCP, "Operation failed!", nullptr);
  }
}

// ----------------------------------------------------------------------
/*!
 * \brief Parse AVI collection countries setting
 */
// ----------------------------------------------------------------------

void Config::parse_config_avi_collection_countries(AviCollection &aviCollection,
                                                   const std::string &path)
{
  try
  {
    std::string countriesPath = path + ".countries";
    if (!itsConfig.exists(countriesPath))
      return;

    libconfig::Setting &countrySetting = itsConfig.lookup(countriesPath);

    if (!countrySetting.isArray())
      throw Fmi::Exception(BCP, "Configuration file error. " + countriesPath + " must be an array");

    for (int j = 0; j < countrySetting.getLength(); j++)
    {
      std::string countryPath = countriesPath + ".[" + Fmi::to_string(j) + "]";

      check_setting_isstring(countrySetting[j], countryPath);

      try
      {
        aviCollection.addCountry(std::string((const char *)countrySetting[j]));
      }
      catch (const std::exception &e)
      {
        throw Fmi::Exception(BCP, "Configuration file error. " + countryPath + " " + e.what());
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

// ----------------------------------------------------------------------
/*!
 * \brief Parse AVI collection bbox setting
 */
// ----------------------------------------------------------------------

void Config::parse_config_avi_collection_bbox(AviCollection &aviCollection, const std::string &path)
{
  try
  {
    std::string bboxPath = path + ".bbox";
    if (!itsConfig.exists(bboxPath))
      return;

    libconfig::Setting &bboxSetting = itsConfig.lookup(bboxPath);

    if (!bboxSetting.isGroup())
      throw Fmi::Exception(BCP, "Configuration file error. " + bboxPath + " must be an object");

    AviBBox bbox;
    std::list<std::string> fields = {"xmin", "ymin", "xmax", "ymax"};
    int index = 0;

    for (auto const &field : fields)
    {
      std::string fieldPath = bboxPath + "." + field;

      if (!itsConfig.exists(fieldPath))
        throw Fmi::Exception(BCP, "Configuration file error. " + fieldPath + " is missing");

      libconfig::Setting &fieldSetting = itsConfig.lookup(fieldPath);
      auto type = fieldSetting.getType();

      if (type == libconfig::Setting::Type::TypeInt)
      {
        int value = 0;
        itsConfig.lookupValue(fieldPath, value);
        bbox.setField(index, static_cast<double>(value));

        // Why this does not work, value is garbage ?
        //
        // fieldSetting.lookupValue(field, value);
      }
      else if (type == libconfig::Setting::Type::TypeFloat)
      {
        double value = 0;
        itsConfig.lookupValue(fieldPath, value);
        bbox.setField(index, value);
      }
      else
        throw Fmi::Exception(BCP, "Configuration file error. " + fieldPath + " must be a number");

      index++;
    }

    try
    {
      aviCollection.setBBox(bbox);
    }
    catch (const std::exception &e)
    {
      throw Fmi::Exception(BCP, "Configuration file error. " + bboxPath + " " + e.what());
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
 * \brief Parse AVI collection icaos setting
 */
// ----------------------------------------------------------------------

void Config::parse_config_avi_collection_icaos(AviCollection &aviCollection,
                                               const std::string &path)
{
  try
  {
    std::string icaosPath = path + ".icaos";
    if (!itsConfig.exists(icaosPath))
      return;

    libconfig::Setting &icaoSetting = itsConfig.lookup(icaosPath);

    if (!icaoSetting.isArray())
      throw Fmi::Exception(BCP, "Configuration file error. " + icaosPath + " must be an array");

    for (int j = 0; j < icaoSetting.getLength(); j++)
    {
      std::string icaoPath = icaosPath + ".[" + Fmi::to_string(j) + "]";

      check_setting_isstring(icaoSetting[j], icaoPath);

      try
      {
        aviCollection.addIcao(std::string((const char *)icaoSetting[j]));
      }
      catch (const std::exception &e)
      {
        throw Fmi::Exception(BCP, "Configuration file error. " + icaoPath + " " + e.what());
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

// ----------------------------------------------------------------------
/*!
 * \brief Parse AVI collection icaofilters setting
 */
// ----------------------------------------------------------------------

void Config::parse_config_avi_collection_icaofilters(AviCollection &aviCollection,
                                                     const std::string &path)
{
  try
  {
    std::string filtersPath = path + ".icaofilters";
    if (!itsConfig.exists(filtersPath))
      return;

    libconfig::Setting &filterSetting = itsConfig.lookup(filtersPath);

    if (!filterSetting.isArray())
      throw Fmi::Exception(BCP, "Configuration file error. " + filtersPath + " must be an array");

    for (int j = 0; j < filterSetting.getLength(); j++)
    {
      std::string filterPath = filtersPath + ".[" + Fmi::to_string(j) + "]";

      check_setting_isstring(filterSetting[j], filterPath);

      try
      {
        aviCollection.addIcaoFilter(std::string((const char *)filterSetting[j]));
      }
      catch (const std::exception &e)
      {
        throw Fmi::Exception(BCP, "Configuration file error. " + filterPath + " " + e.what());
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

// ----------------------------------------------------------------------
/*!
 * \brief Parse avi collections
 */
// ----------------------------------------------------------------------

void Config::parse_config_avi_collections()
{
  try
  {
    if (!itsConfig.exists("avi"))
      return;

    int period_length = 30;
    if (itsConfig.exists("avi.period_length"))
      itsConfig.lookupValue("avi.period_length", period_length);

    std::string rootPath = "avi.collections";

    if (!itsConfig.exists(rootPath))
      return;

    libconfig::Setting &collections = itsConfig.lookup(rootPath);

    if (!collections.isList())
      throw Fmi::Exception(BCP,
                           "Configuration file error. " + rootPath + " must be a list of objects");

    for (int i = 0; i < collections.getLength(); i++)
    {
      std::string path = rootPath + ".[" + Fmi::to_string(i) + ']';
      libconfig::Setting &collectionSetting = collections[i];

      if (!collectionSetting.isGroup())
        throw Fmi::Exception(BCP, "Configuration file error. " + path + " must be an object");

      AviCollection aviCollection;

      try
      {
        std::string name;
        collectionSetting.lookupValue("name", name);
        aviCollection.setName(name);
      }
      catch (const std::exception &e)
      {
        throw Fmi::Exception(BCP, "Configuration file error. " + path + " " + e.what());
      }

      auto name = aviCollection.getName();
      for (auto const &collection : itsAviCollections)
        if (name == collection.getName())
          throw Fmi::Exception(BCP, "Configuration file error. " + path + ".name is duplicate");

      aviCollection.addMessageType(name);

      parse_config_avi_collection_countries(aviCollection, path);
      parse_config_avi_collection_bbox(aviCollection, path);
      parse_config_avi_collection_icaos(aviCollection, path);
      parse_config_avi_collection_icaofilters(aviCollection, path);

      /* Plugin checks given location against metadata anyway, no need to check by avi
      /
      std::string checkLocationsPath = path + ".checklocations";
      if (itsConfig.exists(checkLocationsPath))
      {
        bool checkLocations;
        itsConfig.lookupValue(checkLocationsPath, checkLocations);
        aviCollection.setLocationCheck(checkLocations);
      }
      */

      if (aviCollection.getCountries().empty() && !aviCollection.getBBox() &&
          aviCollection.getIcaos().empty())
        throw Fmi::Exception(
            BCP, "Configuration file error. " + path + " provides no country, bbox or icao");

      aviCollection.setPeriodLength(period_length);
      itsAviCollections.push_back(aviCollection);
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

void Config::parse_visible_collections(const SourceEngine &source_engine)
{
  try
  {
    std::set<std::string> visible_collections;
    bool engine_configured = false;
    auto engine_name = get_engine_name(source_engine);
    auto engine_collection_key = ("visible_collections." + engine_name);
    if (itsConfig.exists(engine_collection_key))
    {
      engine_configured = true;
      const libconfig::Setting &engineCollections = itsConfig.lookup(engine_collection_key);
      if (!engineCollections.isArray())
        throw Fmi::Exception(
            BCP, "Configured value of '" + engine_collection_key + "' must be an array");
      for (int i = 0; i < engineCollections.getLength(); i++)
      {
        auto collection_name = std::string((const char *)engineCollections[i]);
        if (collection_name.empty())
          continue;
        visible_collections.insert(collection_name);
      }
    }
    // If engine is not configured -> show all collections in metadata
    if (visible_collections.empty() && !engine_configured)
      visible_collections.insert("*");

    itsCollectionInfo.addVisibleCollections(source_engine, visible_collections);
  }
  catch (...)
  {
    throw Fmi::Exception(BCP, "Operation failed!", nullptr);
  }
}

void Config::process_collection_info(SourceEngine source_engine)
{
  try
  {
    std::string engine_name = get_engine_name(source_engine);
    const std::string path = ("collection_info." + engine_name);

    if (itsConfig.exists(path))
    {
      const libconfig::Setting &settings = itsConfig.lookup(path);

      if (!settings.isList())
        throw Fmi::Exception(BCP,
                             "Configuration file error. " + path + " must be a list of objects");

      for (int i = 0; i < settings.getLength(); ++i)
      {
        libconfig::Setting &collectionSettings = settings[i];
        std::string collectionPath = path + ".[" + Fmi::to_string(i) + "]";

        check_setting_isgroup(collectionSettings, collectionPath);

        std::string id;
        std::string title;
        std::string description;
        std::set<std::string> keywords;
        collectionSettings.lookupValue("id", id);
        collectionSettings.lookupValue("title", title);
        collectionSettings.lookupValue("description", description);
        if (collectionSettings.exists("keywords"))
        {
          libconfig::Setting &keywordsSetting = collectionSettings.lookup("keywords");

          check_setting_isarray(keywordsSetting, collectionPath + ".keywords");

          for (int j = 0; j < keywordsSetting.getLength(); j++)
          {
            std::string keywordsPath = collectionPath + ".keywords[" + Fmi::to_string(j) + "]";

            check_setting_isstring(keywordsSetting[j], keywordsPath);

            keywords.insert((std::string((const char *)keywordsSetting[j])));
          }
        }
        if (!id.empty() && (!title.empty() || !description.empty() || !keywords.empty()))
          itsCollectionInfo.addInfo(source_engine, id, title, description, keywords);
      }
    }

    // Visible collections
    parse_visible_collections(source_engine);
  }
  catch (...)
  {
    throw Fmi::Exception(BCP, "Operation failed!", nullptr);
  }
}

void Config::parse_config_collection_info()
{
  process_collection_info(SourceEngine::Querydata);
  process_collection_info(SourceEngine::Grid);
  process_collection_info(SourceEngine::Observation);
  process_collection_info(SourceEngine::Avi);
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
    Spine::expandVariables(itsConfig);

    // Read valid parameters for producers from config
    itsProducerParameters.init(itsConfig);

    itsConfig.lookupValue("observation_period", itsObservationPeriod);

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

    // Optional settings. Ensure base url starts with '/' but does not end with it
    itsConfig.lookupValue("timeformat", itsDefaultTimeFormat);
    itsConfig.lookupValue("url", itsDefaultUrl);
    Fmi::trim(itsDefaultUrl);
    if ((itsDefaultUrl.size() > 0) && (itsDefaultUrl.front() != '/'))
      itsDefaultUrl = "/" + itsDefaultUrl;
    if (itsDefaultUrl.size() < 2)
      throw Fmi::Exception(BCP, "EDR url '" + itsDefaultUrl + "' is not valid");
    if (itsDefaultUrl.back() == '/')
      itsDefaultUrl.pop_back();
    itsConfig.lookupValue("expires", itsExpirationTime);
    itsConfig.lookupValue("aviengine_disabled", itsAviEngineDisabled);
    itsConfig.lookupValue("observation_disabled", itsObsEngineDisabled);
    itsConfig.lookupValue("gridengine_disabled", itsGridEngineDisabled);
    itsConfig.lookupValue("primaryForecastSource", itsPrimaryForecastSource);
    itsConfig.lookupValue("prevent_observation_database_query", itsPreventObsEngineDatabaseQuery);
    itsConfig.lookupValue("pretty", itsPretty);

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

    parse_config_output_formats();
    parse_config_data_queries();
    parse_config_geometry_tables();
    parse_config_avi_collections();
    parse_config_api_settings();
    parse_config_collection_info();

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

const std::set<std::string> &Config::getSupportedOutputFormats(const std::string &producer) const
{
  try
  {
    if (itsSupportedOutputFormats.find(producer) != itsSupportedOutputFormats.end())
      return itsSupportedOutputFormats.at(producer);

    return itsSupportedOutputFormats.at(DEFAULT_OUTPUT_FORMATS);
  }
  catch (...)
  {
    throw Fmi::Exception::Trace(BCP, "Operation failed!");
  }
}

const std::set<std::string> &Config::getSupportedDataQueries(const std::string &producer) const
{
  try
  {
    if (itsSupportedDataQueries.find(producer) != itsSupportedDataQueries.end())
      return itsSupportedDataQueries.at(producer);

    return itsSupportedDataQueries.at(DEFAULT_DATA_QUERIES);
  }
  catch (...)
  {
    throw Fmi::Exception::Trace(BCP, "Operation failed!");
  }
}

}  // namespace EDR
}  // namespace Plugin
}  // namespace SmartMet

// ======================================================================
