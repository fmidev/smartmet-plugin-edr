#include "GeoJson.h"
#include "UtilityFunctions.h"
#include <macgyver/Exception.h>
#include <macgyver/Hash.h>
#include <macgyver/StringConversion.h>
#include <optional>

namespace SmartMet
{
namespace Plugin
{
namespace EDR
{
namespace GeoJson
{
namespace
{
struct coordinate_xyz
{
  coordinate_xyz() : x(0.0), y(0.0) {}
  coordinate_xyz(double x_val, double y_val, std::optional<double> z_val)
      : x(x_val), y(y_val), z(z_val)
  {
  }
  double x;
  double y;
  std::optional<double> z;
};

struct time_coord_value
{
  std::string time;
  double lon = 0;
  double lat = 0;
  std::optional<TS::Value> value;
};

using DataPerLevel = std::map<double, std::vector<time_coord_value>>;  // level -> array of values
using DataPerParameter = std::map<std::string, DataPerLevel>;          // parameter_name -> data
using ParameterNames = std::map<std::string, std::string>;             // parameter_name -> origname

std::size_t hash_value(const coordinate_xyz &coord)
{
  auto ret = Fmi::hash_value(coord.x);
  Fmi::hash_combine(ret, Fmi::hash_value(coord.y));
  if (coord.z)
    Fmi::hash_combine(ret, Fmi::hash_value(*coord.z));

  return ret;
}

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

    for (unsigned int i = 0; i < longitude_vector.size(); i++)
      ret.emplace_back(longitude_vector.at(i), latitude_vector.at(i));

