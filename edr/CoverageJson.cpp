#include "CoverageJson.h"
#include "UtilityFunctions.h"
#include <boost/optional.hpp>
#include <macgyver/Exception.h>
#include <macgyver/StringConversion.h>

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
  boost::optional<TS::Value> value;
};

using DataPerLevel = std::map<double, std::vector<time_coord_value>>;  // level -> array of values
using DataPerParameter = std::map<std::string, DataPerLevel>;          // parameter_name -> data

double as_double(const TS::Value &value)
{
  return (boost::get<double>(&value) != nullptr ? *(boost::get<double>(&value))
                                                : Fmi::stod(*(boost::get<std::string>(&value))));
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
  if(temporal_extent.time_periods.empty())
	{
	  auto temporal_interval = Json::Value(Json::ValueType::arrayValue);
	  temporal_interval[0] = nullvalue;
	  temporal_interval[1] = nullvalue;
	  temporal["interval"] = temporal_interval;
	}
  else
	{
	  if(temporal_extent.time_periods.size() == 1)
		{
		  const auto& temporal_extent_period = temporal_extent.time_periods.at(0);
		  
		  // Show only period with starttime and endtime (probably there is too many timesteps in source data)
		  if(temporal_extent_period.timestep == 0)
			{
			  /*
			  auto temporal_interval = Json::Value(Json::ValueType::arrayValue);
			  temporal_interval[0] = Json::Value(Fmi::to_iso_extended_string(temporal_extent_period.start_time)+"Z/"+Fmi::to_iso_extended_string(temporal_extent_period.end_time)+"Z");
			  temporal["interval"] = temporal_interval;
			 */
			  /*
			  auto temporal_interval = Json::Value(Json::ValueType::arrayValue);
			  auto hours = ((temporal_extent_period.end_time-temporal_extent_period.start_time).total_seconds()/3600);
			  temporal_interval[0] = Json::Value("R"+Fmi::to_string(hours)+"/"+Fmi::to_iso_extended_string(temporal_extent_period.start_time)+"Z/PT60M");			  
			  temporal["interval"] = temporal_interval;
*/
			  auto hours = ((temporal_extent_period.end_time-temporal_extent_period.start_time).total_seconds()/3600);
			  auto temporal_interval = Json::Value(Json::ValueType::arrayValue);
			  auto temporal_interval_values = Json::Value(Json::ValueType::arrayValue);
			  auto temporal_interval_array = Json::Value(Json::ValueType::arrayValue);
			  temporal_interval_array[0] = Json::Value(Fmi::to_iso_extended_string(temporal_extent_period.start_time) + "Z");
			  temporal_interval_array[1] = Json::Value(Fmi::to_iso_extended_string(temporal_extent_period.end_time) + "Z");
			  temporal_interval[0] = temporal_interval_array;
			  temporal_interval_values[0] =
				Json::Value("R" + Fmi::to_string(hours) + "/" +
							Fmi::to_iso_extended_string(temporal_extent_period.start_time) + "Z/PT60M");
			  temporal["interval"] = temporal_interval;
			  temporal["values"] = temporal_interval_values;

			}
		  else
			{
			  // Time period which contains timesteps with constant length
			  auto temporal_interval = Json::Value(Json::ValueType::arrayValue);
			  auto temporal_interval_values = Json::Value(Json::ValueType::arrayValue);
			  auto temporal_interval_array = Json::Value(Json::ValueType::arrayValue);
			  temporal_interval_array[0] = Json::Value(Fmi::to_iso_extended_string(temporal_extent_period.start_time) + "Z");
			  temporal_interval_array[1] = Json::Value(Fmi::to_iso_extended_string(temporal_extent_period.end_time) + "Z");
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
		  const auto& first_temporal_extent_period = temporal_extent.time_periods.at(0);
		  // If timestep is 0, time_periods member contains just start_time, meaning there is only separate timesteps
		  if(first_temporal_extent_period.timestep == 0)
			{
			  auto temporal_interval_array = Json::Value(Json::ValueType::arrayValue);
			  for(const auto& period : temporal_extent.time_periods)
				temporal_interval_array[temporal_interval_array.size()] = Json::Value(Fmi::to_iso_extended_string(period.start_time)+"Z");
			  temporal["interval"] = temporal_interval_array;
			}
		  else
			{
			  // Several time periods, time periods may or may not have same time step length
			  auto temporal_interval = Json::Value(Json::ValueType::arrayValue);
			  auto temporal_interval_values = Json::Value(Json::ValueType::arrayValue);
			  auto temporal_interval_array = Json::Value(Json::ValueType::arrayValue);
			  for(unsigned int i = 0; i < temporal_extent.time_periods.size(); i++)
				{
				  const auto& temporal_extent_period = temporal_extent.time_periods.at(i);
				  temporal_interval_array[i] = Json::Value(Fmi::to_iso_extended_string(temporal_extent_period.start_time)+"Z/" +Fmi::to_iso_extended_string(temporal_extent_period.end_time)+"Z");
				  temporal_interval_values[i] =
					Json::Value("R" + Fmi::to_string(temporal_extent_period.timesteps) + "/" +
								Fmi::to_iso_extended_string(temporal_extent_period.start_time) + "Z/PT" +
								Fmi::to_string(temporal_extent_period.timestep) + "M");

				}
			  temporal_interval[0] = temporal_interval_array;
			  temporal["interval"] = temporal_interval;
			  temporal["values"] = temporal_interval_values;						  
			}
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
      query_info_variables["minz"] = Json::Value("Minimum level to return data for, for example 850");
      query_info_variables["maxz"] = Json::Value("Maximum level to return data for, for example 300");

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
    query_info_variables["default_output_format"] = Json::Value("CoverageJSON");

    auto query_info_crs_details = Json::Value(Json::ValueType::arrayValue);
    auto query_info_crs_details_0 = Json::Value(Json::ValueType::objectValue);
	//    query_info_crs_details_0["crs"] = Json::Value("EPSG:4326");
    query_info_crs_details_0["crs"] = Json::Value("CRS:84");
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
    query_info_variables["default_output_format"] = Json::Value("CoverageJSON");

    auto query_info_crs_details = Json::Value(Json::ValueType::arrayValue);
    auto query_info_crs_details_0 = Json::Value(Json::ValueType::objectValue);
	//    query_info_crs_details_0["crs"] = Json::Value("EPSG:4326");
    query_info_crs_details_0["crs"] = Json::Value("CRS:84");
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

    for (unsigned int i = 0; i < longitude_vector.size(); i++)
      ret.emplace_back(TS::LonLat(longitude_vector.at(i), latitude_vector.at(i)));

    return ret;
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
    if (boost::get<TS::TimeSeriesVectorPtr>(&outdata_front))
    {
      TS::TimeSeriesVectorPtr tsv = *(boost::get<TS::TimeSeriesVectorPtr>(&outdata_front));
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
      if (boost::get<TS::TimeSeriesPtr>(&tsdata))
      {
        TS::TimeSeriesPtr ts = *(boost::get<TS::TimeSeriesPtr>(&tsdata));
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
      else if (boost::get<TS::TimeSeriesVectorPtr>(&tsdata))
      {
        std::cout << "get_coordinates -> TS::TimeSeriesVectorPtr - Shouldnt be "
                     "here -> report error!!:\n"
                  << tsdata << std::endl;
      }
      else if (boost::get<TS::TimeSeriesGroupPtr>(&tsdata))
      {
        TS::TimeSeriesGroupPtr tsg = *(boost::get<TS::TimeSeriesGroupPtr>(&tsdata));

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
    }

    if (longitude_vector.size() != latitude_vector.size())
      throw Fmi::Exception(
          BCP, "Something wrong: latitude_vector.size() != longitude_vector.size()!", nullptr);

    for (unsigned int i = 0; i < longitude_vector.size(); i++)
      ret.emplace_back(TS::LonLat(longitude_vector.at(i), latitude_vector.at(i)));

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
    const auto &engine_parameter_info = metadata.parameters;
    const auto &config_parameter_info = *metadata.parameter_info;

    auto parameters = Json::Value(Json::ValueType::objectValue);

    for (const auto &p : query_parameters)
    {
      auto parameter_name = parse_parameter_name(p.name());

      if (lon_lat_level_param(parameter_name))
        continue;

      const auto &edr_parameter = engine_parameter_info.at(parameter_name);

      auto pinfo = config_parameter_info.get_parameter_info(parameter_name, metadata.language);
      auto description = (!pinfo.description.empty() ? pinfo.description : "");
      auto label = (!pinfo.unit_label.empty() ? pinfo.unit_label : edr_parameter.name);
      auto symbol = (!pinfo.unit_symbol_value.empty() ? pinfo.unit_symbol_value : "");
      auto symbol_type = (!pinfo.unit_symbol_type.empty() ? pinfo.unit_symbol_type : "");

      auto parameter = Json::Value(Json::ValueType::objectValue);
      parameter["type"] = Json::Value("Parameter");
      // Description field is optional
      // QEngine returns parameter description in finnish and skandinavian
      // characters cause problems metoffice test interface uses description
      // field -> set parameter name to description field
      parameter["description"] = Json::Value(Json::ValueType::objectValue);
      parameter["description"][metadata.language] = Json::Value(parameter_name);
      parameter["unit"] = Json::Value(Json::ValueType::objectValue);
      parameter["unit"]["label"] = Json::Value(Json::ValueType::objectValue);
      parameter["unit"]["label"][metadata.language] = Json::Value(label);
      parameter["unit"]["symbol"] = Json::Value(Json::ValueType::objectValue);
      parameter["unit"]["symbol"]["value"] = Json::Value(symbol);
      parameter["unit"]["symbol"]["type"] = Json::Value(symbol_type);
      parameter["observedProperty"] = Json::Value(Json::ValueType::objectValue);
      parameter["observedProperty"]["id"] = Json::Value(edr_parameter.name);
      parameter["observedProperty"]["label"] = Json::Value(Json::ValueType::objectValue);
      parameter["observedProperty"]["label"][metadata.language] = Json::Value(edr_parameter.name);
      parameters[parameter_name] = parameter;
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

    if (boost::get<double>(&val) != nullptr)
    {
      if (values_index == 0)
        data_type = Json::Value("float");
      values_array[values_index] = Json::Value(*(boost::get<double>(&val)), precision);
    }
    else if (boost::get<int>(&val) != nullptr)
    {
      if (values_index == 0)
        data_type = Json::Value("int");
      values_array[values_index] = Json::Value(*(boost::get<int>(&val)));
    }
    else if (boost::get<std::string>(&val) != nullptr)
    {
      if (values_index == 0)
        data_type = Json::Value("string");
      values_array[values_index] = Json::Value(*(boost::get<std::string>(&val)));
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
    timesteps.insert(boost::posix_time::to_iso_extended_string(t.utc_time()) + "Z");
    add_value(tv, values_array, data_type, values_index, precision);
  }
  catch (...)
  {
    throw Fmi::Exception::Trace(BCP, "Operation failed!");
  }
}

Json::Value add_prologue_one_point(boost::optional<int> level,
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
    referencing_xy["system"]["id"] = Json::Value("http://www.opengis.net/def/crs/EPSG/0/4326");

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
    referencing_time["system"]["type"] = Json::Value("TemporalCRS");
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

Json::Value add_prologue_multi_point(boost::optional<int> level,
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
    referencing_xy["system"]["id"] = Json::Value("http://www.opengis.net/def/crs/EPSG/0/4326");

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
    referencing_time["system"]["type"] = Json::Value("TemporalCRS");
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
                                             const std::string &domain_type)
{
  try
  {
    Json::Value coverage_collection;

    coverage_collection["type"] = Json::Value("CoverageCollection");
    coverage_collection["parameters"] = get_edr_series_parameters(query_parameters, emd);

    auto referencing = Json::Value(Json::ValueType::arrayValue);
    auto referencing_xy = Json::Value(Json::ValueType::objectValue);
    referencing_xy["coordinates"] = Json::Value(Json::ValueType::arrayValue);
    referencing_xy["coordinates"][0] = Json::Value("y");
    referencing_xy["coordinates"][1] = Json::Value("x");
    referencing_xy["system"] = Json::Value(Json::ValueType::objectValue);
    referencing_xy["system"]["type"] = Json::Value("GeographicCRS");
    referencing_xy["system"]["id"] = Json::Value("http://www.opengis.net/def/crs/EPSG/0/4326");

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
    referencing_time["system"]["type"] = Json::Value("TemporalCRS");
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

#if 0
Json::Value add_prologue_coverage_collection(boost::optional<int> level,
                                             const std::string &level_type,
                                             const std::vector<TS::LonLat> & /* coordinates */)
{
  try
  {
    Json::Value coverage_collection;

    coverage_collection["type"] = Json::Value("CoverageCollection");

    auto referencing = Json::Value(Json::ValueType::arrayValue);
    auto referencing_xy = Json::Value(Json::ValueType::objectValue);
    referencing_xy["coordinates"] = Json::Value(Json::ValueType::arrayValue);
    referencing_xy["coordinates"][0] = Json::Value("y");
    referencing_xy["coordinates"][1] = Json::Value("x");
    referencing_xy["system"] = Json::Value(Json::ValueType::objectValue);
    referencing_xy["system"]["type"] = Json::Value("GeographicCRS");
    referencing_xy["system"]["id"] = Json::Value("http://www.opengis.net/def/crs/EPSG/0/4326");

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
    referencing_time["system"]["type"] = Json::Value("TemporalCRS");
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

    coverage_collection["referencing"] = referencing;
    coverage_collection["domainType"] = Json::Value("Point");

    return coverage_collection;
  }
  catch (...)
  {
    throw Fmi::Exception::Trace(BCP, "Operation failed!");
  }
}
#endif

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
Json::Value parse_edr_metadata_instances(const EDRProducerMetaData &epmd, const EDRQuery &edr_query)
{
  try
  {
    //	  std::cout << "parse_edr_metadata_instances: " << edr_query <<
    // std::endl;
    Json::Value nulljson;

    if (epmd.empty())
      return nulljson;

    EDRProducerMetaData requested_epmd;
    if (!edr_query.collection_id.empty())
    {
      if (epmd.find(edr_query.collection_id) != epmd.end())
        requested_epmd[edr_query.collection_id] = epmd.at(edr_query.collection_id);
    }
    else
      requested_epmd = epmd;

    auto result = Json::Value(Json::ValueType::objectValue);

    const auto &emds = requested_epmd.begin()->second;
    bool instances_exist = !emds.empty();

    if (!instances_exist)
      return reportError(400,
                         ("Collection '" + edr_query.collection_id + "' does not have instances!"));

    const auto &producer = requested_epmd.begin()->first;

    auto links = Json::Value(Json::ValueType::arrayValue);
    auto link_item = Json::Value(Json::ValueType::objectValue);
    link_item["href"] = Json::Value((edr_query.host + "/collections/" + producer + "/instances"));
    link_item["rel"] = Json::Value("self");
    link_item["type"] = Json::Value("application/json");
    links[0] = link_item;

    result["links"] = links;

    auto instances = Json::Value(Json::ValueType::arrayValue);

    unsigned int instance_index = 0;
    for (const auto &emd : emds)
    {
      auto instance_id = Fmi::to_iso_string(emd.temporal_extent.origin_time);

      if (edr_query.query_id == EDRQueryId::SpecifiedCollectionSpecifiedInstance &&
          instance_id != edr_query.instance_id)
        continue;

	  if(emd.parameter_names.empty())
		continue;

      auto instance = Json::Value(Json::ValueType::objectValue);
      instance["id"] = Json::Value(instance_id);
	  if(!emd.temporal_extent.time_periods.empty())
		{
		  std::string title =
			("Origintime: " +
			 boost::posix_time::to_iso_extended_string(emd.temporal_extent.origin_time) + "Z");
		  title += (" Starttime: " +
					boost::posix_time::to_iso_extended_string(emd.temporal_extent.time_periods.front().start_time) + "Z");
		  title +=
			(" Endtime: " + boost::posix_time::to_iso_extended_string(emd.temporal_extent.time_periods.front().end_time) +
			 "Z");
		  if (emd.temporal_extent.time_periods.front().timestep)
			title += (" Timestep: " + Fmi::to_string(emd.temporal_extent.time_periods.front().timestep));
		  instance["title"] = Json::Value(title);
		}
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
      spatial["bbox"] = bbox;
      // CRS (mandatory)
	  //      spatial["crs"] = Json::Value("EPSG:4326");
      spatial["crs"] = Json::Value("CRS:84");
      extent["spatial"] = spatial;
      // Temporal (optional)
      extent["temporal"] = parse_temporal_extent(emd.temporal_extent);
      // Vertical (optional)
      if (!emd.vertical_extent.levels.empty())
      {
        auto vertical = Json::Value(Json::ValueType::objectValue);
        auto vertical_interval = Json::Value(Json::ValueType::arrayValue);
        auto vertical_interval_values = Json::Value(Json::ValueType::arrayValue);
        for (unsigned int i = 0; i < emd.vertical_extent.levels.size(); i++)
          vertical_interval_values[i] = emd.vertical_extent.levels.at(i);
        vertical_interval[0] = emd.vertical_extent.levels.front();
        vertical_interval[1] = emd.vertical_extent.levels.back();
        vertical["interval"] = vertical_interval;
        vertical["values"] = vertical_interval_values;
        vertical["vrs"] = emd.vertical_extent.vrs;
        extent["vertical"] = vertical;
      }
      instance["extent"] = extent;
      // Optional: data_queries
      instance["data_queries"] = get_data_queries(edr_query.host,
                                                  producer,
                                                  emd.data_queries,
                                                  emd.output_formats,
                                                  !emd.vertical_extent.levels.empty(),
                                                  false,
                                                  instance_id);
	  // Optional: crs
	  auto crs = Json::Value(Json::ValueType::arrayValue);
	  //	  crs[0] = Json::Value("EPSG:4326");
	  crs[0] = Json::Value("CRS:84");
	  instance["crs"] = crs;

      // Parameter names (mandatory)
      auto parameter_names = Json::Value(Json::ValueType::objectValue);

      // Parameters:
      // Mandatory fields: id, type, observedProperty
      // Optional fields: label, description, data-type, unit, extent,
      // measurementType
      for (const auto &name : emd.parameter_names)
      {
		if(name.empty())
		  continue;
        auto pinfo = emd.parameter_info->get_parameter_info(name, emd.language);
        const auto &p = emd.parameters.at(name);
        auto param = Json::Value(Json::ValueType::objectValue);
        param["id"] = Json::Value(p.name);
        param["type"] = Json::Value("Parameter");
        // Set parameter name to description field
        param["description"] = Json::Value(!pinfo.description.empty() ? pinfo.description : p.name);
        if (!pinfo.unit_label.empty() || !pinfo.unit_symbol_value.empty() ||
            !pinfo.unit_symbol_type.empty())
        {
          auto unit = Json::Value(Json::ValueType::objectValue);
          unit["label"] = pinfo.unit_label;
          auto unit_symbol = Json::Value(Json::ValueType::objectValue);
          unit_symbol["value"] = pinfo.unit_symbol_value;
          unit_symbol["type"] = pinfo.unit_symbol_type;
          unit["symbol"] = unit_symbol;
          param["unit"] = unit;
        }

        // Observed property: Mandatory: label, Optional: id, description
        auto observedProperty = Json::Value(Json::ValueType::objectValue);
        observedProperty["label"] = Json::Value(p.name);
        //		  observedProperty["id"] = Json::Value("http://....");
        // observedProperty["description"] =
        // Json::Value("Description of property...");
        param["observedProperty"] = observedProperty;
        parameter_names[p.name] = param;
      }


      instance["parameter_names"] = parameter_names;

      instances[instance_index] = instance;
      instance_index++;
    }

    result["instances"] = instances;

    return result;
  }
  catch (...)
  {
    throw Fmi::Exception::Trace(BCP, "Operation failed!");
  }
}

Json::Value parse_edr_metadata_collections(const EDRProducerMetaData &epmd,
                                           const EDRQuery &edr_query)
{
  try
  {
    //	  std::cout << "parse_edr_metadata_collections: " << edr_query <<
    // std::endl;

    EDRProducerMetaData requested_epmd;
    if (!edr_query.collection_id.empty())
    {
      if (epmd.find(edr_query.collection_id) != epmd.end())
        requested_epmd[edr_query.collection_id] = epmd.at(edr_query.collection_id);
    }
    else
      requested_epmd = epmd;

    auto collections = Json::Value(Json::ValueType::arrayValue);
    Json::Value nulljson;
    unsigned int collection_index = 0;

    // Iterate metadata and add item into collection
    for (const auto &item : requested_epmd)
    {
      const auto &producer = item.first;
      EDRMetaData collection_emd;
      const auto &emds = item.second;
      // Get the most recent instance
      for (unsigned int i = 0; i < emds.size(); i++)
      {
        if (i == 0 ||
            collection_emd.temporal_extent.origin_time < emds.at(i).temporal_extent.origin_time)
          collection_emd = emds.at(i);
      }
	  if(collection_emd.parameter_names.empty())
		continue;

      bool instances_exist = (emds.size() > 1);

      auto value = Json::Value(Json::ValueType::objectValue);
      // Producer is Id
      value["id"] = Json::Value(producer);
	  // Collection info, first from engine
	  std::string title = collection_emd.collection_info_engine.title;
	  std::string description = collection_emd.collection_info_engine.description;
	  std::set<std::string> keyword_set = collection_emd.collection_info_engine.keywords;

	  // Then ge5t collection info from configuration file, if we didn't get it from engine
	  if(collection_emd.collection_info)
		{
		  // Title, description, keywords (optional)
		  if(title.empty())
			title = collection_emd.collection_info->title;
		  if(description.empty())
			description = collection_emd.collection_info->description;
		  if(keyword_set.empty())
			keyword_set = collection_emd.collection_info->keywords;
		}

	  value["title"] = Json::Value(title);
	  value["description"] = Json::Value(description);
      auto keywords = Json::Value(Json::ValueType::arrayValue);
	  for (const auto &kword : keyword_set)
		keywords[keywords.size()] = kword;

	  // Add parameter names into keywords
      for (const auto &name : collection_emd.parameter_names)
		keywords[keywords.size()] = name;
	  if(keywords.size() > 0)
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
      spatial["bbox"] = bbox;
      // CRS (mandatory)
	  //      spatial["crs"] = Json::Value("EPSG:4326");
      spatial["crs"] = Json::Value("CRS:84");
      extent["spatial"] = spatial;
      // Temporal (optional)
      extent["temporal"] = parse_temporal_extent(collection_emd.temporal_extent);
      // Vertical (optional)
      if (!collection_emd.vertical_extent.levels.empty())
      {
        auto vertical = Json::Value(Json::ValueType::objectValue);
        auto vertical_interval = Json::Value(Json::ValueType::arrayValue);
        auto vertical_interval_values = Json::Value(Json::ValueType::arrayValue);
        for (unsigned int i = 0; i < collection_emd.vertical_extent.levels.size(); i++)
          vertical_interval_values[i] = collection_emd.vertical_extent.levels.at(i);
        vertical_interval[0] = collection_emd.vertical_extent.levels.front();
        vertical_interval[1] = collection_emd.vertical_extent.levels.back();
        vertical["interval"] = vertical_interval;
        vertical["values"] = vertical_interval_values;
        vertical["vrs"] = collection_emd.vertical_extent.vrs;
        extent["vertical"] = vertical;
      }

      value["extent"] = extent;
      // Optional: data_queries
      value["data_queries"] = get_data_queries(edr_query.host,
                                               producer,
                                               collection_emd.data_queries,
                                               collection_emd.output_formats,
                                               !collection_emd.vertical_extent.levels.empty(),
                                               instances_exist);

	  // Optional: crs
	  auto crs = Json::Value(Json::ValueType::arrayValue);
	  //	  crs[0] = Json::Value("EPSG:4326");
	  crs[0] = Json::Value("CRS:84");
	  value["crs"] = crs;

      // Parameter names (mandatory)
      auto parameter_names = Json::Value(Json::ValueType::objectValue);

      // Parameters:
      // Mandatory fields: id, type, observedProperty
      // Optional fields: label, description, data-type, unit, extent,
      // measurementType
      for (const auto &name : collection_emd.parameter_names)
      {
		if(name.empty())
		  continue;

        auto pinfo =
            collection_emd.parameter_info->get_parameter_info(name, collection_emd.language);
        const auto &p = collection_emd.parameters.at(name);
        auto param = Json::Value(Json::ValueType::objectValue);
        param["id"] = Json::Value(p.name);
        param["type"] = Json::Value("Parameter");
        // Set parameter name to description field
        param["description"] = Json::Value(!pinfo.description.empty() ? pinfo.description : p.name);
        if (!pinfo.unit_label.empty() || !pinfo.unit_symbol_value.empty() ||
            !pinfo.unit_symbol_type.empty())
        {
          auto unit = Json::Value(Json::ValueType::objectValue);
          unit["label"] = pinfo.unit_label;
          auto unit_symbol = Json::Value(Json::ValueType::objectValue);
          unit_symbol["value"] = pinfo.unit_symbol_value;
          unit_symbol["type"] = pinfo.unit_symbol_type;
          unit["symbol"] = unit_symbol;
          param["unit"] = unit;
        }

        // Observed property: Mandatory: label, Optional: id, description
        auto observedProperty = Json::Value(Json::ValueType::objectValue);
        observedProperty["label"] = Json::Value(p.name);
        //		  observedProperty["id"] = Json::Value("http://....");
        //		  observedProperty["description"] =
        // Json::Value("Description of property...");
        param["observedProperty"] = observedProperty;
        parameter_names[p.name] = param;
      }

      value["parameter_names"] = parameter_names;
      auto output_formats = Json::Value(Json::ValueType::arrayValue);
      unsigned int i = 0;
      for (const auto &f : collection_emd.output_formats)
        output_formats[i++] = Json::Value(f);
      value["output_formats"] = output_formats;

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

Json::Value parse_edr_metadata(const EDRProducerMetaData &epmd, const EDRQuery &edr_query)
{
  //  std::cout << "parse_edr_metadata: " << edr_query << std::endl;

  Json::Value nulljson;

  if (edr_query.query_id == EDRQueryId::AllCollections ||
      edr_query.query_id == EDRQueryId::SpecifiedCollection)
    return parse_edr_metadata_collections(epmd, edr_query);

  if (edr_query.query_id == EDRQueryId::SpecifiedCollectionAllInstances ||
      edr_query.query_id == EDRQueryId::SpecifiedCollectionSpecifiedInstance)
    return parse_edr_metadata_instances(epmd, edr_query);

  return nulljson;
}

Json::Value format_output_data_one_point(const TS::OutputData &outputData,
                                         const EDRMetaData &emd,
                                         boost::optional<int> level,
                                         const std::vector<Spine::Parameter> &query_parameters)
{
  try
  {
    // std::cout << "format_output_data_one_point" << std::endl;

    Json::Value coverage;

    const auto &longitude_precision = emd.getPrecision("longitude");
    const auto &latitude_precision = emd.getPrecision("latitude");
    auto ranges = Json::Value(Json::ValueType::objectValue);
    auto coordinates = get_coordinates(outputData, query_parameters);
    std::set<std::string> timesteps;
    for (unsigned int i = 0; i < outputData.size(); i++)
    {
      const auto &outdata = outputData[i].second;
      // iterate columns (parameters)
      for (unsigned int j = 0; j < outdata.size(); j++)
      {
        auto parameter_name = query_parameters[j].name();
        const auto &parameter_precision = emd.getPrecision(parameter_name);
        boost::algorithm::to_lower(parameter_name);
        if (lon_lat_level_param(parameter_name))
          continue;

        auto json_param_object = Json::Value(Json::ValueType::objectValue);
        auto tsdata = outdata.at(j);
        auto values = Json::Value(Json::ValueType::arrayValue);
        unsigned int values_index = 0;

        TS::TimeSeriesPtr ts = *(boost::get<TS::TimeSeriesPtr>(&tsdata));
        if (i == 0)
        {
          coverage = add_prologue_one_point(level,
                                            emd.vertical_extent.level_type,
                                            coordinates.front().lon,
                                            coordinates.front().lat,
                                            longitude_precision,
                                            latitude_precision);
          coverage["parameters"] = get_edr_series_parameters(query_parameters, emd);
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

        for (unsigned int k = 0; k < ts->size(); k++)
        {
          const auto &timed_value = ts->at(k);
          add_value(timed_value,
                    values,
                    json_param_object["dataType"],
                    timesteps,
                    values_index,
                    parameter_precision);
          values_index++;
        }
        json_param_object["values"] = values;
        ranges[parameter_name] = json_param_object;
      }
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

Json::Value format_output_data_position(const TS::OutputData &outputData,
                                        const EDRMetaData &emd,
                                        const std::vector<Spine::Parameter> &query_parameters)
{
  try
  {
    //	std::cout << "format_output_data_position" << std::endl;

    Json::Value coverage_collection;

    const auto &longitude_precision = emd.getPrecision("longitude");
    const auto &latitude_precision = emd.getPrecision("latitude");
    auto level_precision = 0;
    unsigned int longitude_index;
    unsigned int latitude_index;
    boost::optional<unsigned int> level_index;
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
    coverage_collection["parameters"] = get_edr_series_parameters(query_parameters, emd);

    // Referencing x,y coordinates
    auto referencing = Json::Value(Json::ValueType::arrayValue);
    auto referencing_xy = Json::Value(Json::ValueType::objectValue);
    referencing_xy["coordinates"] = Json::Value(Json::ValueType::arrayValue);
    referencing_xy["coordinates"][0] = Json::Value("y");
    referencing_xy["coordinates"][1] = Json::Value("x");
    referencing_xy["system"] = Json::Value(Json::ValueType::objectValue);
    referencing_xy["system"]["type"] = Json::Value("GeographicCRS");
    referencing_xy["system"]["id"] = Json::Value("http://www.opengis.net/def/crs/EPSG/0/4326");

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
    referencing_time["system"]["type"] = Json::Value("TemporalCRS");
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
    //    std::set<std::string> timesteps;

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
        TS::TimeSeriesPtr ts_data = *(boost::get<TS::TimeSeriesPtr>(&tsdata));
        TS::TimeSeriesPtr ts_lon = *(boost::get<TS::TimeSeriesPtr>(&tslon));
        TS::TimeSeriesPtr ts_lat = *(boost::get<TS::TimeSeriesPtr>(&tslat));
        TS::TimeSeriesPtr ts_level = nullptr;
        if (level_index)
        {
          auto tslevel = outdata.at(*level_index);
          ts_level = *(boost::get<TS::TimeSeriesPtr>(&tslevel));
        }

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
          timestamp_values[values_index] = Json::Value(
              boost::posix_time::to_iso_extended_string(lon_value.time.utc_time()) + "Z");

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
    }

    coverage_collection["coverages"] = coverages;

    return coverage_collection;
  }
  catch (...)
  {
    throw Fmi::Exception::Trace(BCP, "Operation failed!");
  }
}

#if 0
Json::Value format_output_data_multi_point(const TS::OutputData &outputData,
                                           const EDRMetaData &emd,
                                           boost::optional<int> level,
                                           const std::vector<Spine::Parameter> &query_parameters)
{
  try
  {
    Json::Value coverage;

    //	  std::cout << "format_output_data_multi_point" << std::endl;

    auto ranges = Json::Value(Json::ValueType::objectValue);
    auto coordinates = get_coordinates(outputData, query_parameters);
    std::set<std::string> timesteps;
    for (unsigned int i = 0; i < outputData.size(); i++)
    {
      const auto &outdata = outputData[i].second;
      // iterate columns (parameters)
      for (unsigned int j = 0; j < outdata.size(); j++)
      {
        auto parameter_name = parse_parameter_name(query_parameters[j].name());
        const auto &parameter_precision = emd.getPrecision(parameter_name);

        if (lon_lat_level_param(parameter_name))
          continue;

        auto json_param_object = Json::Value(Json::ValueType::objectValue);
        auto tsdata = outdata.at(j);
        auto values = Json::Value(Json::ValueType::arrayValue);
        unsigned int values_index = 0;

        TS::TimeSeriesGroupPtr tsg = *(boost::get<TS::TimeSeriesGroupPtr>(&tsdata));

        for (const auto &llts : *tsg)
        {
          for (unsigned int k = 0; k < llts.timeseries.size(); k++)
          {
            const auto ts = llts.timeseries;
            if (i == 0)
            {
              coverage = add_prologue_multi_point(level, emd, coordinates);
              coverage["parameters"] = get_edr_series_parameters(query_parameters, emd);
              json_param_object["type"] = Json::Value("NdArray");
              auto shape = Json::Value(Json::ValueType::arrayValue);
              shape[0] = Json::Value(ts.size());
              auto axis_names = Json::Value(Json::ValueType::arrayValue);
              axis_names[0] = Json::Value("t");
              if (coordinates.size() > 1)
              {
                axis_names[1] = Json::Value("composite");
                if (level)
                  axis_names[2] = Json::Value("z");
                shape[1] = Json::Value(coordinates.size());
                if (level)
                  shape[2] = Json::Value(1);
              }
              else
              {
                axis_names[1] = Json::Value("x");
                axis_names[2] = Json::Value("y");
                if (level)
                  axis_names[3] = Json::Value("z");
                shape[1] = Json::Value(coordinates.size());
                shape[2] = Json::Value(coordinates.size());
                if (level)
                  shape[3] = Json::Value(1);
              }
              json_param_object["axisNames"] = axis_names;
              json_param_object["shape"] = shape;
            }

            const auto &timed_value = ts.at(k);
            add_value(timed_value,
                      values,
                      json_param_object["dataType"],
                      timesteps,
                      values_index,
                      parameter_precision);

            values_index++;
          }
        }
        json_param_object["values"] = values;
        ranges[parameter_name] = json_param_object;
      }
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
#endif

Json::Value format_coverage_collection_point(const DataPerParameter &dpp,
                                             const EDRMetaData &emd,
                                             const std::vector<Spine::Parameter> &query_parameters)
{
  try
  {
    //	std::cout << "format_coverage_collection_point" << std::endl;

    Json::Value coverage_collection;

    if (dpp.empty())
      return coverage_collection;

    const auto &longitude_precision = emd.getPrecision("longitude");
    const auto &latitude_precision = emd.getPrecision("latitude");

    bool levels_present = false;
    auto coverages = Json::Value(Json::ValueType::arrayValue);
    for (const auto &dpp_item : dpp)
    {
      const auto &parameter_name = dpp_item.first;
      const auto &parameter_precision = emd.getPrecision(parameter_name);
      const auto &dpl = dpp_item.second;

      // Iterate data per level
      for (const auto &dpl_item : dpl)
      {
        int level = dpl_item.first;
        // Levels present in some item
        levels_present = (levels_present || (dpl_item.first != std::numeric_limits<double>::max()));

        auto values = dpl_item.second;
        auto data_values = Json::Value(Json::ValueType::arrayValue);
        auto time_coord_values = Json::Value(Json::ValueType::arrayValue);
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
          if (levels_present)
          {
            auto domain_axes_z = Json::Value(Json::ValueType::objectValue);
            domain_axes_z["values"] = Json::Value(Json::ValueType::arrayValue);
            domain_axes_z["values"][0] = Json::Value(level);
            domain_axes["z"] = domain_axes_z;
          }

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

          range_item["axisNames"] = axis_names;
          auto values = Json::Value(Json::ValueType::arrayValue);
          if (value.value)
          {
            values[0] = UtilityFunctions::json_value(*value.value, parameter_precision);
            range_item["dataType"] = Json::Value(emd.isAviProducer ? "string" : "float");
          }
          else
          {
            values[0] = Json::Value();
            range_item["dataType"] = Json::Value(emd.isAviProducer ? "string" : "float");
          }
          range_item["values"] = values;
          auto ranges = Json::Value(Json::ValueType::objectValue);
          ranges[parameter_name] = range_item;
          coverage["ranges"] = ranges;
          coverages[coverages.size()] = coverage;
        }
      }
    }

    coverage_collection =
        add_prologue_coverage_collection(emd, query_parameters, levels_present, "Point");
    coverage_collection["coverages"] = coverages;

    return coverage_collection;
  }
  catch (...)
  {
    throw Fmi::Exception::Trace(BCP, "Operation failed!");
  }
}

#if 0
Json::Value format_coverage_collection_trajectory_alternative(
    const DataPerParameter &dpp,
    const EDRMetaData &emd,
    const std::vector<Spine::Parameter> &query_parameters)
{
  try
  {
    Json::Value coverage;

    if (dpp.empty())
      return coverage;

    auto time_coord_values = Json::Value(Json::ValueType::arrayValue);
    auto ranges = Json::Value(Json::ValueType::objectValue);

    auto levels_present = false;

    const auto &longitude_precision = emd.getPrecision("longitude");
    const auto &latitude_precision = emd.getPrecision("latitude");

    auto data_values_per_parameter = std::map<std::string, Json::Value>();
    for (const auto &dpp_item : dpp)
    {
      const auto &parameter_name = dpp_item.first;
      const auto &parameter_precision = emd.getPrecision(parameter_name);
      const auto &dpl = dpp_item.second;
      auto data_values = Json::Value(Json::ValueType::arrayValue);
      for (const auto &dpl_item : dpl)
      {
        int level = dpl_item.first;
        // Levels present in some item
        levels_present = (levels_present || (dpl_item.first != std::numeric_limits<double>::max()));

        auto values = dpl_item.second;

        for (const auto &value : values)
        {
          auto time_coord_value = Json::Value(Json::ValueType::arrayValue);
          time_coord_value[0] = Json::Value(value.time);
          time_coord_value[1] = Json::Value(value.lon, longitude_precision);
          time_coord_value[2] = Json::Value(value.lat, latitude_precision);
          if (levels_present)
            time_coord_value[3] = Json::Value(level);
          if (value.value)
            data_values[data_values.size()] =
                UtilityFunctions::json_value(*value.value, parameter_precision);
          else
            data_values[data_values.size()] = Json::Value();
          time_coord_values[time_coord_values.size()] = time_coord_value;
        }
      }
      data_values_per_parameter[parameter_name] = data_values;
    }

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

    auto referencing = Json::Value(Json::ValueType::arrayValue);
    auto referencing_xy = Json::Value(Json::ValueType::objectValue);
    referencing_xy["coordinates"] = Json::Value(Json::ValueType::arrayValue);
    referencing_xy["coordinates"][0] = Json::Value("y");
    referencing_xy["coordinates"][1] = Json::Value("x");
    referencing_xy["system"] = Json::Value(Json::ValueType::objectValue);
    referencing_xy["system"]["type"] = Json::Value("GeographicCRS");
    referencing_xy["system"]["id"] = Json::Value("http://www.opengis.net/def/crs/EPSG/0/4326");

    auto referencing_z = Json::Value(Json::ValueType::objectValue);
    if (levels_present)
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
    referencing_time["system"]["type"] = Json::Value("TemporalCRS");
    referencing_time["system"]["calendar"] = Json::Value("Gregorian");
    referencing[0] = referencing_xy;
    if (levels_present)
    {
      referencing[1] = referencing_z;
      referencing[2] = referencing_time;
    }
    else
    {
      referencing[1] = referencing_time;
    }

    coverage_domain["referencing"] = referencing;

    for (const auto &item : data_values_per_parameter)
    {
      auto parameter_name = item.first;
      auto values = item.second;
      auto parameter_range = Json::Value(Json::ValueType::objectValue);
      parameter_range["type"] = Json::Value("NdArray");
      parameter_range["dataType"] = Json::Value("float");
      auto axis_names = Json::Value(Json::ValueType::arrayValue);
      axis_names[0] = Json::Value("composite");
      parameter_range["axisNames"] = axis_names;
      auto shape = Json::Value(Json::ValueType::arrayValue);
      shape[0] = Json::Value(values.size());
      parameter_range["shape"] = shape;
      parameter_range["values"] = item.second;
      ranges[parameter_name] = parameter_range;
    }

    coverage["type"] = Json::Value("Coverage");
    coverage["domainType"] = Json::Value("Trajectory");
    coverage["parameters"] = get_edr_series_parameters(query_parameters, emd);
    coverage["domain"] = coverage_domain;
    coverage["ranges"] = ranges;

    return coverage;
  }
  catch (...)
  {
    throw Fmi::Exception::Trace(BCP, "Operation failed!");
  }
}
#endif

Json::Value format_coverage_collection_trajectory(
    const DataPerParameter &dpp,
    const EDRMetaData &emd,
    const std::vector<Spine::Parameter> &query_parameters)
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
    for (const auto &dpp_item : dpp)
    {
      const auto &parameter_name = dpp_item.first;
      const auto &dpl = dpp_item.second;
      const auto &parameter_precision = emd.getPrecision(parameter_name);

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
    coverage_collection =
        add_prologue_coverage_collection(emd, query_parameters, levels_present, "Trajectory");
    coverage_collection["coverages"] = coverages;

    return coverage_collection;
  }
  catch (...)
  {
    throw Fmi::Exception::Trace(BCP, "Operation failed!");
  }
}

DataPerParameter get_data_per_parameter(const TS::OutputData &outputData,
                                        const EDRMetaData & /*emd */,
                                        const std::set<int> &levels,
                                        const CoordinateFilter &coordinate_filter,
                                        const std::vector<Spine::Parameter> &query_parameters)
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
    const auto &last_param = query_parameters.back();
    auto last_param_name = last_param.name();
    boost::algorithm::to_lower(last_param_name);
    if (levels_present)
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

    for (const auto &output : outputData)
    {
      const auto &outdata = output.second;

      // iterate columns (parameters)
      for (unsigned int j = 0; j < outdata.size(); j++)
      {
        auto parameter_name = parse_parameter_name(query_parameters[j].name());
        DataPerLevel dpl;

        if (lon_lat_level_param(parameter_name))
          continue;

        auto tsdata = outdata.at(j);
        auto tslon = outdata.at(longitude_index);
        auto tslat = outdata.at(latitude_index);
        TS::TimeSeriesGroupPtr tsg_data = *(boost::get<TS::TimeSeriesGroupPtr>(&tsdata));
        TS::TimeSeriesGroupPtr tsg_lon = *(boost::get<TS::TimeSeriesGroupPtr>(&tslon));
        TS::TimeSeriesGroupPtr tsg_lat = *(boost::get<TS::TimeSeriesGroupPtr>(&tslat));
        TS::TimeSeriesGroupPtr tsg_level = nullptr;
        if (levels_present)
        {
          auto tslevel = outdata.at(level_index);
          tsg_level = *(boost::get<TS::TimeSeriesGroupPtr>(&tslevel));
        }

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
            tcv.time =
                (boost::posix_time::to_iso_extended_string(data_value.time.utc_time()) + "Z");

            double level = std::numeric_limits<double>::max();
            if (levels_present)
            {
              const auto &llts_level = tsg_level->at(k);
              const auto &level_value = llts_level.timeseries.at(l);
              if (boost::get<double>(&level_value.value) != nullptr)
              {
                level = *(boost::get<double>(&level_value.value));
              }
              else if (boost::get<int>(&level_value.value) != nullptr)
              {
                level = *(boost::get<int>(&level_value.value));
              }
            }

            if (data_value.value != TS::None())
              tcv.value = data_value.value;
            bool accept =
                coordinate_filter.accept(tcv.lon, tcv.lat, level, data_value.time.utc_time());

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
        dpp[parameter_name] = dpl;
      }
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
    EDRQueryType query_type)

{
  try
  {
    if (outputData.empty())
      Json::Value();

    auto dpp = get_data_per_parameter(outputData, emd, levels, coordinate_filter, query_parameters);

    if (query_type == EDRQueryType::Trajectory)
      return format_coverage_collection_trajectory(dpp, emd, query_parameters);

    return format_coverage_collection_point(dpp, emd, query_parameters);
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

    const auto &tsdata_first = outdata_first.at(0);

    if (boost::get<TS::TimeSeriesPtr>(&tsdata_first))
    {
      // Zero or one levels
      if (levels.size() <= 1)
      {
        boost::optional<int> level;
        if (levels.size() == 1)
          level = *(levels.begin());
        return format_output_data_one_point(outputData, emd, level, query_parameters);
      }

      // More than one level
      return format_output_data_position(outputData, emd, query_parameters);
    }

    if (boost::get<TS::TimeSeriesVectorPtr>(&tsdata_first))
    {
      if (outdata_first.size() > 1)
        std::cout << "formatOutputData - TS::TimeSeriesVectorPtr - Can do "
                     "nothing -> report error! "
                  << std::endl;
      std::vector<TS::TimeSeriesData> tsd;
      TS::TimeSeriesVectorPtr tsv = *(boost::get<TS::TimeSeriesVectorPtr>(&tsdata_first));
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
        boost::optional<int> level;
        if (levels.size() == 1)
          level = *(levels.begin());
        return format_output_data_one_point(od, emd, level, query_parameters);
      }
      // More than one level
      return format_output_data_position(od, emd, query_parameters);
    }

    if (boost::get<TS::TimeSeriesGroupPtr>(&tsdata_first))
    {
      return format_output_data_coverage_collection(
          outputData, emd, levels, coordinate_filter, query_parameters, query_type);
    }

    return empty_result;
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

    auto longitude_precision = edr_md->getPrecision("longitude");
    auto latitude_precision = edr_md->getPrecision("latitude");

    result["type"] = Json::Value("FeatureCollection");
    auto features = Json::Value(Json::ValueType::arrayValue);

    for (const auto &item : *edr_md->locations)
    {
      const auto &loc = item.second;
      auto feature = Json::Value(Json::ValueType::objectValue);
      feature["type"] = Json::Value("Feature");
      feature["id"] = Json::Value(loc.id);
      auto geometry = Json::Value(Json::ValueType::objectValue);
      geometry["type"] = Json::Value("Point");
      auto coordinates = Json::Value(Json::ValueType::arrayValue);
      coordinates[0] = Json::Value(loc.longitude, longitude_precision);
      coordinates[1] = Json::Value(loc.latitude, latitude_precision);
      auto properties = Json::Value(Json::ValueType::objectValue);
      properties["name"] = Json::Value(loc.name);
	  auto detail_string = ("Id is " + loc.type + " from " + loc.keyword);
      properties["detail"] = Json::Value(detail_string);
	  if(!edr_md->temporal_extent.time_periods.empty())
		{
		  auto start_time = edr_md->temporal_extent.time_periods.front().start_time;
		  auto end_time = edr_md->temporal_extent.time_periods.back().end_time;
		  if(end_time.is_not_a_date_time())
			end_time = edr_md->temporal_extent.time_periods.back().start_time;
		  properties["datetime"] = Json::Value(Fmi::to_iso_extended_string(start_time)+"Z/"+Fmi::to_iso_extended_string(end_time)+"Z");
		}
      geometry["coordinates"] = coordinates;
      feature["geometry"] = geometry;
      feature["properties"] = properties;
      features[features.size()] = feature;
    }
    result["features"] = features;

    return result;
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

Json::Value parseEDRMetaData(const EDRQuery &edr_query, const EngineMetaData &emd)
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
        auto md = parse_edr_metadata(engine_metadata, edr_query);
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
        result = parse_edr_metadata(*producer_metadata, edr_query);
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
