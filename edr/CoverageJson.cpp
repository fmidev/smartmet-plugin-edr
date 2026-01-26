#include "CoverageJson.h"
#include "UtilityFunctions.h"
#include <engines/observation/Keywords.h>
#include <macgyver/Exception.h>
#include <macgyver/StringConversion.h>
#include <optional>

namespace SmartMet
{
namespace Plugin
{
namespace EDR
{
namespace CoverageJson
{
namespace
{
struct time_coord_value
{
  std::string time;
  double lon;
  double lat;
  std::optional<TS::Value> value;
};

using DataPerLevel = std::map<double, std::vector<time_coord_value>>;  // level -> array of values
using DataPerParameter = std::map<std::string, DataPerLevel>;          // parameter_name -> data
using ParameterNames = std::map<std::string, std::string>;             // parameter_name -> origname

double as_double(const TS::Value &value)
{
  return value.as_double();
}

bool lon_lat_level_param(const std::string &name)
{
  return (name == "longitude" || name == "latitude" || name == "level");
}

std::string parse_parameter_name(const std::string &name)
{
  auto parameter_name = name;

  // If ':' exists parameter_name is before first ':'
  auto pos = parameter_name.find(':');
  if (pos != std::string::npos)
    parameter_name.resize(pos);
  // Modify to lower case
  boost::algorithm::to_lower(parameter_name);

  return parameter_name;
}

Json::Value parse_temporal_extent(const edr_temporal_extent &temporal_extent)
{
  Json::Value nullvalue;
  auto temporal = Json::Value(Json::ValueType::objectValue);
  if (temporal_extent.time_periods.empty())
  {
    auto temporal_interval = Json::Value(Json::ValueType::arrayValue);
    temporal_interval[0] = nullvalue;
    temporal_interval[1] = nullvalue;
    temporal["interval"] = temporal_interval;
  }
  else
  {
    if (temporal_extent.time_periods.size() == 1)
    {
      const auto &temporal_extent_period = temporal_extent.time_periods.at(0);

      // Show only period with starttime and endtime (probably there is too many timesteps in source
      // data)
      if (temporal_extent_period.timestep == 0)
      {
        // BRAINSTORM-3314
        //
        // No temporal extent values repeating interval since timestep in unknown
        //
        auto temporal_interval = Json::Value(Json::ValueType::arrayValue);
        auto temporal_interval_array = Json::Value(Json::ValueType::arrayValue);
        temporal_interval_array[0] =
            Json::Value(Fmi::to_iso_extended_string(temporal_extent_period.start_time) + "Z");
        temporal_interval_array[1] =
            Json::Value(Fmi::to_iso_extended_string(temporal_extent_period.end_time) + "Z");
        temporal_interval[0] = temporal_interval_array;
        temporal["interval"] = temporal_interval;
      }
      else
      {
        // Time period which contains timesteps with constant length
        auto temporal_interval = Json::Value(Json::ValueType::arrayValue);
        auto temporal_interval_values = Json::Value(Json::ValueType::arrayValue);
        auto temporal_interval_array = Json::Value(Json::ValueType::arrayValue);
        temporal_interval_array[0] =
            Json::Value(Fmi::to_iso_extended_string(temporal_extent_period.start_time) + "Z");
        temporal_interval_array[1] =
            Json::Value(Fmi::to_iso_extended_string(temporal_extent_period.end_time) + "Z");
        temporal_interval[0] = temporal_interval_array;
        temporal_interval_values[0] =
            Json::Value("R" + Fmi::to_string(temporal_extent_period.timesteps) + "/" +
                        Fmi::to_iso_extended_string(temporal_extent_period.start_time) + "Z/PT" +
                        Fmi::to_string(temporal_extent_period.timestep) + "M");
        temporal["interval"] = temporal_interval;
        temporal["values"] = temporal_interval_values;
      }
    }
    else
    {
      // BRAINSTORM-3289, fix for timestep 0
      //
      // Several time periods, time periods may or may not have same time step length.
      // Nonperiodic data has time step 0
      //
      // Do not use repeating interval for single timesteps
      auto temporal_interval = Json::Value(Json::ValueType::arrayValue);
      auto temporal_interval_values = Json::Value(Json::ValueType::arrayValue);
      auto temporal_interval_array = Json::Value(Json::ValueType::arrayValue);
      for (unsigned int i = 0; i < temporal_extent.time_periods.size(); i++)
      {
        const auto &temporal_extent_period = temporal_extent.time_periods.at(i);

        if ((temporal_extent_period.timestep == 0) || (temporal_extent_period.timesteps == 1))
          temporal_interval_values[i] =
              Json::Value(Fmi::to_iso_extended_string(temporal_extent_period.start_time) + "Z");
        else
          temporal_interval_values[i] =
              Json::Value("R" + Fmi::to_string(temporal_extent_period.timesteps) + "/" +
                          Fmi::to_iso_extended_string(temporal_extent_period.start_time) + "Z/PT" +
                          Fmi::to_string(temporal_extent_period.timestep) + "M");
      }
      auto sz = temporal_extent.time_periods.size();
      const auto &first_temporal_extent_period = temporal_extent.time_periods.at(0);
      temporal_interval_array[0] =
          Json::Value(Fmi::to_iso_extended_string(first_temporal_extent_period.start_time) + "Z");
      const auto &last_temporal_extent_period = temporal_extent.time_periods.at(sz - 1);
      if (last_temporal_extent_period.timestep == 0)
        temporal_interval_array[1] =
            Json::Value(Fmi::to_iso_extended_string(last_temporal_extent_period.start_time) + "Z");
      else
        temporal_interval_array[1] =
            Json::Value(Fmi::to_iso_extended_string(last_temporal_extent_period.end_time) + "Z");
      temporal_interval[0] = temporal_interval_array;
      temporal["interval"] = temporal_interval;
      temporal["values"] = temporal_interval_values;
    }
  }

  auto trs = Json::Value(
      "TIMECRS[\"DateTime\",TDATUM[\"Gregorian "
      "Calendar\"],CS[TemporalDateTime,1],AXIS[\"Time "
      "(T)\",future]");
  temporal["trs"] = trs;

  return temporal;
}

Json::Value get_data_queries(const std::string &host,
                             const std::string &producer,
                             const std::set<std::string> &data_query_set,
                             const std::set<std::string> &output_format_set,
                             const std::string &default_output_format,
                             bool levels_exist,
                             bool instances_exist,
                             const std::string &instance_id = "")
{
  auto data_queries = Json::Value(Json::ValueType::objectValue);

  auto instance_str = (!instance_id.empty() ? ("/instances/" + instance_id) : "");

  for (const auto &qt_str : data_query_set)
  {
    auto query_type = to_query_type_id(qt_str);

    if (query_type == EDRQueryType::Cube && !levels_exist)
      continue;

    auto query_info = Json::Value(Json::ValueType::objectValue);
    auto query_info_variables = Json::Value(Json::ValueType::objectValue);
    auto query_info_link = Json::Value(Json::ValueType::objectValue);
    query_info_link["rel"] = Json::Value("data");
    query_info_link["hreflang"] = Json::Value("en");
    //      query_info_link["templated"] = Json::Value(true);

    std::string query_type_string;
    if (query_type == EDRQueryType::Position)
    {
      query_info_link["title"] = Json::Value("Position query");
      query_info_link["href"] =
          Json::Value((host + "/collections/" + producer + instance_str + "/position"));
      query_info_variables["title"] = Json::Value("Position query");
      query_info_variables["description"] = Json::Value("Data at point location");
      query_info_variables["query_type"] = Json::Value("position");
      query_info_variables["coords"] =
          Json::Value("Well Known Text POINT value i.e. POINT(24.9384 60.1699)");
      query_type_string = "position";
    }
    else if (query_type == EDRQueryType::Radius)
    {
      query_info_link["title"] = Json::Value("Radius query");
      query_info_link["href"] =
          Json::Value((host + "/collections/" + producer + instance_str + "/radius"));
      query_info_variables["title"] = Json::Value("Radius query");
      query_info_variables["description"] = Json::Value(
          "Data at the area specified with a geographic position "
          "and radial distance");
      query_info_variables["query_type"] = Json::Value("radius");
      query_info_variables["coords"] =
          Json::Value("Well Known Text POINT value i.e. POINT(24.9384 60.1699)");
      auto within_units = Json::Value(Json::ValueType::arrayValue);
      within_units[0] = Json::Value("km");
      within_units[1] = Json::Value("mi");
      query_info_variables["within_units"] = within_units;
      query_type_string = "radius";
    }
    else if (query_type == EDRQueryType::Area)
    {
      query_info_link["title"] = Json::Value("Area query");
      query_info_link["href"] =
          Json::Value((host + "/collections/" + producer + instance_str + "/area"));
      query_info_variables["title"] = Json::Value("Area query");
      query_info_variables["description"] = Json::Value("Data at the requested area");
      query_info_variables["query_type"] = Json::Value("area");
      query_info_variables["coords"] = Json::Value(
          "Well Known Text POLYGON value i.e. POLYGON((24 61,24 "
          "61.5,24.5 61.5,24.5 61,24 61))");
      query_type_string = "area";
    }
    else if (query_type == EDRQueryType::Cube)
    {
      query_info_link["title"] = Json::Value("Cube query");
      query_info_link["href"] =
          Json::Value((host + "/collections/" + producer + instance_str + "/cube"));
      query_info_variables["title"] = Json::Value("Cube query");
      query_info_variables["description"] = Json::Value("Data inside requested bounding box");
      query_info_variables["query_type"] = Json::Value("cube");
      query_info_variables["coords"] = Json::Value(
          "Well Known Text POLYGON value i.e. POLYGON((24 61,24 "
          "61.5,24.5 61.5,24.5 61,24 61))");
      query_info_variables["minz"] =
          Json::Value("Minimum level to return data for, for example 850");
      query_info_variables["maxz"] =
          Json::Value("Maximum level to return data for, for example 300");

      /*
  query_info_variables["bbox"] = Json::Value(
      "BBOX value defining lower left and upper right corner coordinates "
      "i.e. Finland: bbox=19.449758 59.693749, 31.668339 70.115572");
      */
      query_type_string = "cube";
    }
    else if (query_type == EDRQueryType::Locations)
    {
      query_info_link["title"] = Json::Value("Locations query");
      query_info_link["href"] =
          Json::Value((host + "/collections/" + producer + instance_str + "/locations"));
      query_info_variables["title"] = Json::Value("Locations query");
      query_info_variables["description"] =
          Json::Value("Data at point location defined by a unique identifier");
      query_info_variables["query_type"] = Json::Value("locations");
      query_type_string = "locations";
    }
    else if (query_type == EDRQueryType::Trajectory)
    {
      query_info_link["title"] = Json::Value("Trajectory query");
      query_info_link["href"] =
          Json::Value((host + "/collections/" + producer + instance_str + "/trajectory"));
      query_info_variables["title"] = Json::Value("Trajectory query");
      query_info_variables["description"] = Json::Value("Data along trajectory");
      query_info_variables["query_type"] = Json::Value("trajectory");
      query_info_variables["coords"] = Json::Value(
          "Well Known Text LINESTRING value i.e. LINESTRING(24 "
          "61,24.2 61.2,24.3 61.3)");
      query_type_string = "trajectory";
    }
    else if (query_type == EDRQueryType::Corridor)
    {
      query_info_link["title"] = Json::Value("Corridor query");
      query_info_link["href"] =
          Json::Value((host + "/collections/" + producer + instance_str + "/corridor"));
      query_info_variables["title"] = Json::Value("Corridor query");
      query_info_variables["description"] = Json::Value("Data within corridor");
      query_info_variables["corridor-width"] = Json::Value("Corridor width");
      query_info_variables["query_type"] = Json::Value("corridor");
      auto width_units = Json::Value(Json::ValueType::arrayValue);
      width_units[0] = Json::Value("km");
      width_units[1] = Json::Value("mi");
      query_info_variables["width-units"] = width_units;
      query_info_variables["coords"] = Json::Value(
          "Well Known Text LINESTRING value i.e. LINESTRING(24 "
          "61,24.2 61.2,24.3 61.3)");
      query_type_string = "corridor";
    }

    auto query_info_output_formats = Json::Value(Json::ValueType::arrayValue);
    query_info_output_formats[0] = Json::Value("CoverageJSON");
    unsigned int i = 0;
    for (const auto &f : output_format_set)
      query_info_output_formats[i++] = Json::Value(f);
    query_info_variables["output_formats"] = query_info_output_formats;
    query_info_variables["default_output_format"] = Json::Value(default_output_format);

    auto query_info_crs_details = Json::Value(Json::ValueType::arrayValue);
    auto query_info_crs_details_0 = Json::Value(Json::ValueType::objectValue);
    //    query_info_crs_details_0["crs"] = Json::Value("EPSG:4326");
    query_info_crs_details_0["crs"] = Json::Value("OGC:CRS84");
    query_info_crs_details_0["wkt"] = Json::Value(
        "GEOGCS[\"WGS 84\",DATUM[\"WGS_1984\",SPHEROID[\"WGS "
        "84\",6378137,298.257223563]],PRIMEM[\"Greenwich\",0],UNIT["
        "\"degree\",0.0174532925199433,AUTHORITY[\"EPSG\",\"9122\"]"
        "],AUTHORITY[\"EPSG\",\"4326\"]]");

    query_info_crs_details[0] = query_info_crs_details_0;
    query_info_variables["crs_details"] = query_info_crs_details;

    query_info_link["variables"] = query_info_variables;
    query_info["link"] = query_info_link;
    data_queries[query_type_string] = query_info;
  }

  if (instances_exist)
  {
    auto query_info = Json::Value(Json::ValueType::objectValue);
    auto query_info_variables = Json::Value(Json::ValueType::objectValue);
    auto query_info_link = Json::Value(Json::ValueType::objectValue);
    query_info_link["title"] = Json::Value("Instances query");
    query_info_link["href"] = Json::Value((host + "/collections/" + producer + "/instances"));
    query_info_link["hreflang"] = Json::Value("en");
    query_info_link["rel"] = Json::Value("collection");

    query_info_variables["title"] = Json::Value("Instances query");
    query_info_variables["query_type"] = Json::Value("instances");
    auto query_info_output_formats = Json::Value(Json::ValueType::arrayValue);
    unsigned int i = 0;
    for (const auto &f : output_format_set)
      query_info_output_formats[i++] = Json::Value(f);
    query_info_variables["default_output_format"] = Json::Value(default_output_format);

    auto query_info_crs_details = Json::Value(Json::ValueType::arrayValue);
    auto query_info_crs_details_0 = Json::Value(Json::ValueType::objectValue);
    //    query_info_crs_details_0["crs"] = Json::Value("EPSG:4326");
    query_info_crs_details_0["crs"] = Json::Value("OGC:CRS84");
    query_info_crs_details_0["wkt"] = Json::Value(
        "GEOGCS[\"WGS 84\",DATUM[\"WGS_1984\",SPHEROID[\"WGS "
        "84\",6378137,298.257223563]],PRIMEM[\"Greenwich\",0],UNIT["
        "\"degree\",0.0174532925199433,AUTHORITY[\"EPSG\",\"9122\"]"
        "],AUTHORITY[\"EPSG\",\"4326\"]]");
    query_info_crs_details[0] = query_info_crs_details_0;
    query_info_variables["output_formats"] = query_info_output_formats;
    query_info_variables["crs_details"] = query_info_crs_details;
    query_info_link["variables"] = query_info_variables;
    query_info["link"] = query_info_link;
    data_queries["instances"] = query_info;
  }

  return data_queries;
}

std::vector<TS::LonLat> get_coordinates(TS::TimeSeriesVectorPtr &tsv,
                                        const std::vector<Spine::Parameter> &query_parameters)
{
  try
  {
    std::vector<TS::LonLat> ret;

    std::vector<double> longitude_vector;
    std::vector<double> latitude_vector;

    for (unsigned int i = 0; i < tsv->size(); i++)
    {
      auto param_name = query_parameters[i].name();
      if (param_name != "longitude" && param_name != "latitude")
        continue;

      const TS::TimeSeries &ts = tsv->at(i);
      if (!ts.empty())
      {
        const auto &tv = ts.front();
        double value = as_double(tv.value);
        if (param_name == "longitude")
        {
          longitude_vector.push_back(value);
        }
        else
        {
          latitude_vector.push_back(value);
        }
      }
    }

    if (longitude_vector.size() != latitude_vector.size())
      throw Fmi::Exception(
          BCP, "Something wrong: latitude_vector.size() != longitude_vector.size()!", nullptr);

    ret.reserve(longitude_vector.size());
    for (unsigned int i = 0; i < longitude_vector.size(); i++)
      ret.emplace_back(longitude_vector.at(i), latitude_vector.at(i));

    return ret;
  }
  catch (...)
  {
    throw Fmi::Exception::Trace(BCP, "Operation failed!");
  }
}

void update_lon_lat_vector(const std::string &param_name,
                           const TS::TimeSeries &ts,
                           std::vector<double> &longitude_vector,
                           std::vector<double> &latitude_vector)
{
  try
  {
    if (!ts.empty())
    {
      const TS::TimedValue &tv = ts.at(0);
      double value = as_double(tv.value);
      if (param_name == "longitude")
        longitude_vector.push_back(value);
      else
        latitude_vector.push_back(value);
    }
  }
  catch (...)
  {
    throw Fmi::Exception::Trace(BCP, "Operation failed!");
  }
}

void update_lon_lat_vector(const std::string &param_name,
                           const TS::TimeSeriesGroupPtr &tsg,
                           std::vector<double> &longitude_vector,
                           std::vector<double> &latitude_vector)
{
  try
  {
    for (const auto &llts : *tsg)
    {
      const auto &ts = llts.timeseries;

      update_lon_lat_vector(param_name, ts, longitude_vector, latitude_vector);
    }
  }
  catch (...)
  {
    throw Fmi::Exception::Trace(BCP, "Operation failed!");
  }
}

std::vector<TS::LonLat> get_coordinates(const TS::OutputData &outputData,
                                        const std::vector<Spine::Parameter> &query_parameters)
{
  try
  {
    std::vector<TS::LonLat> ret;

    if (outputData.empty())
      return ret;

    const auto &outdata = outputData.at(0).second;

    if (outdata.empty())
      return ret;

    const auto &outdata_front = outdata.front();
    if (const auto *ptr = std::get_if<TS::TimeSeriesVectorPtr>(&outdata_front))
    {
      TS::TimeSeriesVectorPtr tsv = *ptr;
      return get_coordinates(tsv, query_parameters);
    }

    std::vector<double> longitude_vector;
    std::vector<double> latitude_vector;

    // iterate columns (parameters)
    for (unsigned int i = 0; i < outdata.size(); i++)
    {
      auto param_name = query_parameters[i].name();
      if (param_name != "longitude" && param_name != "latitude")
        continue;

      auto tsdata = outdata.at(i);
      if (const auto *ptr = std::get_if<TS::TimeSeriesPtr>(&tsdata))
      {
        TS::TimeSeriesPtr ts = *ptr;
        update_lon_lat_vector(param_name, *ts, longitude_vector, latitude_vector);
        continue;
      }

      if (std::get_if<TS::TimeSeriesVectorPtr>(&tsdata))
      {
        std::cout << "get_coordinates -> TS::TimeSeriesVectorPtr - Shouldnt be "
                     "here -> report error!!:\n"
                  << tsdata << '\n';
        continue;
      }

      if (const auto *ptr = std::get_if<TS::TimeSeriesGroupPtr>(&tsdata))
      {
        TS::TimeSeriesGroupPtr tsg = *ptr;

        update_lon_lat_vector(param_name, tsg, longitude_vector, latitude_vector);
        continue;
      }
    }

    if (longitude_vector.size() != latitude_vector.size())
      throw Fmi::Exception(
          BCP, "Something wrong: latitude_vector.size() != longitude_vector.size()!", nullptr);

    for (unsigned int i = 0; i < longitude_vector.size(); i++)
      ret.emplace_back(longitude_vector.at(i), latitude_vector.at(i));

    return ret;
  }
  catch (...)
  {
    throw Fmi::Exception::Trace(BCP, "Operation failed!");
  }
}

Json::Value parameter_metadata(const EDRMetaData &metadata,
                               const std::string &parameter_name,
                               std::optional<std::set<std::string>> &stdnames,
                               std::optional<std::set<std::string>> &methods,
                               std::optional<std::set<std::string>> &durations,
                               std::optional<std::set<float>> &levels,
                               const CustomDimReferences &custom_dim_refs,
                               const std::string &language = "")
{
  try
  {
    static const edr_parameter emptyEDRParameterInfo("", "");

    const auto &engine_parameter_info = metadata.parameters;
    const auto &config_parameter_info = *metadata.parameter_info;
    std::string standard_name_vocabulary;

    auto it = custom_dim_refs.find("standard_name");
    if (it != custom_dim_refs.end())
    {
      standard_name_vocabulary = it->second;
      if (standard_name_vocabulary.back() == '/')
        standard_name_vocabulary.pop_back();
    }

    // BRAINSTORM-3029; when fetching flash 'data_source' -parameter, expanded parameters
    // ('<column>_data_source') do not (currently) have engine_parameter info

    bool hasParam = (engine_parameter_info.find(parameter_name) != engine_parameter_info.end());
    const auto &edr_parameter =
        (hasParam ? engine_parameter_info.at(parameter_name) : emptyEDRParameterInfo);
    const auto &edr_parameter_name = (hasParam ? edr_parameter.name : parameter_name);

    auto const &lang = (language.empty() ? metadata.language : language);
    auto pinfo = config_parameter_info.get_parameter_info(parameter_name, lang);

    // Description field: 1) from config 2) from engine 3) parameter name

    std::string desc;
    std::string observed_property_id;
    const std::string &label = (pinfo.label.empty() ? edr_parameter_name : pinfo.label);
    const std::string &observed_property_label =
        (pinfo.observed_property_label.empty() ? label : pinfo.observed_property_label);

    const std::string &standard_name = pinfo.metocean.standard_name;
    if (! standard_name.empty())
      observed_property_id = standard_name_vocabulary + "/" + standard_name;
    else
      observed_property_id = edr_parameter_name;

    if (!pinfo.description.empty())
      desc = pinfo.description;
    else if (!edr_parameter.description.empty())
      desc = edr_parameter.description;
    else if (! standard_name.empty())
      desc = standard_name + " " + Fmi::to_string(pinfo.metocean.level) + "m";
    else
      desc = edr_parameter_name;

    auto unit_label = (!pinfo.unit_label.empty() ? pinfo.unit_label : edr_parameter_name);

    auto parameter = Json::Value(Json::ValueType::objectValue);
    parameter["id"] = Json::Value(edr_parameter_name);
    parameter["type"] = Json::Value("Parameter");
    // Description field is optional
    // QEngine returns parameter description in finnish and skandinavian
    // characters cause problems metoffice test interface uses description
    // field -> set parameter name to description field
    if (language.empty())
      parameter["description"] = Json::Value(desc);
    else
    {
      auto lang_desc = Json::Value(Json::ValueType::objectValue);
      lang_desc[lang] = Json::Value(desc);
      parameter["description"] = lang_desc;
    }
    if ((! pinfo.unit_symbol_value.empty()) && (! pinfo.unit_symbol_type.empty()))
    {
      parameter["unit"] = Json::Value(Json::ValueType::objectValue);
      if (language.empty())
        parameter["unit"]["label"] = Json::Value(unit_label);
      else
      {
        auto lang_label = Json::Value(Json::ValueType::objectValue);
        lang_label[lang] = Json::Value(unit_label);
        parameter["unit"]["label"] = lang_label;
      }
      parameter["unit"]["symbol"] = Json::Value(Json::ValueType::objectValue);
      parameter["unit"]["symbol"]["value"] = Json::Value(pinfo.unit_symbol_value);
      parameter["unit"]["symbol"]["type"] = Json::Value(pinfo.unit_symbol_type);
    }
    if (language.empty())
      parameter["label"] = Json::Value(label);
    else
    {
      auto lang_label = Json::Value(Json::ValueType::objectValue);
      lang_label[lang] = Json::Value(label);
      parameter["label"] = lang_label;
    }
    parameter["observedProperty"] = Json::Value(Json::ValueType::objectValue);
    parameter["observedProperty"]["id"] = Json::Value(observed_property_id);
    if (language.empty())
      parameter["observedProperty"]["label"] = Json::Value(observed_property_label);
    else
    {
      auto lang_label = Json::Value(Json::ValueType::objectValue);
      lang_label[lang] = Json::Value(observed_property_label);
      parameter["observedProperty"]["label"] = lang_label;
    }

    if (! standard_name.empty())
    {
      parameter["metocean:standard_name"] = Json::Value(standard_name);
      parameter["metocean:level"] = Json::Value(pinfo.metocean.level);

      parameter["measurementType"] = Json::Value(Json::ValueType::objectValue);
      parameter["measurementType"]["method"] = Json::Value(pinfo.metocean.method);
      parameter["measurementType"]["duration"] = Json::Value(pinfo.metocean.duration);

      if (stdnames)
      {
        stdnames->insert(standard_name);
        methods->insert(pinfo.metocean.method);
        durations->insert(pinfo.metocean.duration);
        levels->insert(pinfo.metocean.level);
      }
    }

    return parameter;
  }
  catch (...)
  {
    throw Fmi::Exception::Trace(BCP, "Operation failed!");
  }
}

Json::Value get_edr_series_parameters(const std::vector<Spine::Parameter> &query_parameters,
                                      const EDRMetaData &metadata,
                                      const CustomDimReferences &custom_dim_refs,
                                      const std::string &language)
{
  try
  {
    auto parameters = Json::Value(Json::ValueType::objectValue);
    std::string prev_param;
    auto isGridProducer = metadata.isGridProducer();

    // Custom dimensions (not used)
    std::optional<std::set<std::string>> stdnames;
    std::optional<std::set<std::string>> methods;
    std::optional<std::set<std::string>> durations;
    std::optional<std::set<float>> levels;

    for (const auto &p : query_parameters)
    {
      auto parameter_name = parse_parameter_name(p.originalName());
      boost::algorithm::to_lower(parameter_name);

      // BRAINSTORM-3116 'level' kludge, FIXME !
      //
      if (parameter_name == EDR_OBSERVATION_LEVEL)
        continue;

      if (isGridProducer)
      {
        if (parameter_name == prev_param)
          continue;

        prev_param = parameter_name;
      }

      if (! lon_lat_level_param(parameter_name))
        parameters[parameter_name] = parameter_metadata(
            metadata, parameter_name, stdnames, methods, durations, levels, custom_dim_refs,
            language);
    }

    return parameters;
  }
  catch (...)
  {
    throw Fmi::Exception::Trace(BCP, "Operation failed!");
  }
}

void add_value(const TS::TimedValue &tv,
               Json::Value &values_array,
               Json::Value &data_type,
               unsigned int values_index,
               unsigned int precision)
{
  try
  {
    const auto &val = tv.value;

    if (const double *ptr = std::get_if<double>(&val))
    {
      if (values_index == 0)
        data_type = Json::Value("float");
      values_array[values_index] = Json::Value(*ptr, precision);
    }
    else if (const int *ptr = std::get_if<int>(&val))
    {
      if (values_index == 0)
        data_type = Json::Value("int");
      values_array[values_index] = Json::Value(*ptr);
    }
    else if (const std::string *ptr = std::get_if<std::string>(&val))
    {
      if (values_index == 0)
        data_type = Json::Value("string");
      values_array[values_index] = Json::Value(*ptr);
    }
    else
    {
      Json::Value nulljson;
      if (values_index == 0)
        data_type = nulljson;
      values_array[values_index] = nulljson;
    }
  }
  catch (...)
  {
    throw Fmi::Exception::Trace(BCP, "Operation failed!");
  }
}

void add_value(const TS::TimedValue &tv,
               Json::Value &values_array,
               Json::Value &data_type,
               std::set<std::string> &timesteps,
               unsigned int values_index,
               unsigned int precision)
{
  try
  {
    const auto &t = tv.time;
    timesteps.insert(Fmi::date_time::to_iso_extended_string(t.utc_time()) + "Z");
    add_value(tv, values_array, data_type, values_index, precision);
  }
  catch (...)
  {
    throw Fmi::Exception::Trace(BCP, "Operation failed!");
  }
}

Json::Value add_prologue_one_point(std::optional<int> level,
                                   const std::string &level_type,
                                   double longitude,
                                   double latitude,
                                   int longitude_precision,
                                   int latitude_precision)
{
  try
  {
    //      std::cout << "add_prologue_one_point" << std::endl;
    Json::Value coverage;

    coverage["type"] = Json::Value("Coverage");
    auto domain = Json::Value(Json::ValueType::objectValue);
    domain["type"] = Json::Value("Domain");
    domain["axes"] = Json::Value(Json::ValueType::objectValue);

    domain["axes"]["x"] = Json::Value(Json::ValueType::objectValue);
    domain["axes"]["x"]["values"] = Json::Value(Json::ValueType::arrayValue);
    auto &x_array = domain["axes"]["x"]["values"];
    domain["axes"]["y"] = Json::Value(Json::ValueType::objectValue);
    domain["axes"]["y"]["values"] = Json::Value(Json::ValueType::arrayValue);
    auto &y_array = domain["axes"]["y"]["values"];
    x_array[0] = Json::Value(longitude, longitude_precision);
    y_array[0] = Json::Value(latitude, latitude_precision);

    if (level)
    {
      domain["axes"]["z"] = Json::Value(Json::ValueType::objectValue);
      domain["axes"]["z"]["values"] = Json::Value(Json::ValueType::arrayValue);
      auto &z_array = domain["axes"]["z"]["values"];
      z_array[0] = Json::Value(*level);
    }

    domain["axes"]["t"] = Json::Value(Json::ValueType::objectValue);
    domain["axes"]["t"]["values"] = Json::Value(Json::ValueType::arrayValue);

    // Referencing x,y coordinates
    auto referencing = Json::Value(Json::ValueType::arrayValue);
    auto referencing_xy = Json::Value(Json::ValueType::objectValue);
    referencing_xy["coordinates"] = Json::Value(Json::ValueType::arrayValue);
    referencing_xy["coordinates"][0] = Json::Value("y");
    referencing_xy["coordinates"][1] = Json::Value("x");
    referencing_xy["system"] = Json::Value(Json::ValueType::objectValue);
    referencing_xy["system"]["type"] = Json::Value("GeographicCRS");
    referencing_xy["system"]["id"] = Json::Value("https://www.opengis.net/def/crs/OGC/1.3/CRS84");

    // Referencing z coordinate
    auto referencing_z = Json::Value(Json::ValueType::objectValue);
    if (level)
    {
      referencing_z["coordinates"] = Json::Value(Json::ValueType::arrayValue);
      referencing_z["coordinates"][0] = Json::Value("z");
      referencing_z["system"] = Json::Value(Json::ValueType::objectValue);
      referencing_z["system"]["type"] = Json::Value("VerticalCRS");
      referencing_z["system"]["id"] = Json::Value("TODO: " + level_type);
    }

    auto referencing_time = Json::Value(Json::ValueType::objectValue);
    referencing_time["coordinates"] = Json::Value(Json::ValueType::arrayValue);
    referencing_time["coordinates"][0] = Json::Value("t");
    referencing_time["system"] = Json::Value(Json::ValueType::objectValue);
    referencing_time["system"]["type"] = Json::Value("TemporalRS");
    referencing_time["system"]["calendar"] = Json::Value("Gregorian");
    referencing[0] = referencing_xy;
    if (level)
    {
      referencing[1] = referencing_z;
      referencing[2] = referencing_time;
    }
    else
    {
      referencing[1] = referencing_time;
    }

    domain["referencing"] = referencing;
    domain["domainType"] = Json::Value("PointSeries");
    coverage["domain"] = domain;

    return coverage;
  }
  catch (...)
  {
    throw Fmi::Exception::Trace(BCP, "Operation failed!");
  }
}

[[maybe_unused]]
Json::Value add_prologue_multi_point(std::optional<int> level,
                                     const EDRMetaData &emd,
                                     const std::vector<TS::LonLat> &coordinates)
{
  try
  {
    Json::Value coverage;

    const auto &level_type = emd.vertical_extent.level_type;
    const auto &longitude_precision = emd.getPrecision("longitude");
    const auto &latitude_precision = emd.getPrecision("latitude");

    coverage["type"] = Json::Value("Coverage");
    auto domain = Json::Value(Json::ValueType::objectValue);
    domain["type"] = Json::Value("Domain");
    domain["axes"] = Json::Value(Json::ValueType::objectValue);

    if (coordinates.size() > 1)
    {
      auto composite = Json::Value(Json::ValueType::objectValue);
      composite["dataType"] = Json::Value("tuple");
      composite["coordinates"] = Json::Value(Json::ValueType::arrayValue);
      composite["coordinates"][0] = Json::Value("x");
      composite["coordinates"][1] = Json::Value("y");
      auto values = Json::Value(Json::ValueType::arrayValue);
      for (unsigned int i = 0; i < coordinates.size(); i++)
      {
        values[i] = Json::Value(Json::ValueType::arrayValue);
        values[i][0] = Json::Value(coordinates.at(i).lon, longitude_precision);
        values[i][1] = Json::Value(coordinates.at(i).lat, latitude_precision);
      }
      composite["values"] = values;
      domain["axes"]["composite"] = composite;
    }
    else
    {
      domain["axes"]["x"] = Json::Value(Json::ValueType::objectValue);
      domain["axes"]["x"]["values"] = Json::Value(Json::ValueType::arrayValue);
      auto &x_array = domain["axes"]["x"]["values"];
      domain["axes"]["y"] = Json::Value(Json::ValueType::objectValue);
      domain["axes"]["y"]["values"] = Json::Value(Json::ValueType::arrayValue);
      auto &y_array = domain["axes"]["y"]["values"];
      for (unsigned int i = 0; i < coordinates.size(); i++)
      {
        x_array[i] = Json::Value(coordinates.at(i).lon, longitude_precision);
        y_array[i] = Json::Value(coordinates.at(i).lat, latitude_precision);
      }
    }

    if (level)
    {
      domain["axes"]["z"] = Json::Value(Json::ValueType::objectValue);
      domain["axes"]["z"]["values"] = Json::Value(Json::ValueType::arrayValue);
      auto &z_array = domain["axes"]["z"]["values"];
      z_array[0] = Json::Value(*level);
    }

    domain["axes"]["t"] = Json::Value(Json::ValueType::objectValue);
    domain["axes"]["t"]["values"] = Json::Value(Json::ValueType::arrayValue);
    auto referencing = Json::Value(Json::ValueType::arrayValue);
    auto referencing_xy = Json::Value(Json::ValueType::objectValue);
    referencing_xy["coordinates"] = Json::Value(Json::ValueType::arrayValue);
    referencing_xy["coordinates"][0] = Json::Value("y");
    referencing_xy["coordinates"][1] = Json::Value("x");
    referencing_xy["system"] = Json::Value(Json::ValueType::objectValue);
    referencing_xy["system"]["type"] = Json::Value("GeographicCRS");
    referencing_xy["system"]["id"] = Json::Value("https://www.opengis.net/def/crs/OGC/1.3/CRS84");

    auto referencing_z = Json::Value(Json::ValueType::objectValue);
    if (level)
    {
      referencing_z["coordinates"] = Json::Value(Json::ValueType::arrayValue);
      referencing_z["coordinates"][0] = Json::Value("z");
      referencing_z["system"] = Json::Value(Json::ValueType::objectValue);
      referencing_z["system"]["type"] = Json::Value("VerticalCRS");
      referencing_z["system"]["id"] = Json::Value("TODO: " + level_type);
    }

    auto referencing_time = Json::Value(Json::ValueType::objectValue);
    referencing_time["coordinates"] = Json::Value(Json::ValueType::arrayValue);
    referencing_time["coordinates"][0] = Json::Value("t");
    referencing_time["system"] = Json::Value(Json::ValueType::objectValue);
    referencing_time["system"]["type"] = Json::Value("TemporalRS");
    referencing_time["system"]["calendar"] = Json::Value("Gregorian");
    referencing[0] = referencing_xy;
    if (level)
    {
      referencing[1] = referencing_z;
      referencing[2] = referencing_time;
    }
    else
    {
      referencing[1] = referencing_time;
    }

    domain["referencing"] = referencing;
    domain["domainType"] = Json::Value("MultiPointSeries");
    coverage["domain"] = domain;

    return coverage;
  }
  catch (...)
  {
    throw Fmi::Exception::Trace(BCP, "Operation failed!");
  }
}

Json::Value add_prologue_coverage_collection(const EDRMetaData &emd,
                                             const std::vector<Spine::Parameter> &query_parameters,
                                             bool levels_exists,
                                             const std::string &domain_type,
                                             const CustomDimReferences &custom_dim_refs,
                                             const std::string &language)
{
  try
  {
    Json::Value coverage_collection;

    coverage_collection["type"] = Json::Value("CoverageCollection");
    coverage_collection["parameters"] =
        get_edr_series_parameters(query_parameters, emd, custom_dim_refs, language);

    auto referencing = Json::Value(Json::ValueType::arrayValue);
    auto referencing_xy = Json::Value(Json::ValueType::objectValue);
    referencing_xy["coordinates"] = Json::Value(Json::ValueType::arrayValue);
    referencing_xy["coordinates"][0] = Json::Value("y");
    referencing_xy["coordinates"][1] = Json::Value("x");
    referencing_xy["system"] = Json::Value(Json::ValueType::objectValue);
    referencing_xy["system"]["type"] = Json::Value("GeographicCRS");
    referencing_xy["system"]["id"] = Json::Value("https://www.opengis.net/def/crs/OGC/1.3/CRS84");

    auto referencing_z = Json::Value(Json::ValueType::objectValue);
    if (levels_exists)
    {
      referencing_z["coordinates"] = Json::Value(Json::ValueType::arrayValue);
      referencing_z["coordinates"][0] = Json::Value("z");
      referencing_z["system"] = Json::Value(Json::ValueType::objectValue);
      referencing_z["system"]["type"] = Json::Value("VerticalCRS");
      referencing_z["system"]["id"] = Json::Value("TODO: " + emd.vertical_extent.level_type);
    }

    auto referencing_time = Json::Value(Json::ValueType::objectValue);
    referencing_time["coordinates"] = Json::Value(Json::ValueType::arrayValue);
    referencing_time["coordinates"][0] = Json::Value("t");
    referencing_time["system"] = Json::Value(Json::ValueType::objectValue);
    referencing_time["system"]["type"] = Json::Value("TemporalRS");
    referencing_time["system"]["calendar"] = Json::Value("Gregorian");
    referencing[0] = referencing_xy;
    if (levels_exists)
    {
      referencing[1] = referencing_z;
      referencing[2] = referencing_time;
    }
    else
    {
      referencing[1] = referencing_time;
    }

    coverage_collection["referencing"] = referencing;
    coverage_collection["domainType"] = Json::Value(domain_type);

    return coverage_collection;
  }
  catch (...)
  {
    throw Fmi::Exception::Trace(BCP, "Operation failed!");
  }
}

Json::Value parse_parameter_names(const EDRMetaData &collection_emd,
                                  const CustomDimReferences &custom_dim_refs,
                                  Json::Value &custom_dims)
{
  try
  {
    auto parameter_names = Json::Value(Json::ValueType::objectValue);

    // Custom dimensions
    std::optional<std::set<std::string>> stdnames = std::set<std::string>{};
    std::optional<std::set<std::string>> methods = std::set<std::string>{};
    std::optional<std::set<std::string>> durations = std::set<std::string>{};
    std::optional<std::set<float>> levels = std::set<float>{};

    for (const auto &name : collection_emd.parameter_names)
    {
      if (! name.empty())
        parameter_names[name] = parameter_metadata(
            collection_emd, name, stdnames, methods, durations, levels, custom_dim_refs);
    }

    if (stdnames->size() > 0)
    {
      auto stdname_interval = Json::Value(Json::ValueType::arrayValue);
      stdname_interval[0] = Json::Value(*(stdnames->begin()));
      stdname_interval[1] = Json::Value(*(stdnames->rbegin()));
      auto stdname_values = Json::Value(Json::ValueType::arrayValue);
      for (auto const &name : *stdnames)
      {
        stdname_values[stdname_values.size()] = Json::Value(name);
      }
      auto stdname_dim = Json::Value(Json::ValueType::objectValue);
      auto dim = "standard_name";
      stdname_dim["id"] = Json::Value(dim);
      stdname_dim["interval"] = stdname_interval;
      stdname_dim["values"] = stdname_values;
      auto it = custom_dim_refs.find(dim);
      if (it != custom_dim_refs.end())
        stdname_dim["reference"] = Json::Value(it->second);
      custom_dims[custom_dims.size()] = stdname_dim;

      auto level_interval = Json::Value(Json::ValueType::arrayValue);
      level_interval[0] = Json::Value(*(levels->begin()));
      level_interval[1] = Json::Value(*(levels->rbegin()));
      auto level_values = Json::Value(Json::ValueType::arrayValue);
      for (auto const &level : *levels)
      {
        level_values[level_values.size()] = Json::Value(level);
      }
      auto level_dim = Json::Value(Json::ValueType::objectValue);
      dim = "level";
      level_dim["id"] = Json::Value(dim);
      level_dim["interval"] = level_interval;
      level_dim["values"] = level_values;
      it = custom_dim_refs.find(dim);
      if (it != custom_dim_refs.end())
        level_dim["reference"] = Json::Value(it->second);
      custom_dims[custom_dims.size()] = level_dim;

      auto method_interval = Json::Value(Json::ValueType::arrayValue);
      method_interval[0] = Json::Value(*(methods->begin()));
      method_interval[1] = Json::Value(*(methods->rbegin()));
      auto method_values = Json::Value(Json::ValueType::arrayValue);
      for (auto const &method : *methods)
      {
        method_values[method_values.size()] = Json::Value(method);
      }
      auto method_dim = Json::Value(Json::ValueType::objectValue);
      dim = "method";
      method_dim["id"] = Json::Value(dim);
      method_dim["interval"] = method_interval;
      method_dim["values"] = method_values;
      it = custom_dim_refs.find(dim);
      if (it != custom_dim_refs.end())
        method_dim["reference"] = Json::Value(it->second);
      custom_dims[custom_dims.size()] = method_dim;

      auto duration_interval = Json::Value(Json::ValueType::arrayValue);
      duration_interval[0] = Json::Value(*(durations->begin()));
      duration_interval[1] = Json::Value(*(durations->rbegin()));
      auto duration_values = Json::Value(Json::ValueType::arrayValue);
      for (auto const &duration : *durations)
      {
        duration_values[duration_values.size()] = Json::Value(duration);
      }
      auto duration_dim = Json::Value(Json::ValueType::objectValue);
      dim = "duration";
      duration_dim["id"] = Json::Value(dim);
      duration_dim["interval"] = duration_interval;
      duration_dim["values"] = duration_values;
      it = custom_dim_refs.find(dim);
      if (it != custom_dim_refs.end())
        duration_dim["reference"] = Json::Value(it->second);
      custom_dims[custom_dims.size()] = duration_dim;
    }

    return parameter_names;
  }
  catch (...)
  {
    throw Fmi::Exception::Trace(BCP, "Operation failed!");
  }
}

void parse_instance_link(bool instances_exist,
                         const EDRQuery &edr_query,
                         const std::string &producer,
                         Json::Value &links)
{
  try
  {
    if (instances_exist)
    {
      auto instance_link = Json::Value(Json::ValueType::objectValue);
      instance_link["href"] =
          Json::Value((edr_query.host + "/collections/" + producer + "/instances"));
      instance_link["hreflang"] = Json::Value("en");
      instance_link["rel"] = Json::Value("collection");
      instance_link["type"] = Json::Value("application/json");
      links[1] = instance_link;
    }
  }
  catch (...)
  {
    throw Fmi::Exception::Trace(BCP, "Operation failed!");
  }
}

Json::Value parse_license_link(const ProducerLicenses &licenses, const std::string &producer)
{
  try
  {
    auto license = licenses.find(producer);
    if (license == licenses.end())
      license = licenses.find(DEFAULT_LICENSE);

    if ((license == licenses.end()) || license->second.empty())
      return {};

    auto license_link = Json::Value(Json::ValueType::objectValue);

    for (auto const &field : license->second)
      license_link[field.first] = field.second;

    return license_link;
  }
  catch (...)
  {
    throw Fmi::Exception::Trace(BCP, "Operation failed!");
  }
}

EDRProducerMetaData get_producer_metadata(const EDRProducerMetaData &epmd,
                                          const EDRQuery &edr_query)
{
  try
  {
    EDRProducerMetaData ret;

    if (!edr_query.collection_id.empty())
    {
      if (epmd.find(edr_query.collection_id) != epmd.end())
        ret[edr_query.collection_id] = epmd.at(edr_query.collection_id);
    }
    else
    {
      ret = epmd;
    }

    return ret;
  }
  catch (...)
  {
    throw Fmi::Exception::Trace(BCP, "Operation failed!");
  }
}

bool process_instance(const EDRQuery &edr_query,
                      const EDRMetaData &emd,
                      const std::string &instance_id)
{
  try
  {
    if (edr_query.query_id == EDRQueryId::SpecifiedCollectionSpecifiedInstance &&
        instance_id != edr_query.instance_id)
      return false;

    if (emd.parameter_names.empty())
      return false;

    return true;
  }
  catch (...)
  {
    throw Fmi::Exception::Trace(BCP, "Operation failed!");
  }
}

void parse_title(const EDRMetaData &emd, Json::Value &instance)
{
  try
  {
    if (!emd.temporal_extent.time_periods.empty())
    {
      std::string title =
          ("Origintime: " +
           Fmi::date_time::to_iso_extended_string(emd.temporal_extent.origin_time) + "Z");
      title += (" Starttime: " +
                Fmi::date_time::to_iso_extended_string(
                    emd.temporal_extent.time_periods.front().start_time) +
                "Z");
      title += (" Endtime: " +
                Fmi::date_time::to_iso_extended_string(
                    emd.temporal_extent.time_periods.front().end_time) +
                "Z");
      if (emd.temporal_extent.time_periods.front().timestep)
        title +=
            (" Timestep: " + Fmi::to_string(emd.temporal_extent.time_periods.front().timestep));
      instance["title"] = Json::Value(title);
    }
  }
  catch (...)
  {
    throw Fmi::Exception::Trace(BCP, "Operation failed!");
  }
}

void parse_vertical_extent(const EDRMetaData &emd, Json::Value &extent)
{
  try
  {
    if (!emd.vertical_extent.levels.empty())
    {
      auto vertical = Json::Value(Json::ValueType::objectValue);
      auto vertical_interval = Json::Value(Json::ValueType::arrayValue);
      auto vertical_interval_values = Json::Value(Json::ValueType::arrayValue);
      for (unsigned int i = 0; i < emd.vertical_extent.levels.size(); i++)
        vertical_interval_values[i] = emd.vertical_extent.levels.at(i);
      vertical_interval[0] = emd.vertical_extent.levels.front();
      vertical_interval[1] = emd.vertical_extent.levels.back();
      vertical["interval"] = Json::Value(Json::ValueType::arrayValue);
      vertical["interval"][0] = vertical_interval;
      vertical["values"] = vertical_interval_values;
      vertical["vrs"] = emd.vertical_extent.vrs;
      extent["vertical"] = vertical;
    }
  }
  catch (...)
  {
    throw Fmi::Exception::Trace(BCP, "Operation failed!");
  }
}

void add_timestep_dimension(const edr_temporal_extent &temporal_extent,
                            const CustomDimReferences &custom_dim_refs,
                            Json::Value &custom_dims)
{
  try
  {
    if (!temporal_extent.time_steps.empty())
    {
      auto timesteps = Json::Value(Json::ValueType::arrayValue);
      for (auto ts : temporal_extent.time_steps)
      {
        timesteps[timesteps.size()] = Json::Value(ts);
      }

      auto interval = Json::Value(Json::ValueType::arrayValue);
      auto itl = temporal_extent.time_steps.end();
      itl--;
      interval[0] = Json::Value(*(temporal_extent.time_steps.begin()));
      interval[1] = Json::Value(*itl);

      auto timestepdim = Json::Value(Json::ValueType::objectValue);
      auto dim = "timestep";
      timestepdim["id"] = Json::Value(dim);
      timestepdim["interval"] = interval;
      timestepdim["values"] = timesteps;
      auto it = custom_dim_refs.find(dim);
      if (it != custom_dim_refs.end())
        timestepdim["reference"] = Json::Value(it->second);

      custom_dims[custom_dims.size()] = timestepdim;
    }
  }
  catch (...)
  {
    throw Fmi::Exception::Trace(BCP, "Operation failed!");
  }
}

/*
clang-format off

Table C.1 — EDR Collection Object structure:
https://docs.ogc.org/is/19-086r4/19-086r4.html#toc63
-------------------------------------------------------
| Field Name      | Type                   | Required  | Description
-------------------------------------------------------
| links           | link Array             | Yes       | Array of Link objects
| id              | String                 | Yes       | Unique identifier string for the collection, used as the value for the collection_id path parameter in all queries on the collection
| title           | String                 | No        | A short text label for the collection
| description     | String                 | No        | A text description of the information provided by the collection
| keywords        | String Array           | No        | Array of words and phrases that define the information that the collection provides
| extent          | extent object          | Yes       | Object describing the spatio-temporal extent of the information provided by the collection
| data_queries    | data_queries object    | No        | Object providing query specific information
| crs             | String Array           | No        | Array of coordinate reference system names, which define the output coordinate systems supported by the collection
| output_formats  | String Array           | No        | Array of data format names, which define the data formats to which information in the collection can be output
| parameter_names | parameter_names object | Yes       | Describes the data values available in the collection

clang-format on
*/

// Metadata of specified producers' specified  collections' instance/instances
Json::Value parse_edr_metadata_instances(const EDRProducerMetaData &epmd,
                                         const EDRQuery &edr_query,
                                         const ProducerLicenses &licenses,
                                         const CustomDimReferences &custom_dim_refs)
{
  try
  {
    //	  std::cout << "parse_edr_metadata_instances: " << edr_query <<
    // std::endl;
    Json::Value nulljson;

    if (epmd.empty())
      return nulljson;

    EDRProducerMetaData requested_epmd = get_producer_metadata(epmd, edr_query);

    auto result = Json::Value(Json::ValueType::objectValue);

    const auto &emds = requested_epmd.begin()->second;
    bool instances_exist = !emds.empty();

    if (!instances_exist)
      return reportError(400,
                         ("Collection '" + edr_query.collection_id + "' does not have instances!"));

    const auto &producer = requested_epmd.begin()->first;

    auto links = Json::Value(Json::ValueType::arrayValue);
    auto link_item = Json::Value(Json::ValueType::objectValue);
    auto href = edr_query.host + "/collections/" + producer + "/instances";
    link_item["href"] = Json::Value(href);
    link_item["rel"] = Json::Value("self");
    link_item["type"] = Json::Value("application/json");
    links[0] = link_item;
    auto license = parse_license_link(licenses, edr_query.collection_id);
    if (!license.isNullOrEmpty())
      links[1] = license;

    result["links"] = links;

    auto instances = Json::Value(Json::ValueType::arrayValue);

    unsigned int instance_index = 0;
    for (const auto &emd : emds)
    {
      auto instance_id = Fmi::to_iso_string(emd.temporal_extent.origin_time);

      if (!process_instance(edr_query, emd, instance_id))
        continue;

      auto instance = Json::Value(Json::ValueType::objectValue);
      instance["id"] = Json::Value(instance_id);

      parse_title(emd, instance);

      // Links
      /*
      auto instance_links = Json::Value(Json::ValueType::arrayValue);
      auto instance_link_item = Json::Value(Json::ValueType::objectValue);
      instance_link_item["href"] = Json::Value((edr_query.host + "/collections/"
      + producer + "/" + instance_id)); instance_link_item["hreflang"] =
      Json::Value("en"); instance_link_item["rel"] = Json::Value("data");
      instance_links[0] = instance_link_item;
      instance["links"] = instance_links;
      */

      // Extent (mandatory)
      auto extent = Json::Value(Json::ValueType::objectValue);
      // Spatial (mandatory)
      auto spatial = Json::Value(Json::ValueType::objectValue);
      // BBOX (mandatory)
      auto bbox = Json::Value(Json::ValueType::arrayValue);
      bbox[0] = Json::Value(emd.spatial_extent.bbox_xmin);
      bbox[1] = Json::Value(emd.spatial_extent.bbox_ymin);
      bbox[2] = Json::Value(emd.spatial_extent.bbox_xmax);
      bbox[3] = Json::Value(emd.spatial_extent.bbox_ymax);
      spatial["bbox"] = Json::Value(Json::ValueType::arrayValue);
      spatial["bbox"][0] = bbox;
      // CRS (mandatory)
      //      spatial["crs"] = Json::Value("EPSG:4326");
      spatial["crs"] = Json::Value("OGC:CRS84");
      extent["spatial"] = spatial;
      // Temporal (optional)
      extent["temporal"] = parse_temporal_extent(emd.temporal_extent);
      // Vertical (optional)
      parse_vertical_extent(emd, extent);

      instance["extent"] = extent;
      // Optional: data_queries
      instance["data_queries"] = get_data_queries(edr_query.host,
                                                  producer,
                                                  emd.data_queries,
                                                  emd.output_formats,
                                                  emd.default_output_format,
                                                  !emd.vertical_extent.levels.empty(),
                                                  false,
                                                  instance_id);
      // Optional: crs
      auto crs = Json::Value(Json::ValueType::arrayValue);
      //	  crs[0] = Json::Value("EPSG:4326");
      crs[0] = Json::Value("OGC:CRS84");
      instance["crs"] = crs;

      // Custom dimensions
      auto custom_dims = Json::Value(Json::ValueType::arrayValue);

      // Parameter names (mandatory)
      auto parameter_names = parse_parameter_names(emd, custom_dim_refs, custom_dims);
      instance["parameter_names"] = parameter_names;

      // Timestep dimension (allowed timesteps for &timestep=n)
      add_timestep_dimension(emd.temporal_extent, custom_dim_refs, custom_dims);

      if (custom_dims.size() > 0)
        instance["extent"]["custom"] = custom_dims;

      instances[instance_index] = instance;
      instance_index++;

      if (edr_query.query_id == EDRQueryId::SpecifiedCollectionSpecifiedInstance)
      {
        links[0]["href"] = Json::Value(href + "/" + instance_id);
        instances[0]["links"] = links;

        return instances[0];
      }
    }

    result["instances"] = instances;

    return result;
  }
  catch (...)
  {
    throw Fmi::Exception::Trace(BCP, "Operation failed!");
  }
}

EDRMetaData get_most_recent_instance(const std::vector<EDRMetaData> &emds)
{
  try
  {
    EDRMetaData ret;
    for (unsigned int i = 0; i < emds.size(); i++)
    {
      if (i == 0 || ret.temporal_extent.origin_time < emds.at(i).temporal_extent.origin_time)
        ret = emds.at(i);
    }

    return ret;
  }
  catch (...)
  {
    throw Fmi::Exception::Trace(BCP, "Operation failed!");
  }
}

void get_collection_info_from_configuration_file(const EDRMetaData &collection_emd,
                                                 std::string &title,
                                                 std::string &description,
                                                 std::set<std::string> &keyword_set)
{
  try
  {
    // Then get collection info from configuration file, if we didn't get it from engine
    if (collection_emd.collection_info)
    {
      // Title, description, keywords (optional)
      if (title.empty())
        title = collection_emd.collection_info->title;
      if (description.empty())
        description = collection_emd.collection_info->description;
      if (keyword_set.empty())
        keyword_set = collection_emd.collection_info->keywords;
    }
  }
  catch (...)
  {
    throw Fmi::Exception::Trace(BCP, "Operation failed!");
  }
}

void add_parameter_names_into_keywords(const EDRMetaData &collection_emd, Json::Value &keywords)
{
  try
  {
    // Add parameter names into keywords
    for (const auto &name : collection_emd.parameter_names)
      keywords[keywords.size()] = name;
  }
  catch (...)
  {
    throw Fmi::Exception::Trace(BCP, "Operation failed!");
  }
}

Json::Value parse_edr_metadata_collections(const EDRProducerMetaData &epmd,
                                           const EDRQuery &edr_query,
                                           const ProducerLicenses &licenses,
                                           const CustomDimReferences &custom_dim_refs)
{
  try
  {
    //	  std::cout << "parse_edr_metadata_collections: " << edr_query <<
    // std::endl;

    EDRProducerMetaData requested_epmd = get_producer_metadata(epmd, edr_query);

    auto collections = Json::Value(Json::ValueType::arrayValue);
    Json::Value nulljson;
    unsigned int collection_index = 0;

    // Iterate metadata and add item into collection
    for (const auto &item : requested_epmd)
    {
      const auto &producer = item.first;
      const auto &emds = item.second;
      // Get the most recent instance
      EDRMetaData collection_emd = get_most_recent_instance(emds);

      if (collection_emd.parameter_names.empty())
        continue;

      bool instances_exist = (emds.size() > 1);

      auto value = Json::Value(Json::ValueType::objectValue);
      // Producer is Id
      value["id"] = Json::Value(producer);
      // Collection info, first from engine
      std::string title = collection_emd.collection_info_engine.title;
      std::string description = collection_emd.collection_info_engine.description;
      std::set<std::string> keyword_set = collection_emd.collection_info_engine.keywords;

      // Then get collection info from configuration file, if we didn't get it from engine
      get_collection_info_from_configuration_file(collection_emd, title, description, keyword_set);

      value["title"] = Json::Value(title);
      value["description"] = Json::Value(description);
      auto keywords = Json::Value(Json::ValueType::arrayValue);
      for (const auto &kword : keyword_set)
        keywords[keywords.size()] = kword;

      // Add parameter names into keywords
      add_parameter_names_into_keywords(collection_emd, keywords);

      if (keywords.size() > 0)
        value["keywords"] = keywords;

      // Array of links (mandatory)
      auto collection_link = Json::Value(Json::ValueType::objectValue);
      collection_link["href"] = Json::Value((edr_query.host + "/collections/" + producer));
      collection_link["hreflang"] = Json::Value("en");
      if (edr_query.query_id == EDRQueryId::SpecifiedCollection)
        collection_link["rel"] = Json::Value("self");
      else
        collection_link["rel"] = Json::Value("data");
      collection_link["type"] = Json::Value("application/json");
      //    collection_link["title"] = Json::Value("Collection metadata in JSON");

      auto links = Json::Value(Json::ValueType::arrayValue);
      links[0] = collection_link;
      parse_instance_link(instances_exist, edr_query, producer, links);

      if (!edr_query.collection_id.empty())
      {
        auto license = parse_license_link(licenses, producer);
        if (!license.isNullOrEmpty())
          links.append(license);
      }

      value["links"] = links;
      // Extent (mandatory)
      auto extent = Json::Value(Json::ValueType::objectValue);
      // Spatial (mandatory)
      auto spatial = Json::Value(Json::ValueType::objectValue);
      // BBOX (mandatory)
      auto bbox = Json::Value(Json::ValueType::arrayValue);
      bbox[0] = Json::Value(collection_emd.spatial_extent.bbox_xmin);
      bbox[1] = Json::Value(collection_emd.spatial_extent.bbox_ymin);
      bbox[2] = Json::Value(collection_emd.spatial_extent.bbox_xmax);
      bbox[3] = Json::Value(collection_emd.spatial_extent.bbox_ymax);
      spatial["bbox"] = Json::Value(Json::ValueType::arrayValue);
      spatial["bbox"][0] = bbox;
      // CRS (mandatory)
      //      spatial["crs"] = Json::Value("EPSG:4326");
      spatial["crs"] = Json::Value("OGC:CRS84");
      extent["spatial"] = spatial;
      // Temporal (optional)
      extent["temporal"] = parse_temporal_extent(collection_emd.temporal_extent);
      // Vertical (optional)
      parse_vertical_extent(collection_emd, extent);

      value["extent"] = extent;
      // Optional: data_queries
      value["data_queries"] = get_data_queries(edr_query.host,
                                               producer,
                                               collection_emd.data_queries,
                                               collection_emd.output_formats,
                                               collection_emd.default_output_format,
                                               !collection_emd.vertical_extent.levels.empty(),
                                               instances_exist);

      // Optional: crs
      auto crs = Json::Value(Json::ValueType::arrayValue);
      //	  crs[0] = Json::Value("EPSG:4326");
      crs[0] = Json::Value("OGC:CRS84");
      value["crs"] = crs;

      // Custom dimensions
      auto custom_dims = Json::Value(Json::ValueType::arrayValue);

      // Parameter names (mandatory)
      auto parameter_names = parse_parameter_names(collection_emd, custom_dim_refs, custom_dims);

      value["parameter_names"] = parameter_names;
      auto output_formats = Json::Value(Json::ValueType::arrayValue);
      unsigned int i = 0;
      for (const auto &f : collection_emd.output_formats)
        output_formats[i++] = Json::Value(f);
      value["output_formats"] = output_formats;

      // Timestep dimension (allowed timesteps for &timestep=n)
      add_timestep_dimension(collection_emd.temporal_extent, custom_dim_refs, custom_dims);

      if (custom_dims.size() > 0)
        value["extent"]["custom"] = custom_dims;

      collections[collection_index++] = value;
    }

    // Collections of one producer
    if (edr_query.query_id == EDRQueryId::SpecifiedCollection && collection_index == 1)
      return collections[0];

    return collections;
  }
  catch (...)
  {
    throw Fmi::Exception::Trace(BCP, "Operation failed!");
  }
}

Json::Value parse_edr_metadata(const EDRProducerMetaData &epmd,
                               const EDRQuery &edr_query,
                               const ProducerLicenses &licenses,
                               const CustomDimReferences &custom_dim_refs)
{
  try
  {
    //  std::cout << "parse_edr_metadata: " << edr_query << std::endl;

    Json::Value nulljson;

    if (edr_query.query_id == EDRQueryId::AllCollections ||
        edr_query.query_id == EDRQueryId::SpecifiedCollection)
      return parse_edr_metadata_collections(epmd, edr_query, licenses, custom_dim_refs);

    if (edr_query.query_id == EDRQueryId::SpecifiedCollectionAllInstances ||
        edr_query.query_id == EDRQueryId::SpecifiedCollectionSpecifiedInstance)
      return parse_edr_metadata_instances(epmd, edr_query, licenses, custom_dim_refs);

    return nulljson;
  }
  catch (...)
  {
    throw Fmi::Exception::Trace(BCP, "Operation failed!");
  }
}

void add_values(const TS::TimeSeriesPtr &ts,
                int parameter_precision,
                Json::Value &values,
                Json::Value &json_param_object,
                std::set<std::string> &timesteps)
{
  try
  {
    unsigned int values_index = 0;
    for (const auto &timed_value : *ts)
    {
      add_value(timed_value,
                values,
                json_param_object["dataType"],
                timesteps,
                values_index,
                parameter_precision);
      values_index++;
    }
  }
  catch (...)
  {
    throw Fmi::Exception::Trace(BCP, "Operation failed!");
  }
}

void process_parameters_one_point(const std::vector<TS::TimeSeriesData> &outdata,
                                  const std::vector<Spine::Parameter> &query_parameters,
                                  const EDRMetaData &emd,
                                  const std::vector<TS::LonLat> &coordinates,
                                  const unsigned int &i,
                                  const std::optional<int> &level,
                                  std::set<std::string> &timesteps,
                                  Json::Value &ranges,
                                  Json::Value &coverage,
                                  const CustomDimReferences &custom_dim_refs,
                                  const std::string &language)
{
  try
  {
    const auto &longitude_precision = emd.getPrecision("longitude");
    const auto &latitude_precision = emd.getPrecision("latitude");
    // iterate columns (parameters)
    for (unsigned int j = 0; j < outdata.size(); j++)
    {
      auto parameter_name = query_parameters[j].name();

      // BRAINSTORM-3116 'level' kludge, FIXME !
      //
      if (parameter_name == EDR_OBSERVATION_LEVEL)
        continue;

      const auto &parameter_precision = emd.getPrecision(parameter_name);
      boost::algorithm::to_lower(parameter_name);
      if (lon_lat_level_param(parameter_name))
        continue;

      auto json_param_object = Json::Value(Json::ValueType::objectValue);
      auto tsdata = outdata.at(j);
      auto values = Json::Value(Json::ValueType::arrayValue);

      TS::TimeSeriesPtr ts = std::get<TS::TimeSeriesPtr>(tsdata);
      if (i == 0)
      {
        coverage = add_prologue_one_point(level,
                                          emd.vertical_extent.level_type,
                                          coordinates.front().lon,
                                          coordinates.front().lat,
                                          longitude_precision,
                                          latitude_precision);
        coverage["parameters"] =
            get_edr_series_parameters(query_parameters, emd, custom_dim_refs, language);
        json_param_object["type"] = Json::Value("NdArray");
        auto axis_names = Json::Value(Json::ValueType::arrayValue);
        axis_names[0] = Json::Value("t");
        axis_names[1] = Json::Value("x");
        axis_names[2] = Json::Value("y");
        if (level)
          axis_names[3] = Json::Value("z");
        json_param_object["axisNames"] = axis_names;
        auto shape = Json::Value(Json::ValueType::arrayValue);
        shape[0] = Json::Value(ts->size());
        shape[1] = Json::Value(coordinates.size());
        shape[2] = Json::Value(coordinates.size());
        if (level)
          shape[3] = Json::Value(1);
        json_param_object["shape"] = shape;
      }

      add_values(ts, parameter_precision, values, json_param_object, timesteps);

      json_param_object["values"] = values;
      parameter_name = parse_parameter_name(query_parameters[j].originalName());
      ranges[parameter_name] = json_param_object;
    }
  }
  catch (...)
  {
    throw Fmi::Exception::Trace(BCP, "Operation failed!");
  }
}

Json::Value format_output_data_one_point(const TS::OutputData &outputData,
                                         const EDRMetaData &emd,
                                         std::optional<int> level,
                                         const std::vector<Spine::Parameter> &query_parameters,
                                         const CustomDimReferences &custom_dim_refs,
                                         const std::string &language)
{
  try
  {
    // std::cout << "format_output_data_one_point" << std::endl;

    Json::Value coverage;

    auto ranges = Json::Value(Json::ValueType::objectValue);
    auto coordinates = get_coordinates(outputData, query_parameters);
    if (coordinates.empty())
      return coverage;

    std::set<std::string> timesteps;
    for (unsigned int i = 0; i < outputData.size(); i++)
    {
      const auto &outdata = outputData[i].second;
      // iterate columns (parameters)
      process_parameters_one_point(
          outdata, query_parameters, emd, coordinates, i, level, timesteps, ranges, coverage,
          custom_dim_refs, language);
    }

    auto &time_array = coverage["domain"]["axes"]["t"]["values"];
    unsigned int time_index = 0;
    for (const auto &t : timesteps)
      time_array[time_index++] = Json::Value(t);

    coverage["ranges"] = ranges;

    return coverage;
  }
  catch (...)
  {
    throw Fmi::Exception::Trace(BCP, "Operation failed!");
  }
}

#if 0
void process_ts_parameters_at_position(const TS::TimeSeriesPtr &ts_data,
                                       const TS::TimeSeriesPtr &ts_lon,
                                       const TS::TimeSeriesPtr &ts_lat,
                                       const TS::TimeSeriesPtr &ts_level,
                                       const std::string &parameter_name,
                                       const int &parameter_precision,
                                       const int &longitude_precision,
                                       const int &latitude_precision,
                                       const int &level_precision,
                                       Json::Value &coverages,
                                       unsigned int &coverages_index)
{
  try
  {
    unsigned int values_index = 0;
    for (unsigned int k = 0; k < ts_data->size(); k++)
    {
      auto coverage_object = Json::Value(Json::ValueType::objectValue);
      coverage_object["type"] = Json::Value("Coverage");
      auto domain_object = Json::Value(Json::ValueType::objectValue);
      domain_object["type"] = Json::Value("Domain");
      auto x_object = Json::Value(Json::ValueType::objectValue);
      x_object["values"] = Json::Value(Json::ValueType::arrayValue);
      auto y_object = Json::Value(Json::ValueType::objectValue);
      y_object["values"] = Json::Value(Json::ValueType::arrayValue);
      auto z_object = Json::Value(Json::ValueType::objectValue);
      z_object["values"] = Json::Value(Json::ValueType::arrayValue);
      auto timestamp_object = Json::Value(Json::ValueType::objectValue);
      timestamp_object["values"] = Json::Value(Json::ValueType::arrayValue);

      // const auto& data_value = ts_data->at(k);
      const auto &lon_value = ts_lon->at(k);
      const auto &lat_value = ts_lat->at(k);
      auto &x_values = x_object["values"];
      auto &y_values = y_object["values"];
      Json::Value data_type;
      add_value(lon_value, x_values, data_type, values_index, longitude_precision);
      add_value(lat_value, y_values, data_type, values_index, latitude_precision);
      if (ts_level)
      {
        const auto &level_value = ts_level->at(k);
        auto &z_values = z_object["values"];
        add_value(level_value, z_values, data_type, values_index, level_precision);
      }
      auto &timestamp_values = timestamp_object["values"];
      timestamp_values[values_index] =
          Json::Value(Fmi::date_time::to_iso_extended_string(lon_value.time.utc_time()) + "Z");

      auto domain_object_axes = Json::Value(Json::ValueType::objectValue);
      domain_object_axes["x"] = x_object;
      domain_object_axes["y"] = y_object;
      if (ts_level)
        domain_object_axes["z"] = z_object;
      domain_object_axes["t"] = timestamp_object;

      domain_object["axes"] = domain_object_axes;
      coverage_object["domain"] = domain_object;

      auto ranges_object = Json::Value(Json::ValueType::objectValue);
      auto parameter_object = Json::Value(Json::ValueType::objectValue);
      parameter_object["type"] = Json::Value("NdArray");
      auto shape_object = Json::Value(Json::ValueType::arrayValue);
      shape_object[0] = Json::Value(1);
      shape_object[1] = Json::Value(1);
      shape_object[2] = Json::Value(1);
      if (ts_level)
        shape_object[3] = Json::Value(1);
      auto axis_object = Json::Value(Json::ValueType::arrayValue);
      axis_object[0] = Json::Value("t");
      axis_object[1] = Json::Value("x");
      axis_object[2] = Json::Value("y");
      if (ts_level)
        axis_object[3] = Json::Value("z");
      auto values_array = Json::Value(Json::ValueType::arrayValue);
      const auto &data_value = ts_data->at(k);
      add_value(data_value, values_array, data_type, values_index, parameter_precision);
      parameter_object["dataType"] = data_type;

      //				values_array[0] = Json::Value(1.5784); //
      // todo
      parameter_object["shape"] = shape_object;
      parameter_object["axisNames"] = axis_object;
      parameter_object["values"] = values_array;
      ranges_object[parameter_name] = parameter_object;
      coverage_object["ranges"] = ranges_object;

      coverages[coverages_index] = coverage_object;
      coverages_index++;
    }
  }
  catch (...)
  {
    throw Fmi::Exception::Trace(BCP, "Operation failed!");
  }
}

void process_parameters_at_position(const std::vector<TS::TimeSeriesData> &outdata,
                                    const std::vector<Spine::Parameter> &query_parameters,
                                    const EDRMetaData &emd,
                                    const unsigned int &longitude_index,
                                    const unsigned int &latitude_index,
                                    const std::optional<unsigned int> &level_index,
                                    Json::Value &coverages,
                                    unsigned int &coverages_index)
{
  try
  {
    const auto &longitude_precision = emd.getPrecision("longitude");
    const auto &latitude_precision = emd.getPrecision("latitude");
    const auto level_precision = 0;

    for (unsigned int j = 0; j < outdata.size(); j++)
    {
      auto parameter_name = query_parameters[j].name();
      const auto &parameter_precision = emd.getPrecision(parameter_name);
      boost::algorithm::to_lower(parameter_name);
      if (lon_lat_level_param(parameter_name))
        continue;

      auto tsdata = outdata.at(j);
      auto tslon = outdata.at(longitude_index);
      auto tslat = outdata.at(latitude_index);
      TS::TimeSeriesPtr ts_data = std::get<TS::TimeSeriesPtr>(tsdata);
      TS::TimeSeriesPtr ts_lon = std::get<TS::TimeSeriesPtr>(tslon);
      TS::TimeSeriesPtr ts_lat = std::get<TS::TimeSeriesPtr>(tslat);
      TS::TimeSeriesPtr ts_level = nullptr;
      if (level_index)
      {
        auto tslevel = outdata.at(*level_index);
        ts_level = std::get<TS::TimeSeriesPtr>(tslevel);
      }
      process_ts_parameters_at_position(ts_data,
                                        ts_lon,
                                        ts_lat,
                                        ts_level,
                                        parse_parameter_name(query_parameters[j].originalName()),
                                        parameter_precision,
                                        longitude_precision,
                                        latitude_precision,
                                        level_precision,
                                        coverages,
                                        coverages_index);
    }
  }
  catch (...)
  {
    throw Fmi::Exception::Trace(BCP, "Operation failed!");
  }
}

Json::Value format_output_data_position(const TS::OutputData &outputData,
                                        const EDRMetaData &emd,
                                        const std::vector<Spine::Parameter> &query_parameters,
                                        const CustomDimReferences &custom_dim_refs,
                                        const std::string &language)
{
  try
  {
    //	std::cout << "format_output_data_position" << std::endl;

    Json::Value coverage_collection;
    unsigned int longitude_index;
    unsigned int latitude_index;
    std::optional<unsigned int> level_index;
    const auto &last_param = query_parameters.back();
    auto last_param_name = last_param.name();
    boost::algorithm::to_lower(last_param_name);
    if (last_param_name == "level")
    {
      level_index = (query_parameters.size() - 1);
      latitude_index = (query_parameters.size() - 2);
      longitude_index = (query_parameters.size() - 3);
    }
    else
    {
      latitude_index = (query_parameters.size() - 1);
      longitude_index = (query_parameters.size() - 2);
    }

    coverage_collection["type"] = Json::Value("CoverageCollection");
    coverage_collection["domainType"] = Json::Value("Point");
    coverage_collection["parameters"] =
        get_edr_series_parameters(query_parameters, emd, custom_dim_refs, language);

    // Referencing x,y coordinates
    auto referencing = Json::Value(Json::ValueType::arrayValue);
    auto referencing_xy = Json::Value(Json::ValueType::objectValue);
    referencing_xy["coordinates"] = Json::Value(Json::ValueType::arrayValue);
    referencing_xy["coordinates"][0] = Json::Value("y");
    referencing_xy["coordinates"][1] = Json::Value("x");
    referencing_xy["system"] = Json::Value(Json::ValueType::objectValue);
    referencing_xy["system"]["type"] = Json::Value("GeographicCRS");
    referencing_xy["system"]["id"] = Json::Value("https://www.opengis.net/def/crs/OGC/1.3/CRS84");

    // Referencing z coordinate
    auto referencing_z = Json::Value(Json::ValueType::objectValue);
    if (level_index)
    {
      referencing_z["coordinates"] = Json::Value(Json::ValueType::arrayValue);
      referencing_z["coordinates"][0] = Json::Value("z");
      referencing_z["system"] = Json::Value(Json::ValueType::objectValue);
      referencing_z["system"]["type"] = Json::Value("VerticalCRS");
      referencing_z["system"]["id"] = Json::Value("TODO: " + emd.vertical_extent.level_type);
    }

    auto referencing_time = Json::Value(Json::ValueType::objectValue);
    referencing_time["coordinates"] = Json::Value(Json::ValueType::arrayValue);
    referencing_time["coordinates"][0] = Json::Value("t");
    referencing_time["system"] = Json::Value(Json::ValueType::objectValue);
    referencing_time["system"]["type"] = Json::Value("TemporalRS");
    referencing_time["system"]["calendar"] = Json::Value("Gregorian");
    referencing[0] = referencing_xy;
    if (level_index)
    {
      referencing[1] = referencing_z;
      referencing[2] = referencing_time;
    }
    else
    {
      referencing[1] = referencing_time;
    }

    coverage_collection["referencing"] = referencing;

    auto coverages = Json::Value(Json::ValueType::arrayValue);

    unsigned int coverages_index = 0;

    for (const auto &output : outputData)
    {
      const auto &outdata = output.second;
      // iterate columns (parameters)
      process_parameters_at_position(outdata,
                                     query_parameters,
                                     emd,
                                     longitude_index,
                                     latitude_index,
                                     level_index,
                                     coverages,
                                     coverages_index);
    }

    coverage_collection["coverages"] = coverages;

    return coverage_collection;
  }
  catch (...)
  {
    throw Fmi::Exception::Trace(BCP, "Operation failed!");
  }
}
#endif

void set_axis_names(const bool &levels_present, Json::Value &axis_names)
{
  try
  {
    axis_names[0] = Json::Value("x");
    axis_names[1] = Json::Value("y");
    if (levels_present)
    {
      axis_names[2] = Json::Value("z");
      axis_names[3] = Json::Value("t");
    }
    else
    {
      axis_names[2] = Json::Value("t");
    }
  }
  catch (...)
  {
    throw Fmi::Exception::Trace(BCP, "Operation failed!");
  }
}

void set_parameter_value(const time_coord_value &value,
                         const int &parameter_precision,
                         const bool &isAviProducer,
                         Json::Value &values,
                         Json::Value &range_item)
{
  try
  {
    if (value.value)
    {
      values[0] = UtilityFunctions::json_value(*value.value, parameter_precision);
      range_item["dataType"] = Json::Value(isAviProducer ? "string" : "float");
    }
    else
    {
      values[0] = Json::Value();
      range_item["dataType"] = Json::Value(isAviProducer ? "string" : "float");
    }
  }
  catch (...)
  {
    throw Fmi::Exception::Trace(BCP, "Operation failed!");
  }
}

void set_level_value(const bool &levels_present, const int &level, Json::Value &domain_axes)
{
  try
  {
    if (levels_present)
    {
      auto domain_axes_z = Json::Value(Json::ValueType::objectValue);
      domain_axes_z["values"] = Json::Value(Json::ValueType::arrayValue);
      domain_axes_z["values"][0] = Json::Value(level);
      domain_axes["z"] = domain_axes_z;
    }
  }
  catch (...)
  {
    throw Fmi::Exception::Trace(BCP, "Operation failed!");
  }
}

void process_values(const std::string & /* parameter_name */,
                    const std::string &output_name,
                    const int &parameter_precision,
                    const int &longitude_precision,
                    const int &latitude_precision,
                    const bool &isAviProducer,
                    const std::vector<time_coord_value> &values,
                    const bool &levels_present,
                    const int &level,
                    Json::Value &coverages)
{
  try
  {
    for (const auto &value : values)
    {
      auto coverage = Json::Value(Json::ValueType::objectValue);
      coverage["type"] = Json::Value("Coverage");
      auto domain = Json::Value(Json::ValueType::objectValue);
      domain["type"] = Json::Value("Domain");
      auto domain_axes = Json::Value(Json::ValueType::objectValue);
      auto domain_axes_x = Json::Value(Json::ValueType::objectValue);
      domain_axes_x["values"] = Json::Value(Json::ValueType::arrayValue);
      auto data_type = Json::Value(Json::ValueType::objectValue);
      domain_axes_x["values"][0] = Json::Value(value.lon, longitude_precision);
      auto domain_axes_y = Json::Value(Json::ValueType::objectValue);
      domain_axes_y["values"] = Json::Value(Json::ValueType::arrayValue);
      domain_axes_y["values"][0] = Json::Value(value.lat, latitude_precision);
      auto domain_axes_t = Json::Value(Json::ValueType::objectValue);
      domain_axes_t["values"] = Json::Value(Json::ValueType::arrayValue);
      domain_axes_t["values"][0] = Json::Value(value.time);
      domain_axes["x"] = domain_axes_x;
      domain_axes["y"] = domain_axes_y;
      domain_axes["t"] = domain_axes_t;
      set_level_value(levels_present, level, domain_axes);

      domain["axes"] = domain_axes;
      coverage["domain"] = domain;

      auto range_item = Json::Value(Json::ValueType::objectValue);
      range_item["type"] = Json::Value("NdArray");
      auto shape = Json::Value(Json::ValueType::arrayValue);
      shape[0] = Json::Value(1);
      shape[1] = Json::Value(1);
      shape[2] = Json::Value(1);
      if (levels_present)
        shape[3] = Json::Value(1);

      range_item["shape"] = shape;
      auto axis_names = Json::Value(Json::ValueType::arrayValue);
      set_axis_names(levels_present, axis_names);

      range_item["axisNames"] = axis_names;
      auto parameter_values = Json::Value(Json::ValueType::arrayValue);
      set_parameter_value(value, parameter_precision, isAviProducer, parameter_values, range_item);

      range_item["values"] = parameter_values;
      auto ranges = Json::Value(Json::ValueType::objectValue);
      ranges[output_name] = range_item;
      coverage["ranges"] = ranges;
      coverages[coverages.size()] = coverage;
    }
  }
  catch (...)
  {
    throw Fmi::Exception::Trace(BCP, "Operation failed!");
  }
}

void process_coverage_collection_point_parameter(const DataPerLevel &dpl,
                                                 const std::string &parameter_name,
                                                 const std::string &output_name,
                                                 const int &parameter_precision,
                                                 const int &longitude_precision,
                                                 const int &latitude_precision,
                                                 bool isAviProducer,
                                                 bool &levels_present,
                                                 Json::Value &coverages)
{
  try
  {
    for (const auto &dpl_item : dpl)
    {
      int level = dpl_item.first;
      // Levels present in some item
      levels_present = (levels_present || (dpl_item.first != std::numeric_limits<double>::max()));

      auto values = dpl_item.second;

      process_values(parameter_name,
                     output_name,
                     parameter_precision,
                     longitude_precision,
                     latitude_precision,
                     isAviProducer,
                     values,
                     levels_present,
                     level,
                     coverages);
    }
  }
  catch (...)
  {
    throw Fmi::Exception::Trace(BCP, "Operation failed!");
  }
}

Json::Value format_coverage_collection_point(const DataPerParameter &dpp,
                                             const ParameterNames &dpn,
                                             const EDRMetaData &emd,
                                             const std::vector<Spine::Parameter> &query_parameters,
                                             const CustomDimReferences &custom_dim_refs,
                                             const std::string &language)
{
  try
  {
    //	std::cout << "format_coverage_collection_point" << std::endl;

    Json::Value coverage_collection;

    if (dpp.empty())
      return coverage_collection;

    const auto &longitude_precision = emd.getPrecision("longitude");
    const auto &latitude_precision = emd.getPrecision("latitude");
    auto isAviProducer = emd.isAviProducer();

    bool levels_present = false;
    auto coverages = Json::Value(Json::ValueType::arrayValue);
    auto output_name = dpn.cbegin();
    for (const auto &dpp_item : dpp)
    {
      const auto &parameter_name = dpp_item.first;
      const auto &parameter_precision = emd.getPrecision(parameter_name);
      const auto &dpl = dpp_item.second;

      // Iterate data per level
      process_coverage_collection_point_parameter(dpl,
                                                  parameter_name,
                                                  output_name->second,
                                                  parameter_precision,
                                                  longitude_precision,
                                                  latitude_precision,
                                                  isAviProducer,
                                                  levels_present,
                                                  coverages);
      output_name++;
    }

    coverage_collection =
        add_prologue_coverage_collection(emd, query_parameters, levels_present, "Point",
        custom_dim_refs, language);
    coverage_collection["coverages"] = coverages;

    return coverage_collection;
  }
  catch (...)
  {
    throw Fmi::Exception::Trace(BCP, "Operation failed!");
  }
}

void process_coverage_collection_trajectory_parameter(const DataPerLevel &dpl,
                                                      const std::string &parameter_name,
                                                      const int &parameter_precision,
                                                      const int &longitude_precision,
                                                      const int &latitude_precision,
                                                      bool &levels_present,
                                                      Json::Value &coverages)
{
  try
  {
    for (const auto &dpl_item : dpl)
    {
      int level = dpl_item.first;
      // Levels present in some item
      levels_present = (levels_present || (dpl_item.first != std::numeric_limits<double>::max()));

      auto values = dpl_item.second;
      auto data_values = Json::Value(Json::ValueType::arrayValue);
      auto time_coord_values = Json::Value(Json::ValueType::arrayValue);
      for (unsigned int i = 0; i < values.size(); i++)
      {
        const auto &value = values.at(i);
        auto time_coord_value = Json::Value(Json::ValueType::arrayValue);
        time_coord_value[0] = Json::Value(value.time);
        time_coord_value[1] = Json::Value(value.lon, longitude_precision);
        time_coord_value[2] = Json::Value(value.lat, latitude_precision);
        if (levels_present)
          time_coord_value[3] = Json::Value(level);
        time_coord_values[i] = time_coord_value;
        if (value.value)
          data_values[i] = UtilityFunctions::json_value(*value.value, parameter_precision);
        else
          data_values[i] = Json::Value();
      }

      auto coverage = Json::Value(Json::ValueType::objectValue);
      coverage["type"] = Json::Value("Coverage");
      auto coverage_domain = Json::Value(Json::ValueType::objectValue);
      coverage_domain["type"] = Json::Value("Domain");

      auto domain_axes = Json::Value(Json::ValueType::objectValue);
      auto domain_axes_composite = Json::Value(Json::ValueType::objectValue);
      domain_axes_composite["dataType"] = Json::Value("tuple");
      auto domain_axes_coordinates = Json::Value(Json::ValueType::arrayValue);
      domain_axes_coordinates[0] = Json::Value("t");
      domain_axes_coordinates[1] = Json::Value("x");
      domain_axes_coordinates[2] = Json::Value("y");
      if (levels_present)
        domain_axes_coordinates[3] = Json::Value("z");
      domain_axes_composite["coordinates"] = domain_axes_coordinates;
      domain_axes_composite["values"] = time_coord_values;
      domain_axes["composite"] = domain_axes_composite;
      coverage_domain["axes"] = domain_axes;

      auto domain_ranges = Json::Value(Json::ValueType::objectValue);
      auto domain_parameter = Json::Value(Json::ValueType::objectValue);
      domain_parameter["type"] = Json::Value("NdArray");
      domain_parameter["dataType"] = Json::Value("float");
      auto axis_names = Json::Value(Json::ValueType::arrayValue);
      axis_names[0] = Json::Value("composite");
      domain_parameter["axisNames"] = axis_names;
      auto shape = Json::Value(Json::ValueType::arrayValue);
      shape[0] = Json::Value(values.size());
      domain_parameter["shape"] = shape;
      domain_parameter["values"] = data_values;
      domain_ranges[parameter_name] = domain_parameter;
      coverage["domain"] = coverage_domain;
      coverage["ranges"] = domain_ranges;

      coverages[coverages.size()] = coverage;
    }
  }
  catch (...)
  {
    throw Fmi::Exception::Trace(BCP, "Operation failed!");
  }
}

Json::Value format_coverage_collection_trajectory(
    const DataPerParameter &dpp,
    const ParameterNames &dpn,
    const EDRMetaData &emd,
    const std::vector<Spine::Parameter> &query_parameters,
    const CustomDimReferences &custom_dim_refs,
    const std::string &language)
{
  try
  {
    //	  std::cout << "format_coverage_collection_trajectory" << std::endl;

    Json::Value coverage_collection;

    if (dpp.empty())
      return coverage_collection;

    const auto &longitude_precision = emd.getPrecision("longitude");
    const auto &latitude_precision = emd.getPrecision("latitude");

    bool levels_present = false;
    auto coverages = Json::Value(Json::ValueType::arrayValue);
    auto output_name = dpn.cbegin();
    for (const auto &dpp_item : dpp)
    {
      const auto &parameter_name = dpp_item.first;
      const auto &dpl = dpp_item.second;
      const auto &parameter_precision = emd.getPrecision(parameter_name);

      process_coverage_collection_trajectory_parameter(dpl,
                                                       output_name->second,
                                                       parameter_precision,
                                                       longitude_precision,
                                                       latitude_precision,
                                                       levels_present,
                                                       coverages);
      output_name++;
    }
    coverage_collection =
        add_prologue_coverage_collection(emd, query_parameters, levels_present, "Trajectory",
                                         custom_dim_refs, language);
    coverage_collection["coverages"] = coverages;

    return coverage_collection;
  }
  catch (...)
  {
    throw Fmi::Exception::Trace(BCP, "Operation failed!");
  }
}

double get_level(const TS::TimeSeriesGroupPtr &tsg_level,
                 const bool &levels_present,
                 const unsigned int &tsg_index,
                 const unsigned int &llts_index)
{
  try
  {
    double level = std::numeric_limits<double>::max();

    if (levels_present)
    {
      const auto &llts_level = tsg_level->at(tsg_index);
      const auto &level_value = llts_level.timeseries.at(llts_index);
      if (const auto *ptr = std::get_if<double>(&level_value.value))
      {
        level = *ptr;
      }
      else if (const auto *ptr = std::get_if<int>(&level_value.value))
      {
        level = *ptr;
      }
    }

    return level;
  }
  catch (...)
  {
    throw Fmi::Exception::Trace(BCP, "Operation failed!");
  }
}

void add_time_coord_value(const TS::LonLatTimeSeries &llts_data,
                          const TS::LonLatTimeSeries &llts_lon,
                          const TS::LonLatTimeSeries &llts_lat,
                          const TS::TimeSeriesGroupPtr &tsg_level,
                          const unsigned int tsg_index,
                          const bool &levels_present,
                          unsigned int &levels_index,
                          const CoordinateFilter &coordinate_filter,
                          DataPerLevel &dpl)
{
  try
  {
    for (unsigned int lev_idx = 0; lev_idx < llts_data.timeseries.size(); lev_idx++)
    {
      const auto &data_value = llts_data.timeseries.at(lev_idx);
      const auto &lon_value = llts_lon.timeseries.at(lev_idx);
      const auto &lat_value = llts_lat.timeseries.at(lev_idx);

      time_coord_value tcv;
      tcv.lon = as_double(lon_value.value);
      tcv.lat = as_double(lat_value.value);
      tcv.time = (Fmi::date_time::to_iso_extended_string(data_value.time.utc_time()) + "Z");

      double level = get_level(tsg_level, levels_present, tsg_index, lev_idx);

      if (data_value.value != TS::None())
        tcv.value = data_value.value;
      bool accept = coordinate_filter.accept(tcv.lon, tcv.lat, level, data_value.time.utc_time());

      if (accept)
        dpl[level].push_back(tcv);

      if (levels_present)
      {
        levels_index++;
        if (levels_index >= tsg_level->size())
          levels_index = 0;
      }
    }
  }
  catch (...)
  {
    throw Fmi::Exception::Trace(BCP, "Operation failed!");
  }
}

DataPerLevel get_data_per_level(const TS::TimeSeriesGroupPtr &tsg_data,
                                const TS::TimeSeriesGroupPtr &tsg_lon,
                                const TS::TimeSeriesGroupPtr &tsg_lat,
                                const TS::TimeSeriesGroupPtr &tsg_level,
                                const bool &levels_present,
                                const CoordinateFilter &coordinate_filter)
{
  try
  {
    DataPerLevel dpl;

    unsigned int levels_index = 0;
    for (unsigned int k = 0; k < tsg_data->size(); k++)
    {
      const auto &llts_data = tsg_data->at(k);
      const auto &llts_lon = tsg_lon->at(k);
      const auto &llts_lat = tsg_lat->at(k);

      add_time_coord_value(llts_data,
                           llts_lon,
                           llts_lat,
                           tsg_level,
                           k,
                           levels_present,
                           levels_index,
                           coordinate_filter,
                           dpl);
    }
    return dpl;
  }
  catch (...)
  {
    throw Fmi::Exception::Trace(BCP, "Operation failed!");
  }
}

void process_parameter_data(const std::vector<TS::TimeSeriesData> &outdata,
                            const unsigned int &longitude_index,
                            const unsigned int &latitude_index,
                            const unsigned int &level_index,
                            const CoordinateFilter &coordinate_filter,
                            const std::vector<Spine::Parameter> &query_parameters,
                            const bool &levels_present,
                            DataPerParameter &dpp,
                            ParameterNames &dpn)
{
  try
  {
    for (unsigned int j = 0; j < outdata.size(); j++)
    {
      auto parameter_name = parse_parameter_name(query_parameters[j].name());
      boost::algorithm::to_lower(parameter_name);

      if (lon_lat_level_param(parameter_name))
        continue;

      auto tsdata = outdata.at(j);
      auto tslon = outdata.at(longitude_index);
      auto tslat = outdata.at(latitude_index);
      TS::TimeSeriesGroupPtr tsg_data = std::get<TS::TimeSeriesGroupPtr>(tsdata);
      TS::TimeSeriesGroupPtr tsg_lon = std::get<TS::TimeSeriesGroupPtr>(tslon);
      TS::TimeSeriesGroupPtr tsg_lat = std::get<TS::TimeSeriesGroupPtr>(tslat);
      TS::TimeSeriesGroupPtr tsg_level = nullptr;
      if (levels_present)
      {
        auto tslevel = outdata.at(level_index);
        tsg_level = std::get<TS::TimeSeriesGroupPtr>(tslevel);
      }

      DataPerLevel dpl = get_data_per_level(
          tsg_data, tsg_lon, tsg_lat, tsg_level, levels_present, coordinate_filter);
      dpp[parameter_name] = dpl;
      dpn[parameter_name] = parse_parameter_name(query_parameters[j].originalName());
    }
  }
  catch (...)
  {
    throw Fmi::Exception::Trace(BCP, "Operation failed!");
  }
}

DataPerParameter get_data_per_parameter(const TS::OutputData &outputData,
                                        const std::set<int> &levels,
                                        const CoordinateFilter &coordinate_filter,
                                        const std::vector<Spine::Parameter> &query_parameters,
                                        ParameterNames &dpn)
{
  try
  {
    DataPerParameter dpp;

    //	std::cout << "get_output_data_per_parameter" << std::endl;

    bool levels_present = !levels.empty();

    // Get indexes of longitude, latitude, level
    unsigned int longitude_index;
    unsigned int latitude_index;
    unsigned int level_index;
    if (levels_present)
    {
      level_index = (query_parameters.size() - 1);
      latitude_index = (query_parameters.size() - 2);
      longitude_index = (query_parameters.size() - 3);
    }
    else
    {
      level_index =
          -1;  // Not actually used due to levels_present=false, but silence CodeChecker error
               // FIXME: it would be better to use std::optional, but unfortuynaly changes
               //        seems tyo escalate
      latitude_index = (query_parameters.size() - 1);
      longitude_index = (query_parameters.size() - 2);
    }

    for (const auto &output : outputData)
    {
      const auto &outdata = output.second;

      // iterate columns (parameters)
      process_parameter_data(outdata,
                             longitude_index,
                             latitude_index,
                             level_index,
                             coordinate_filter,
                             query_parameters,
                             levels_present,
                             dpp,
                             dpn);
    }
    return dpp;
  }
  catch (...)
  {
    throw Fmi::Exception::Trace(BCP, "Operation failed!");
  }
}

Json::Value format_output_data_coverage_collection(
    const TS::OutputData &outputData,
    const EDRMetaData &emd,
    const std::set<int> &levels,
    const CoordinateFilter &coordinate_filter,
    const std::vector<Spine::Parameter> &query_parameters,
    EDRQueryType query_type,
    const CustomDimReferences &custom_dim_refs,
    const std::string &language)
{
  try
  {
    if (outputData.empty())
      Json::Value();

    ParameterNames dpn;
    auto dpp = get_data_per_parameter(outputData, levels, coordinate_filter, query_parameters, dpn);

    if (query_type == EDRQueryType::Trajectory)
      return format_coverage_collection_trajectory(
          dpp, dpn, emd, query_parameters, custom_dim_refs, language);

    return format_coverage_collection_point(
        dpp, dpn, emd, query_parameters, custom_dim_refs, language);
  }
  catch (...)
  {
    throw Fmi::Exception::Trace(BCP, "Operation failed!");
  }
}

void add_parameter(const std::string &parameter_name,
                   const int &parameter_precision,
                   const TS::TimeSeriesPtr &ts_data,
                   const TS::TimeSeriesPtr &ts_level,
                   const int &level_precision,
                   const bool &firstParameter,
                   unsigned int &data_index,
                   Json::Value &values_array,
                   Json::Value &level_values,
                   std::map<std::string, Json::Value> &parameter_data_type)
{
  try
  {
    Json::Value data_type_level;
    Json::Value data_type_data;
    for (unsigned int k = 0; k < ts_data->size(); k++)
    {
      const auto &data_value = ts_data->at(k);
      const auto &level_value = ts_level->at(k);
      add_value(data_value, values_array, data_type_data, data_index, parameter_precision);
      // All parameters have the same levels
      if (firstParameter)
        add_value(level_value, level_values, data_type_level, data_index, level_precision);
      if (parameter_data_type.find(parameter_name) == parameter_data_type.end())
        parameter_data_type[parameter_name] = data_type_data;
      data_index++;
    }
  }
  catch (...)
  {
    throw Fmi::Exception::Trace(BCP, "Operation failed!");
  }
}

Json::Value format_output_data_vertical_profile(
    const TS::OutputData &outputData,
    const EDRMetaData &emd,
    const std::set<int> &levels,
    const CoordinateFilter & /* coordinate_filter */,
    const std::vector<Spine::Parameter> &query_parameters,
    EDRQueryType /* query_type */,
    bool useDataLevels,
    const CustomDimReferences &custom_dim_refs,
    const std::string &language)
{
  try
  {
    if (outputData.empty())
      Json::Value();

    Json::Value coverageCollection;

    const auto &longitude_precision = emd.getPrecision("longitude");
    const auto &latitude_precision = emd.getPrecision("latitude");
    auto level_precision = (useDataLevels ? emd.getPrecision("level_pressure") : 0);
    unsigned int level_index = (query_parameters.size() - 1);
    unsigned int latitude_index = (query_parameters.size() - 2);
    unsigned int longitude_index = (query_parameters.size() - 3);

    // Only one position
    const auto &outdata = outputData.front().second;

    std::map<std::string, Json::Value> parameter_data_values;
    std::map<std::string, Json::Value> parameter_data_type;
    //	std::map<std::string, Json::Value> parameter_level_values;
    auto level_values = Json::Value(Json::ValueType::arrayValue);
    auto values_array = Json::Value(Json::ValueType::arrayValue);
    std::string prev_param;
    unsigned int data_index = 0;
    auto isGridProducer = emd.isGridProducer();
    // iterate columns (parameters)
    for (unsigned int j = 0; j < outdata.size(); j++)
    {
      auto parameter_name = query_parameters[j].name();
      const auto &parameter_precision = emd.getPrecision(parameter_name);
      boost::algorithm::to_lower(parameter_name);
      if (lon_lat_level_param(parameter_name))
        continue;

      auto firstParameter = prev_param.empty();
      auto resetDataIndex = ((!firstParameter) && (!isGridProducer));
      auto tsdata = outdata.at(j);
      auto tslevel = outdata.at(level_index);
      TS::TimeSeriesPtr ts_data = std::get<TS::TimeSeriesPtr>(tsdata);
      TS::TimeSeriesPtr ts_level = std::get<TS::TimeSeriesPtr>(tslevel);

      parameter_name = parse_parameter_name(query_parameters[j].originalName());

      if (isGridProducer)
      {
        // Parameter's levels are fetched as separate parameters, append their data
        // into single parameter/values_array

        if (parameter_name != prev_param)
        {
          if (!firstParameter)
          {
            parameter_data_values[prev_param] = values_array;
            resetDataIndex = true;
          }

          prev_param = parameter_name;
        }
      }

      if (resetDataIndex)
      {
        values_array = Json::Value(Json::ValueType::arrayValue);
        data_index = 0;
      }

      add_parameter(parameter_name,
                    parameter_precision,
                    ts_data,
                    ts_level,
                    level_precision,
                    firstParameter,
                    data_index,
                    values_array,
                    level_values,
                    parameter_data_type);

      if (!isGridProducer)
      {
        parameter_data_values[parameter_name] = values_array;
        prev_param = parameter_name;
      }
    }

    if (isGridProducer && (!prev_param.empty()))
      parameter_data_values[prev_param] = values_array;

    if (parameter_data_values.empty())
      return coverageCollection;

    coverageCollection = Json::Value(Json::ValueType::objectValue);
    coverageCollection["type"] = Json::Value("CoverageCollection");
    coverageCollection["domainType"] = Json::Value("VerticalProfile");
    coverageCollection["parameters"] =
        get_edr_series_parameters(query_parameters, emd, custom_dim_refs, language);
    coverageCollection["coverages"] = Json::Value(Json::ValueType::arrayValue);

    auto coordinates = get_coordinates(outputData, query_parameters);
    auto axis_names = Json::Value(Json::ValueType::arrayValue);
    axis_names[0] = Json::Value("z");

    auto tslon = outdata.at(longitude_index);
    auto tslat = outdata.at(latitude_index);
    TS::TimeSeriesPtr ts_lon = std::get<TS::TimeSeriesPtr>(tslon);
    TS::TimeSeriesPtr ts_lat = std::get<TS::TimeSeriesPtr>(tslat);
    // Only one position, timestep
    const auto &lon_timed_value = ts_lon->front();
    const auto &lat_timed_value = ts_lat->front();

    // Referencing x,y coordinates
    auto referencing = Json::Value(Json::ValueType::arrayValue);
    auto referencing_xy = Json::Value(Json::ValueType::objectValue);
    referencing_xy["coordinates"] = Json::Value(Json::ValueType::arrayValue);
    referencing_xy["coordinates"][0] = Json::Value("y");
    referencing_xy["coordinates"][1] = Json::Value("x");
    referencing_xy["system"] = Json::Value(Json::ValueType::objectValue);
    referencing_xy["system"]["type"] = Json::Value("GeographicCRS");
    referencing_xy["system"]["id"] = Json::Value("https://www.opengis.net/def/crs/OGC/1.3/CRS84");

    // Referencing z coordinate
    auto referencing_z = Json::Value(Json::ValueType::objectValue);
    referencing_z["coordinates"] = Json::Value(Json::ValueType::arrayValue);
    referencing_z["coordinates"][0] = Json::Value("z");
    referencing_z["system"] = Json::Value(Json::ValueType::objectValue);
    referencing_z["system"]["type"] = Json::Value("VerticalCRS");
    referencing_z["system"]["id"] = Json::Value(emd.vertical_extent.level_type);

    auto referencing_time = Json::Value(Json::ValueType::objectValue);
    referencing_time["coordinates"] = Json::Value(Json::ValueType::arrayValue);
    referencing_time["coordinates"][0] = Json::Value("t");
    referencing_time["system"] = Json::Value(Json::ValueType::objectValue);
    referencing_time["system"]["type"] = Json::Value("TemporalRS");
    referencing_time["system"]["calendar"] = Json::Value("Gregorian");

    referencing[0] = referencing_xy;
    referencing[1] = referencing_z;
    referencing[2] = referencing_time;

    coverageCollection["referencing"] = referencing;

    // Iterate coverages/timesteps
    auto timeIter = ts_lon->begin();
    auto tI = timeIter;
    size_t timeSteps = 0;
    size_t tS = 0;

    if (useDataLevels)
      timeSteps = ts_lon->size();
    else
      timeSteps = (isGridProducer ? ts_lon->size() : (ts_lon->size() / levels.size()));

    for (std::size_t tStep = 0, coverageIdx = 0; (tStep < timeSteps);)
    {
      auto ranges = Json::Value(Json::ValueType::objectValue);
      auto domain = Json::Value(Json::ValueType::objectValue);

      domain["type"] = Json::Value("Domain");
      domain["domainType"] = Json::Value("VerticalProfile");

      domain["axes"] = Json::Value(Json::ValueType::objectValue);
      domain["axes"]["x"] = Json::Value(Json::ValueType::objectValue);
      domain["axes"]["x"]["values"] = Json::Value(Json::ValueType::arrayValue);
      auto &x_array = domain["axes"]["x"]["values"];
      domain["axes"]["y"] = Json::Value(Json::ValueType::objectValue);
      domain["axes"]["y"]["values"] = Json::Value(Json::ValueType::arrayValue);
      auto &y_array = domain["axes"]["y"]["values"];

      // One position coordinates do not change
      x_array[0] = UtilityFunctions::json_value(lon_timed_value.value, longitude_precision);
      y_array[0] = UtilityFunctions::json_value(lat_timed_value.value, latitude_precision);

      auto levelArray = Json::Value(Json::ValueType::arrayValue);
      std::size_t levelIdx = 0;
      if (emd.vertical_extent.level_type == "PressureLevel")
      {
        if (!useDataLevels)
        {
          for (auto level = levels.crbegin(); (level != levels.crend()); level++)
            levelArray[levelIdx++] = Json::Value(*level);
        }
      }
      else
      {
        for (auto level : levels)
          levelArray[levelIdx++] = Json::Value(level);
      }

      std::string timestep =
          (Fmi::date_time::to_iso_extended_string(timeIter->time.utc_time()) + "Z");
      auto t_array = Json::Value(Json::ValueType::arrayValue);
      t_array[0] = Json::Value(timestep);
      domain["axes"]["t"] = Json::Value(Json::ValueType::objectValue);
      domain["axes"]["t"]["values"] = t_array;

      for (auto &item : parameter_data_values)
      {
        const auto &param_name = item.first;

        // BRAINSTORM-3116 'level' kludge, FIXME !
        //
        if (param_name == EDR_OBSERVATION_LEVEL)
          continue;

        auto param_range = Json::Value(Json::ValueType::objectValue);
        param_range["type"] = Json::Value("NdArray");
        param_range["dataType"] = parameter_data_type.at(param_name);  // Json::Value("float");
        param_range["axisNames"] = axis_names;

        auto valueArray = Json::Value(Json::ValueType::arrayValue);

        if (useDataLevels)
        {
          // Observation (sounding) data is in timestep order and itemIdx runs every row (level)
          // until time changes

          size_t valIdx = 0;
          size_t itemIdx = tStep;
          tS = tStep;

          bool loadLevels = (levelArray.size() == 0);

          for (tI = timeIter; (itemIdx < timeSteps); itemIdx++, valIdx++, tI++)
          {
            if ((itemIdx != tS) && (tI->time != timeIter->time))
              break;

            valueArray[valIdx] = item.second[itemIdx];

            if (loadLevels)
              levelArray[levelIdx++] = level_values[itemIdx];
          }

          tS = itemIdx;
        }
        else
        {
          // Every n'th item value has data for the timestep
          std::size_t itemCnt = (isGridProducer ? item.second.size() : ts_lon->size());
          for (std::size_t itemIdx = tStep, valIdx = 0; (itemIdx < itemCnt);
               itemIdx += timeSteps, valIdx++)
            valueArray[valIdx] = item.second[itemIdx];
        }

        auto shape = Json::Value(Json::ValueType::arrayValue);
        shape[0] = valueArray.size();

        param_range["shape"] = shape;
        param_range["values"] = valueArray;
        ranges[param_name] = param_range;
      }

      domain["axes"]["z"] = Json::Value(Json::ValueType::objectValue);
      domain["axes"]["z"]["values"] = levelArray;

      Json::Value coverage = Json::Value(Json::ValueType::objectValue);
      coverage["type"] = Json::Value("Coverage");
      coverage["domain"] = domain;
      coverage["ranges"] = ranges;
      coverageCollection["coverages"][coverageIdx++] = coverage;

      if (useDataLevels)
      {
        tStep = tS;
        timeIter = tI;
      }
      else
      {
        tStep++;
        timeIter++;
      }
    }

    return coverageCollection;
  }
  catch (...)
  {
    throw Fmi::Exception::Trace(BCP, "Operation failed!");
  }
}

Json::Value parse_locations(const std::string &producer, const EngineMetaData &emd)
{
  try
  {
    Json::Value result;

    const EDRMetaData *edr_md = nullptr;
    const auto &metadata = emd.getMetaData();
    for (const auto &item : metadata)
    {
      const auto &engine_metadata = item.second;
      if (engine_metadata.find(producer) != engine_metadata.end())
      {
        edr_md = &engine_metadata.at(producer).front();
        break;
      }
    }

    // 1. querydata, 2. grid, 3. observation
    // All instances of a collection share the same locations, so just get the
    // metadata of first instance

    if (!edr_md || !edr_md->locations)
      return result;

    bool soundingObservations = (edr_md->isObsProducer() && (producer == SOUNDING_PRODUCER));
    auto longitude_precision = edr_md->getPrecision("longitude");
    auto latitude_precision = edr_md->getPrecision("latitude");

    result["type"] = Json::Value("FeatureCollection");
    auto features = Json::Value(Json::ValueType::arrayValue);

    auto now = Fmi::SecondClock::universal_time();

    for (const auto &item : *edr_md->locations)
    {
      const auto &loc = item.second;
      auto feature = Json::Value(Json::ValueType::objectValue);
      feature["type"] = Json::Value("Feature");
      feature["id"] = Json::Value(loc.id);
      auto geometry = Json::Value(Json::ValueType::objectValue);

      if ((!(edr_md->isAviProducer())) || (!edr_md->getGeometry(loc.id, geometry)))
      {
        geometry["type"] = Json::Value("Point");
        auto coordinates = Json::Value(Json::ValueType::arrayValue);
        coordinates[0] = Json::Value(loc.longitude, longitude_precision);
        coordinates[1] = Json::Value(loc.latitude, latitude_precision);
        geometry["coordinates"] = coordinates;
      }
      auto properties = Json::Value(Json::ValueType::objectValue);
      properties["name"] = Json::Value(loc.name);
      auto detail_string = ("Id is " + loc.type);
      if (!loc.keyword.empty())
        detail_string.append(" from " + loc.keyword);
      properties["detail"] = Json::Value(detail_string);
      if (!edr_md->temporal_extent.time_periods.empty())
      {
        Fmi::DateTime start_time;
        Fmi::DateTime end_time;

        if (soundingObservations)
        {
          try
          {
            auto stationId = Fmi::stoi(loc.id);
            auto it = edr_md->stationMetaData.find(stationId);

            if (it != edr_md->stationMetaData.end())
            {
              start_time = it->second.period.begin();
              end_time = it->second.period.end();
            }
          }
          catch (...)
          {
          }
        }
        else if (edr_md->isAviProducer())
        {
          // BRAINSTORM-3320
          //
          // Station's temporal extent, varies e.g. for taf collection
          //
          auto it = edr_md->stationTemporalExtentMetaData.find(loc.id);

          if (it != edr_md->stationTemporalExtentMetaData.end())
          {
            start_time = it->second.time_periods.front().start_time;
            end_time = it->second.time_periods.back().end_time;
          }
        }
        else
        {
          start_time = loc.start_time;
          end_time = loc.end_time;
        }

        if (start_time.is_not_a_date_time())
          start_time = edr_md->temporal_extent.time_periods.front().start_time;
        if (end_time.is_not_a_date_time())
          end_time = edr_md->temporal_extent.time_periods.back().end_time;
        if (end_time.is_not_a_date_time())
          end_time = edr_md->temporal_extent.time_periods.back().start_time;
        if (end_time > now)
          end_time = now;

        properties["datetime"] = Json::Value(Fmi::to_iso_extended_string(start_time) + "Z/" +
                                             Fmi::to_iso_extended_string(end_time) + "Z");
      }
      feature["geometry"] = geometry;
      feature["properties"] = properties;
      features[features.size()] = feature;
    }

    if (edr_md->isAviProducer() && !edr_md->locations->empty())
    {
      auto all = Json::Value(Json::ValueType::objectValue);
      all["id"] = "all";
      all["type"] = "Feature";

      auto bbox = Json::Value(Json::ValueType::arrayValue);
      bbox[0] = Json::Value(edr_md->spatial_extent.bbox_xmin);
      bbox[1] = Json::Value(edr_md->spatial_extent.bbox_ymin);
      bbox[2] = Json::Value(edr_md->spatial_extent.bbox_xmax);
      bbox[3] = Json::Value(edr_md->spatial_extent.bbox_ymax);
      all["bbox"] = bbox;

      auto properties = Json::Value(Json::ValueType::objectValue);
      auto start_time = edr_md->temporal_extent.single_time_periods.front().start_time;
      auto end_time = edr_md->temporal_extent.single_time_periods.back().start_time;
      properties["datetime"] = Json::Value(Fmi::to_iso_extended_string(start_time) + "Z/" +
                                           Fmi::to_iso_extended_string(end_time) + "Z");
      properties["detail"] = "Id is special location";
      properties["name"] = "Special logical selector representing all collection locations";
      all["properties"] = properties;

      features[features.size()] = all;
    }

    result["features"] = features;

    return result;
  }
  catch (...)
  {
    throw Fmi::Exception::Trace(BCP, "Operation failed!");
  }
}

}  // namespace

Json::Value formatOutputData(const TS::OutputData &outputData,
                             const EDRMetaData &emd,
                             EDRQueryType query_type,
                             const std::set<int> &levels,
                             const CoordinateFilter &coordinate_filter,
                             const std::vector<Spine::Parameter> &query_parameters,
                             bool useDataLevels,
                             const CustomDimReferences &custom_dim_refs,
                             const std::string &language)
{
  try
  {
    Json::Value empty_result;

    if (outputData.empty())
      return empty_result;

    const auto &outdata_first = outputData.at(0).second;

    if (outdata_first.empty())
      return empty_result;

    const auto &tsdata_first = outdata_first.at(0);

    if (std::get_if<TS::TimeSeriesPtr>(&tsdata_first))
    {
      // Zero or one levels
      if (levels.size() <= 1)
      {
        std::optional<int> level;
        if (levels.size() == 1)
          level = *(levels.begin());
        return format_output_data_one_point(
            outputData, emd, level, query_parameters, custom_dim_refs, language);
      }

      // More than one level
      return format_output_data_vertical_profile(
          outputData, emd, levels, coordinate_filter, query_parameters, query_type, useDataLevels,
          custom_dim_refs, language);
      //      return format_output_data_position(outputData, emd, query_parameters);
    }

    if (const auto *ptr = std::get_if<TS::TimeSeriesVectorPtr>(&tsdata_first))
    {
      if (outdata_first.size() > 1)
        std::cout << "formatOutputData - TS::TimeSeriesVectorPtr - Can do "
                     "nothing -> report error!\n";
      std::vector<TS::TimeSeriesData> tsd;
      TS::TimeSeriesVectorPtr tsv = *ptr;
      for (const auto &ts : *tsv)
      {
        TS::TimeSeriesPtr tsp(new TS::TimeSeries(ts));
        tsd.emplace_back(tsp);
      }
      TS::OutputData od;
      od.emplace_back("_obs_", tsd);
      // Zero or one levels
      if (levels.size() <= 1)
      {
        std::optional<int> level;
        if (levels.size() == 1)
          level = *(levels.begin());
        return format_output_data_one_point(
            od, emd, level, query_parameters, custom_dim_refs, language);
      }
      // More than one level
      return format_output_data_vertical_profile(
          od, emd, levels, coordinate_filter, query_parameters, query_type, useDataLevels,
          custom_dim_refs, language);
    }

    if (std::get_if<TS::TimeSeriesGroupPtr>(&tsdata_first))
    {
      return format_output_data_coverage_collection(
          outputData, emd, levels, coordinate_filter, query_parameters, query_type,
          custom_dim_refs, language);
    }

    return empty_result;
  }
  catch (...)
  {
    throw Fmi::Exception::Trace(BCP, "Operation failed!");
  }
}

Json::Value reportError(int code, const std::string &description)
{
  auto ret = Json::Value(Json::ValueType::objectValue);

  ret["code"] = Json::Value(code);
  ret["description"] = Json::Value(description);

  return ret;
}

Json::Value parseEDRMetaData(const EDRQuery &edr_query,
                             const EngineMetaData &emd,
                             const ProducerLicenses &licenses,
                             const CustomDimReferences &custom_dim_refs)
{
  try
  {
    const std::string &producer = edr_query.collection_id;

    if (edr_query.query_id == EDRQueryId::SpecifiedCollectionLocations)
      return parse_locations(producer, emd);

    Json::Value result;

    if (producer.empty())
    {
      auto edr_metadata = Json::Value();
      const auto &metadata = emd.getMetaData();
      for (const auto &item : metadata)
      {
        const auto &engine_metadata = item.second;
        auto md = parse_edr_metadata(engine_metadata, edr_query, licenses, custom_dim_refs);
        edr_metadata.append(md);
      }

      // Add main level links
      Json::Value meta_data;
      auto link = Json::Value(Json::ValueType::objectValue);
      link["href"] = Json::Value(edr_query.host + "/collections");
      link["rel"] = Json::Value("self");
      link["type"] = Json::Value("application/json");
      link["title"] = Json::Value("Collections metadata in JSON");
      auto links = Json::Value(Json::ValueType::arrayValue);
      links[0] = link;
      auto license = parse_license_link(licenses, DEFAULT_LICENSE);
      if (!license.isNullOrEmpty())
        links[1] = license;
      result["links"] = links;
      result["collections"] = edr_metadata;
    }
    else
    {
      const EDRProducerMetaData *producer_metadata = nullptr;
      const auto &metadata = emd.getMetaData();
      // Iterate metadata of all engines and when producer is found parse its metadata
      for (const auto &item : metadata)
      {
        const auto &engine_metadata = item.second;
        if (engine_metadata.find(producer) != engine_metadata.end())
        {
          producer_metadata = &engine_metadata;
          break;
        }
      }
      if (producer_metadata)
        result = parse_edr_metadata(*producer_metadata, edr_query, licenses, custom_dim_refs);
    }
    return result;
  }
  catch (...)
  {
    throw Fmi::Exception::Trace(BCP, "Operation failed!");
  }
}

}  // namespace CoverageJson
}  // namespace EDR
}  // namespace Plugin
}  // namespace SmartMet