    return ret;
  }
  catch (...)
  {
    throw Fmi::Exception::Trace(BCP, "Operation failed!");
  }
}

void get_coordinates(std::vector<double> &longitude_vector,
                     std::vector<double> &latitude_vector,
                     const TS::TimeSeriesGroupPtr &tsg,
                     const std::string &param_name)

{
  try
  {
    for (const auto &llts : *tsg)
    {
      const auto &ts = llts.timeseries;

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
        if (!ts->empty())
        {
          const TS::TimedValue &tv = ts->at(0);
          double value = as_double(tv.value);
          if (param_name == "longitude")
            longitude_vector.push_back(value);
          else
            latitude_vector.push_back(value);
        }
      }
      else if (std::get_if<TS::TimeSeriesVectorPtr>(&tsdata))
      {
        std::cout << "get_coordinates -> TS::TimeSeriesVectorPtr - Shouldnt be "
                     "here -> report error!!:\n"
                  << tsdata << '\n';
      }
      else if (const auto *ptr = std::get_if<TS::TimeSeriesGroupPtr>(&tsdata))
      {
        TS::TimeSeriesGroupPtr tsg = *ptr;

        get_coordinates(longitude_vector, latitude_vector, tsg, param_name);
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

Json::Value get_edr_series_parameters(const std::vector<Spine::Parameter> &query_parameters,
                                      const EDRMetaData &metadata)
{
  try
  {
    auto parameters = Json::Value(Json::ValueType::arrayValue);

    const auto &engine_parameter_info = metadata.parameters;
    const auto &config_parameter_info = *metadata.parameter_info;

    for (const auto &p : query_parameters)
    {
      auto parameter_name = parse_parameter_name(p.originalName());

      if (lon_lat_level_param(parameter_name))
        continue;

      const auto &edr_parameter = engine_parameter_info.at(parameter_name);

      auto pinfo = config_parameter_info.get_parameter_info(parameter_name, metadata.language);
      auto description = (!pinfo.description.empty() ? pinfo.description : "");
      auto label = (!pinfo.unit_label.empty() ? pinfo.unit_label : edr_parameter.name);
      auto symbol = (!pinfo.unit_symbol_value.empty() ? pinfo.unit_symbol_value : "");
      auto symbol_type = (!pinfo.unit_symbol_type.empty() ? pinfo.unit_symbol_type : "");

      auto parameter = Json::Value(Json::ValueType::objectValue);
      parameter["id"] = Json::Value(parameter_name);
      parameter["type"] = Json::Value("Parameter");
      // Description field is optional
      // QEngine returns parameter description in finnish and skandinavian
      // characters cause problems metoffice test interface uses description
      // field -> set parameter name to description field
      parameter["description"] = Json::Value(parameter_name);
      parameter["unit"] = Json::Value(Json::ValueType::objectValue);
      parameter["unit"]["label"] = Json::Value(label);
      parameter["unit"]["symbol"] = Json::Value(Json::ValueType::objectValue);
      parameter["unit"]["symbol"]["value"] = Json::Value(symbol);
      parameter["unit"]["symbol"]["type"] = Json::Value(symbol_type);

      parameter["observedProperty"] = Json::Value(Json::ValueType::objectValue);
      parameter["observedProperty"]["id"] = Json::Value(edr_parameter.name);
      parameter["observedProperty"]["label"] = Json::Value(edr_parameter.name);
      parameters[parameters.size()] = parameter;
    }

    return parameters;
  }
  catch (...)
  {
    throw Fmi::Exception::Trace(BCP, "Operation failed!");
  }
}

int to_int(const TS::TimedValue &tv)
{
  int ret = std::numeric_limits<int>::max();

  if (const auto *ptr = std::get_if<int>(&tv.value))
    ret = *ptr;
  else if (const auto *ptr = std::get_if<double>(&tv.value))
    ret = *ptr;

  return ret;
}

double to_double(const TS::TimedValue &tv)
{
  if (const auto *ptr = std::get_if<double>(&tv.value))
    return *ptr;

  return std::numeric_limits<double>::max();
}

void add_value(const TS::TimedValue &tv,
               Json::Value &values_array,
               unsigned int values_index,
               unsigned int precision)
{
  try
  {
    const auto &val = tv.value;

    if (const auto *ptr = std::get_if<double>(&val))
    {
      values_array[values_index] = Json::Value(*ptr, precision);
    }
    else if (const auto *ptr = std::get_if<int>(&val))
    {
      values_array[values_index] = Json::Value(*ptr);
    }
    else if (const auto *ptr = std::get_if<std::string>(&val))
    {
      values_array[values_index] = Json::Value(*ptr);
    }
    else
    {
      Json::Value nulljson;
      values_array[values_index] = nulljson;
    }
  }
  catch (...)
  {
    throw Fmi::Exception::Trace(BCP, "Operation failed!");
  }
}

#if 0
void add_value(const TS::TimedValue &tv,
               Json::Value &values_array,
               Json::Value &data_type,
               unsigned int values_index,
               unsigned int precision)
{
  try
  {
    const auto &val = tv.value;

    if (const auto* ptr = std::get_if<double>(&val))
    {
      if (values_index == 0)
        data_type = Json::Value("float");
      values_array[values_index] = Json::Value(*ptr, precision);
    }
    else if (const auto* ptr = std::get_if<int>(&val))
    {
      if (values_index == 0)
        data_type = Json::Value("int");
      values_array[values_index] = Json::Value(*ptr);
    }
    else if (const auto* ptr = std::get_if<std::string>(&val))
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
#endif

#if 0
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
#endif

Json::Value get_bbox(const std::vector<TS::LonLat> &coords, int lon_precision, int lat_precision)
{
  double min_lon = 0.0;
  double max_lon = 0.0;
  double min_lat = 0.0;
  double max_lat = 0.0;

  for (unsigned int i = 0; i < coords.size(); i++)
  {
    const auto &latlon = coords.at(i);
    if (i == 0)
    {
      min_lon = latlon.lon;
      min_lat = latlon.lat;
      max_lon = latlon.lon;
      max_lat = latlon.lat;
      continue;
    }
    if (latlon.lon < min_lon)
      min_lon = latlon.lon;
    if (latlon.lon > max_lon)
      max_lon = latlon.lon;

    if (latlon.lat < min_lat)
      min_lat = latlon.lat;
    if (latlon.lat > max_lat)
      max_lat = latlon.lat;
  }

  Json::Value bbox = Json::Value(Json::ValueType::arrayValue);
  bbox[0] = Json::Value(min_lon, lon_precision);
  bbox[1] = Json::Value(min_lat, lat_precision);
  bbox[2] = Json::Value(max_lon, lon_precision);
  bbox[3] = Json::Value(max_lat, lat_precision);

  return bbox;
}

Json::Value format_output_data_one_point(const TS::OutputData &outputData,
                                         const EDRMetaData &emd,
                                         std::optional<int> /* level */,
                                         const std::vector<Spine::Parameter> &query_parameters)
{
  try
  {
    // std::cout << "format_output_data_one_point" << std::endl;

    auto feature_collection = Json::Value(Json::ValueType::objectValue);
    auto features = Json::Value(Json::ValueType::arrayValue);

    feature_collection["type"] = Json::Value("FeatureCollection");
    feature_collection["parameters"] = get_edr_series_parameters(query_parameters, emd);
    auto coordinates = get_coordinates(outputData, query_parameters);
    const auto &lon_precision = emd.getPrecision("longitude");
    const auto &lat_precision = emd.getPrecision("latitude");
    feature_collection["bbox"] = get_bbox(coordinates, lon_precision, lat_precision);
    auto lonlat = coordinates.front();
    auto point_coordinates = Json::Value(Json::ValueType::arrayValue);
    point_coordinates[0] = Json::Value(lonlat.lon, lon_precision);
    point_coordinates[1] = Json::Value(lonlat.lat, lat_precision);
    auto point_geometry = Json::Value(Json::ValueType::objectValue);
    point_geometry["type"] = Json::Value("Point");
    point_geometry["coordinates"] = point_coordinates;

    for (const auto &tmp : outputData)
    {
      const auto &outdata = tmp.second;

      // iterate columns (parameters)
      for (unsigned int j = 0; j < outdata.size(); j++)
      {
        auto feature = Json::Value(Json::ValueType::objectValue);
        feature["type"] = Json::Value("Feature");
        auto parameter_name = query_parameters[j].name();
        const auto &parameter_precision = emd.getPrecision(parameter_name);
        boost::algorithm::to_lower(parameter_name);
        if (lon_lat_level_param(parameter_name))
          continue;

        auto tsdata = outdata.at(j);
        TS::TimeSeriesPtr ts = std::get<TS::TimeSeriesPtr>(tsdata);
        auto parameter_values = Json::Value(Json::ValueType::arrayValue);
        auto time_values = Json::Value(Json::ValueType::arrayValue);
        for (unsigned int k = 0; k < ts->size(); k++)
        {
          const auto &timed_value = ts->at(k);
          add_value(timed_value, parameter_values, k, parameter_precision);
          add_value(timed_value, time_values, k, parameter_precision);
          time_values[k] = Json::Value(
              Fmi::date_time::to_iso_extended_string(timed_value.time.utc_time()) + "Z");
        }
        auto properties = Json::Value(Json::ValueType::objectValue);
        parameter_name = parse_parameter_name(query_parameters[j].originalName());
        properties[parameter_name] = parameter_values;
        properties["time"] = time_values;
        feature["properties"] = properties;
        feature["geometry"] = point_geometry;
        features[features.size()] = feature;
      }
    }

    feature_collection["features"] = features;

    return feature_collection;
  }
  catch (...)
  {
    throw Fmi::Exception::Trace(BCP, "Operation failed!");
  }
}

void add_position_features(Json::Value &features,
                           const TS::TimeSeriesPtr &ts_data,
                           const TS::TimeSeriesPtr &ts_lon,
                           const TS::TimeSeriesPtr &ts_lat,
                           const TS::TimeSeriesPtr &ts_level,
                           const std::string &parameter_name,
                           int parameter_precision,
                           int lon_precision,
                           int lat_precision,
                           int level_precision)
{
  try
  {
    std::map<size_t, coordinate_xyz> coordinates;
    std::map<size_t, std::vector<Json::Value>> timesteps_per_coordinate;
    std::map<size_t, std::vector<Json::Value>> parameter_values_per_coordinate;
    for (unsigned int k = 0; k < ts_data->size(); k++)
    {
      auto lon_value = to_double(ts_lon->at(k));
      auto lat_value = to_double(ts_lat->at(k));
      std::optional<double> level_value;
      if (ts_level)
        level_value = to_int(ts_level->at(k));
      coordinate_xyz coord(lon_value, lat_value, level_value);
      auto key = hash_value(coord);
      auto timestep =
          Json::Value(Fmi::date_time::to_iso_extended_string(ts_data->at(k).time.utc_time()) + "Z");
      timesteps_per_coordinate[key].push_back(timestep);
      parameter_values_per_coordinate[key].push_back(
          UtilityFunctions::json_value(ts_data->at(k).value, parameter_precision));
      coordinates[key] = coord;
    }

    for (const auto &item : coordinates)
    {
      auto key = item.first;
      const auto &coord = item.second;
      auto feature = Json::Value(Json::ValueType::objectValue);
      feature["type"] = Json::Value("Feature");
      auto point_coordinates = Json::Value(Json::ValueType::arrayValue);
      point_coordinates[0] = Json::Value(coord.x, lon_precision);
      point_coordinates[1] = Json::Value(coord.y, lat_precision);
      if (coord.z)
        point_coordinates[2] = Json::Value(*coord.z, level_precision);
      auto point_geometry = Json::Value(Json::ValueType::objectValue);
      point_geometry["type"] = Json::Value("Point");
      point_geometry["coordinates"] = point_coordinates;
      feature["geometry"] = point_geometry;

      auto parameter_values = Json::Value(Json::ValueType::arrayValue);
      auto time_values = Json::Value(Json::ValueType::arrayValue);

      const auto &timesteps = timesteps_per_coordinate.at(key);
      const auto &values = parameter_values_per_coordinate.at(key);
      for (unsigned int k = 0; k < timesteps.size(); k++)
      {
        time_values[time_values.size()] = timesteps.at(k);
        parameter_values[parameter_values.size()] = values.at(k);
      }

      auto properties = Json::Value(Json::ValueType::objectValue);
      properties[parameter_name] = parameter_values;
      properties["time"] = time_values;
      feature["properties"] = properties;
      features[features.size()] = feature;
    }
  }
  catch (...)
  {
    throw Fmi::Exception::Trace(BCP, "Operation failed!");
  }
}

Json::Value format_output_data_position(const TS::OutputData &outputData,
                                        const EDRMetaData &emd,
                                        const std::vector<Spine::Parameter> &query_parameters)
{
  try
  {
    //	std::cout << "format_output_data_position" << std::endl;
    auto feature_collection = Json::Value(Json::ValueType::objectValue);
    auto features = Json::Value(Json::ValueType::arrayValue);

    feature_collection["type"] = Json::Value("FeatureCollection");
    feature_collection["parameters"] = get_edr_series_parameters(query_parameters, emd);

    const auto lon_precision = emd.getPrecision("longitude");
    const auto lat_precision = emd.getPrecision("latitude");
    auto level_precision = 0;
    unsigned int longitude_index = 0;
    unsigned int latitude_index = 0;
    std::optional<unsigned int> level_index;
    const auto &last_param = query_parameters.back();
    auto last_param_name = last_param.name();
    boost::algorithm::to_lower(last_param_name);
    if (last_param_name == "level")
    {
      level_index = query_parameters.size() - 1;
      latitude_index = query_parameters.size() - 2;
      longitude_index = query_parameters.size() - 3;
    }
    else
    {
      latitude_index = query_parameters.size() - 1;
      longitude_index = query_parameters.size() - 2;
    }

    for (const auto &output : outputData)
    {
      const auto &outdata = output.second;

      // iterate columns (parameters)
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

        add_position_features(features,
                              ts_data,
                              ts_lon,
                              ts_lat,
                              ts_level,
                              parse_parameter_name(query_parameters[j].originalName()),
                              parameter_precision,
                              lon_precision,
                              lat_precision,
                              level_precision);
      }
    }

    feature_collection["features"] = features;

    return feature_collection;
  }
  catch (...)
  {
    throw Fmi::Exception::Trace(BCP, "Operation failed!");
  }
}

DataPerLevel get_parameter_data(const TS::TimeSeriesGroupPtr &tsg_data,
                                const TS::TimeSeriesGroupPtr &tsg_lon,
                                const TS::TimeSeriesGroupPtr &tsg_lat,
                                const TS::TimeSeriesGroupPtr &tsg_level,
                                bool levels_present,
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

      for (unsigned int l = 0; l < llts_data.timeseries.size(); l++)
      {
        const auto &data_value = llts_data.timeseries.at(l);
        const auto &lon_value = llts_lon.timeseries.at(l);
        const auto &lat_value = llts_lat.timeseries.at(l);

        time_coord_value tcv;
        tcv.lon = as_double(lon_value.value);
        tcv.lat = as_double(lat_value.value);
        tcv.time = (Fmi::date_time::to_iso_extended_string(data_value.time.utc_time()) + "Z");

        double level = std::numeric_limits<double>::max();
        if (levels_present)
        {
          const auto &llts_level = tsg_level->at(k);
          const auto &level_value = llts_level.timeseries.at(l);
          if (const auto *ptr = std::get_if<double>(&level_value.value))
          {
            level = *ptr;
          }
          else if (const auto *ptr = std::get_if<int>(&level_value.value))
          {
            level = *ptr;
          }
        }

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
    return dpl;
  }
  catch (...)
  {
    throw Fmi::Exception::Trace(BCP, "Operation failed!");
  }
}

DataPerParameter get_data_per_parameter(const TS::OutputData &outputData,
                                        const EDRMetaData & /* emd */,
                                        const std::set<int> &levels,
                                        const CoordinateFilter &coordinate_filter,
                                        const std::vector<Spine::Parameter> &query_parameters,
                                        ParameterNames &dpn)
{
  try
  {
    DataPerParameter dpp;

    //	  std::cout << "get_output_data_per_parameter" << std::endl;

    bool levels_present = !levels.empty();

    // Get indexes of longitude, latitude, level
    unsigned int longitude_index = 0;
    unsigned int latitude_index = 0;
    unsigned int level_index = 0;
    const auto &last_param = query_parameters.back();
    auto last_param_name = last_param.name();
    boost::algorithm::to_lower(last_param_name);
    if (levels_present)
    {
      level_index = query_parameters.size() - 1;
      latitude_index = query_parameters.size() - 2;
      longitude_index = query_parameters.size() - 3;
    }
    else
    {
      latitude_index = query_parameters.size() - 1;
      longitude_index = query_parameters.size() - 2;
    }

    for (const auto &output : outputData)
    {
      const auto &outdata = output.second;

      // iterate columns (parameters)
      for (unsigned int j = 0; j < outdata.size(); j++)
      {
        auto parameter_name = parse_parameter_name(query_parameters[j].name());

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

        DataPerLevel dpl = get_parameter_data(
            tsg_data, tsg_lon, tsg_lat, tsg_level, levels_present, coordinate_filter);

        dpp[parameter_name] = dpl;
        dpn[parameter_name] = parse_parameter_name(query_parameters[j].originalName());
      }
    }
    return dpp;
  }
  catch (...)
  {
    throw Fmi::Exception::Trace(BCP, "Operation failed!");
  }
}

void add_collection_level_features(Json::Value &features,
                                   const std::vector<time_coord_value> &time_coord_values,
                                   const std::string &parameter_name,
                                   int parameter_precision,
                                   int lon_precision,
                                   int lat_precision,
                                   int level_precision,
                                   const std::optional<double> &level)
{
  try
  {
    std::map<size_t, coordinate_xyz> coordinates;
    std::map<size_t, std::vector<std::string>> timestamps_per_coordinate;
    std::map<size_t, std::vector<std::optional<TS::Value>>> values_per_coordinate;
    for (const auto &item3 : time_coord_values)
    {
      coordinate_xyz coord(item3.lon, item3.lat, level);
      auto key = hash_value(coord);
      timestamps_per_coordinate[key].push_back(item3.time);
      values_per_coordinate[key].push_back(item3.value);
      coordinates[key] = coord;
    }

    auto parameter_values = Json::Value(Json::ValueType::arrayValue);
    auto time_values = Json::Value(Json::ValueType::arrayValue);

    for (const auto &item : coordinates)
    {
      auto key = item.first;
      const auto &coord = item.second;
      const auto &timeseries = timestamps_per_coordinate.at(key);
      const auto &values = values_per_coordinate.at(key);

      for (unsigned int i = 0; i < timeseries.size(); i++)
      {
        time_values[i] = Json::Value(timeseries.at(i));
        Json::Value parameter_value;
        if (values.at(i))
          parameter_value = UtilityFunctions::json_value(*values.at(i), parameter_precision);
        parameter_values[i] = parameter_value;
      }
      auto feature = Json::Value(Json::ValueType::objectValue);
      feature["type"] = Json::Value("Feature");
      auto point_coordinates = Json::Value(Json::ValueType::arrayValue);
      point_coordinates[0] = Json::Value(coord.x, lon_precision);
      point_coordinates[1] = Json::Value(coord.y, lat_precision);
      if (coord.z)
        point_coordinates[2] = Json::Value(*coord.z, level_precision);
      auto point_geometry = Json::Value(Json::ValueType::objectValue);
      point_geometry["type"] = Json::Value("Point");
      point_geometry["coordinates"] = point_coordinates;
      feature["geometry"] = point_geometry;
      auto properties = Json::Value(Json::ValueType::objectValue);
      properties[parameter_name] = parameter_values;
      properties["time"] = time_values;
      feature["properties"] = properties;
      features[features.size()] = feature;
    }
  }
  catch (...)
  {
    throw Fmi::Exception::Trace(BCP, "Operation failed!");
  }
}

Json::Value format_output_data_feature_collection(
    const TS::OutputData &outputData,
    const EDRMetaData &emd,
    const std::set<int> &levels,
    const CoordinateFilter &coordinate_filter,
    const std::vector<Spine::Parameter> &query_parameters,
    EDRQueryType /* query_type */)

{
  try
  {
    if (outputData.empty())
      Json::Value();

    //	std::cout << "RESULT: " << outputData << std::endl;

    auto feature_collection = Json::Value(Json::ValueType::objectValue);

    feature_collection["type"] = Json::Value("FeatureCollection");
    feature_collection["parameters"] = get_edr_series_parameters(query_parameters, emd);
    const auto &lon_precision = emd.getPrecision("longitude");
    const auto &lat_precision = emd.getPrecision("latitude");
    auto level_precision = 0;
    double bbox_xmin = 180.0;
    double bbox_ymin = 90.0;
    double bbox_xmax = -180.0;
    double bbox_ymax = -90.0;

    ParameterNames dpn;
    auto dpp =
        get_data_per_parameter(outputData, emd, levels, coordinate_filter, query_parameters, dpn);

    auto features = Json::Value(Json::ValueType::arrayValue);
    auto output_name = dpn.cbegin();
    for (const auto &item : dpp)
    {
      const auto &parameter_name = item.first;
      const auto &level_values = item.second;
      const auto parameter_precision = emd.getPrecision(parameter_name);
      for (const auto &item2 : level_values)
      {
        std::optional<double> level;
        if (item2.first != std::numeric_limits<double>::max())
          level = item2.first;
        const auto &time_coord_values = item2.second;

        for (const auto &item3 : time_coord_values)
        {
          coordinate_xyz coord(item3.lon, item3.lat, level);
          bbox_xmin = std::min(bbox_xmin, coord.x);
          bbox_xmax = std::max(bbox_xmax, coord.x);
          bbox_ymin = std::min(bbox_ymin, coord.y);
          bbox_ymax = std::max(bbox_ymax, coord.y);
        }

        add_collection_level_features(features,
                                      time_coord_values,
                                      output_name->second,
                                      parameter_precision,
                                      lon_precision,
                                      lat_precision,
                                      level_precision,
                                      level);
      }
      output_name++;
    }

    if (features.size() > 0)
    {
      Json::Value bbox = Json::Value(Json::ValueType::arrayValue);
      bbox[0] = Json::Value(bbox_xmin, lon_precision);
      bbox[1] = Json::Value(bbox_ymin, lat_precision);
      bbox[2] = Json::Value(bbox_xmax, lon_precision);
      bbox[3] = Json::Value(bbox_ymax, lat_precision);
      feature_collection["bbox"] = bbox;
    }
    feature_collection["features"] = features;

    return feature_collection;
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
                             const std::vector<Spine::Parameter> &query_parameters)
{
  try
  {
    Json::Value empty_result;

    if (outputData.empty())
      return empty_result;

    const auto &outdata_first = outputData.at(0).second;

    if (outdata_first.empty())
      return empty_result;

    empty_result["type"] = Json::Value("FeatureCollection");
    auto features_array = Json::Value(Json::ValueType::arrayValue);
    auto empty_feature = Json::Value(Json::ValueType::objectValue);
    auto empty_geometry = Json::Value(Json::ValueType::objectValue);
    auto geometry_coordinates = Json::Value(Json::ValueType::arrayValue);
    geometry_coordinates[0] = Json::Value(0.0);
    geometry_coordinates[1] = Json::Value(0.0);
    empty_geometry["type"] = Json::Value("Point");
    empty_geometry["coordinates"] = geometry_coordinates;
    empty_feature["type"] = Json::Value("Feature");
    empty_feature["geometry"] = empty_geometry;
    empty_feature["properties"] = Json::Value(Json::ValueType::objectValue);

    features_array[0] = empty_feature;
    empty_result["features"] = features_array;

    const auto &tsdata_first = outdata_first.at(0);

    if (std::get_if<TS::TimeSeriesPtr>(&tsdata_first))
    {
      // Zero or one levels
      if (levels.size() <= 1)
      {
        std::optional<int> level;
        if (levels.size() == 1)
          level = *(levels.begin());
        return format_output_data_one_point(outputData, emd, level, query_parameters);
      }

      // More than one level
      return format_output_data_position(outputData, emd, query_parameters);
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
      od.push_back(std::make_pair("_obs_", tsd));
      // Zero or one levels
      if (levels.size() <= 1)
      {
        std::optional<int> level;
        if (levels.size() == 1)
          level = *(levels.begin());
        return format_output_data_one_point(od, emd, level, query_parameters);
      }
      // More than one level
      return format_output_data_position(od, emd, query_parameters);
    }

    if (std::get_if<TS::TimeSeriesGroupPtr>(&tsdata_first))
    {
      return format_output_data_feature_collection(
          outputData, emd, levels, coordinate_filter, query_parameters, query_type);
    }

    return empty_result;
  }
  catch (...)
  {
    throw Fmi::Exception::Trace(BCP, "Operation failed!");
  }
}

}  // namespace GeoJson
}  // namespace EDR
}  // namespace Plugin
}  // namespace SmartMet
