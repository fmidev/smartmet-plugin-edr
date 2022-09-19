#include "CoverageJson.h"
#include <engines/querydata/Engine.h>
#include <engines/grid/Engine.h>
#include <macgyver/Exception.h>
#include <macgyver/StringConversion.h>
#include <timeseries/TimeSeriesOutput.h>
#include <boost/optional.hpp>
#ifndef WITHOUT_OBSERVATION
#include <engines/observation/Engine.h>
#include <engines/observation/ObservableProperty.h>
#endif

namespace SmartMet {
namespace Plugin {
namespace EDR {
namespace CoverageJson {
namespace {

struct time_coord_value
{
  std::string time;
  double lon;
  double lat;
  boost::optional<double> value;
};

using DataPerLevel = std::map<double, std::vector<time_coord_value>>; // level -> array of values
using DataPerParameter = std::map<std::string, DataPerLevel>; // parameter_name -> data


bool lon_lat_level_param(const std::string& name)
{
  return (name == "longitude" || name == "latitude"  || name == "level");
}

std::string parse_parameter_name(const std::string& name)
{
  auto parameter_name = name;

  // If ':' exists patameter_name is before first ':'
  if(parameter_name.find(":") != std::string::npos)
    parameter_name.resize(parameter_name.find(":"));
  // Modify to lower case
  boost::algorithm::to_lower(parameter_name);
  
  return parameter_name;
}
  
Json::Value parse_temporal_extent(const edr_temporal_extent& temporal_extent)
{
  auto temporal = Json::Value(Json::ValueType::objectValue);
  auto temporal_interval = Json::Value(Json::ValueType::arrayValue);
  auto temporal_interval2 = Json::Value(Json::ValueType::arrayValue);
  temporal_interval2[0] = Json::Value(boost::posix_time::to_iso_extended_string(temporal_extent.start_time) + "Z");
  temporal_interval[0] = temporal_interval2;
  
  auto temporal_interval_values = Json::Value(Json::ValueType::arrayValue);
  temporal_interval_values[0] = Json::Value("R" + Fmi::to_string(temporal_extent.timesteps) + "/"+boost::posix_time::to_iso_extended_string(temporal_extent.start_time) + "Z/PT" + Fmi::to_string(temporal_extent.timestep) + "M");
  auto trs = Json::Value("TIMECRS[\"DateTime\",TDATUM[\"Gregorian "
						 "Calendar\"],CS[TemporalDateTime,1],AXIS[\"Time "
						 "(T)\",future]");
  
  temporal["interval"] = temporal_interval;
  temporal["values"] = temporal_interval_values;
  temporal["trs"] = trs;
  
  return temporal;
}

Json::Value get_data_queries(const EDRQuery &edr_query, const std::string& producer)
{
  auto data_queries = Json::Value(Json::ValueType::objectValue);

  auto data_query_set = edr_query.data_queries.find(producer) != edr_query.data_queries.end() ? edr_query.data_queries.at(producer) : edr_query.data_queries.at(DEFAULT_DATA_QUERIES);
  for(const auto& query_type : data_query_set)
    {
      auto query_info = Json::Value(Json::ValueType::objectValue);
      auto query_info_variables = Json::Value(Json::ValueType::objectValue);
      auto query_info_link = Json::Value(Json::ValueType::objectValue);
      query_info_link["rel"] = Json::Value("data");
      query_info_link["templated"] = Json::Value(true);
      
	  std::string query_type_string = "";
      if(query_type == EDRQueryType::Position)
		{
		  query_info_link["title"] = Json::Value("Position query");
		  query_info_link["href"] = Json::Value((edr_query.host + "/collections/" + producer + "/position"));
		  query_info_variables["title"] = Json::Value("Position query");
		  query_info_variables["description"] = Json::Value("Data at point location");
		  query_info_variables["query_type"] = Json::Value("position");
		  query_info_variables["coords"] = Json::Value("Well Known Text POINT value i.e. POINT(24.9384 60.1699)");
		  query_type_string = "position";
		}
      else if(query_type == EDRQueryType::Radius)
		{
		  query_info_link["title"] = Json::Value("Radius query");
		  query_info_link["href"] = Json::Value((edr_query.host + "/collections/" + producer + "/radius"));
		  query_info_variables["title"] = Json::Value("Radius query");
		  query_info_variables["description"] = Json::Value("Data at the area specified with a geographic position and radial distance");
		  query_info_variables["query_type"] = Json::Value("radius");
		  query_info_variables["coords"] = Json::Value("Well Known Text POINT value i.e. POINT(24.9384 60.1699)");
		  auto within_units = Json::Value(Json::ValueType::arrayValue);
		  within_units[0] = Json::Value("km");
		  within_units[1] = Json::Value("mi");
		  query_info_variables["within_units"] = within_units;
		  query_type_string = "radius";
		}
      else if(query_type == EDRQueryType::Area)
		{
		  query_info_link["title"] = Json::Value("Area query");
		  query_info_link["href"] = Json::Value((edr_query.host + "/collections/" + producer + "/area"));
		  query_info_variables["title"] = Json::Value("Area query");
		  query_info_variables["description"] = Json::Value("Data at the requested area");
		  query_info_variables["query_type"] = Json::Value("area");
		  query_info_variables["coords"] = Json::Value("Well Known Text POLYGON value i.e. POLYGON((24 61,24 61.5,24.5 61.5,24.5 61,24 61))");
		  query_type_string = "area";
		}
      else if(query_type == EDRQueryType::Locations)
		{
		  query_info_link["title"] = Json::Value("Locations query");
		  query_info_link["href"] = Json::Value((edr_query.host + "/collections/" + producer + "/locations"));
		  query_info_variables["title"] = Json::Value("Locations query");
		  query_info_variables["description"] = Json::Value("Data at point location defined by a unique identifier");
		  query_info_variables["query_type"] = Json::Value("locations");
		  query_type_string = "locations";
		}
      else if(query_type == EDRQueryType::Trajectory)
		{
		  query_info_link["title"] = Json::Value("Trajectory query");
		  query_info_link["href"] = Json::Value((edr_query.host + "/collections/" + producer + "/trajectory"));
		  query_info_variables["title"] = Json::Value("Trajectory query");
		  query_info_variables["description"] = Json::Value("Data along trajectory");
		  query_info_variables["query_type"] = Json::Value("trajectory");
		  query_info_variables["coords"] = Json::Value("Well Known Text LINESTRING value i.e. LINESTRING(24 61,24.2 61.2,24.3 61.3)");
		  query_type_string = "trajectory";
		}
       auto query_info_output_formats = Json::Value(Json::ValueType::arrayValue);
      query_info_output_formats[0] = Json::Value("CoverageJSON");
      query_info_variables["output_formats"] = query_info_output_formats;
      query_info_variables["default_output_format"] = Json::Value("CoverageJSON");

      auto query_info_crs_details = Json::Value(Json::ValueType::arrayValue);
      auto query_info_crs_details_0 = Json::Value(Json::ValueType::objectValue);
      query_info_crs_details_0["crs"] = Json::Value("EPSG:4326");
      query_info_crs_details_0["wkt"] = Json::Value("GEOGCS[\"WGS 84\",DATUM[\"WGS_1984\",SPHEROID[\"WGS 84\",6378137,298.257223563]],PRIMEM[\"Greenwich\",0],UNIT[\"degree\",0.0174532925199433,AUTHORITY[\"EPSG\",\"9122\"]],AUTHORITY[\"EPSG\",\"4326\"]]");

      query_info_crs_details[0] = query_info_crs_details_0;
      query_info_variables["crs_details"] = query_info_crs_details;

      query_info_link["variables"] = query_info_variables;
      query_info["link"] = query_info_link;
	  data_queries[query_type_string] = query_info;
    }

  return data_queries;
}


std::vector<TS::LonLat>
get_coordinates(TS::TimeSeriesVectorPtr& tsv,
                const std::vector<Spine::Parameter> &query_parameters) {
  try {
	std::vector<TS::LonLat> ret;
	
	std::vector<double> longitude_vector;
	std::vector<double> latitude_vector;
	
	for (unsigned int i = 0; i < tsv->size(); i++) {
	  auto param_name = query_parameters[i].name();
	  if (param_name != "longitude" && param_name != "latitude")
		continue;
	  
	  const TS::TimeSeries& ts = tsv->at(i);
	  if (ts.size() > 0) {
		const auto &tv = ts.front();
		double value =
		  (boost::get<double>(&tv.value) != nullptr
		   ? *(boost::get<double>(&tv.value))
		   : Fmi::stod(*(boost::get<std::string>(&tv.value))));
		if (param_name == "longitude") {
		  longitude_vector.push_back(value);
		} else {
		  latitude_vector.push_back(value);
		}		
	  }
	}

	if (longitude_vector.size() != latitude_vector.size())
	  throw Fmi::Exception(
						   BCP,
						   "Something wrong: latitude_vector.size() != longitude_vector.size()!",
						   nullptr);
	
	for (unsigned int i = 0; i < longitude_vector.size(); i++)
	  ret.push_back(TS::LonLat(longitude_vector.at(i), latitude_vector.at(i)));		
	
	return ret;	
  } catch (...) {
	throw Fmi::Exception::Trace(BCP, "Operation failed!");
  }
}


std::vector<TS::LonLat>
get_coordinates(const TS::OutputData &outputData,
                const std::vector<Spine::Parameter> &query_parameters) {
  try {
    std::vector<TS::LonLat> ret;

    if (outputData.empty())
      return ret;

    const auto &outdata = outputData.at(0).second;

	if(outdata.empty())
      return ret;

    const auto &outdata_front = outdata.front();
	if (boost::get<TS::TimeSeriesVectorPtr>(&outdata_front)) {
        TS::TimeSeriesVectorPtr tsv =
            *(boost::get<TS::TimeSeriesVectorPtr>(&outdata_front));
		return get_coordinates(tsv, query_parameters);
	}

   
    std::vector<double> longitude_vector;
    std::vector<double> latitude_vector;

    // iterate columns (parameters)
    for (unsigned int i = 0; i < outdata.size(); i++) {
      auto param_name = query_parameters[i].name();
      if (param_name != "longitude" && param_name != "latitude")
        continue;

      auto tsdata = outdata.at(i);
      if (boost::get<TS::TimeSeriesPtr>(&tsdata)) {
        TS::TimeSeriesPtr ts = *(boost::get<TS::TimeSeriesPtr>(&tsdata));
        if (ts->size() > 0) {
          const TS::TimedValue &tv = ts->at(0);
		  double value =
			(boost::get<double>(&tv.value) != nullptr
			 ? *(boost::get<double>(&tv.value))
			 : Fmi::stod(*(boost::get<std::string>(&tv.value))));
		  
          if (param_name == "longitude")
            longitude_vector.push_back(value);
          else
            latitude_vector.push_back(value);
        }
      } else if (boost::get<TS::TimeSeriesVectorPtr>(&tsdata)) {
		
        std::cout
		  << "get_coordinates -> TS::TimeSeriesVectorPtr - Shouldnt be here -> report error!!:\n" << tsdata
            << std::endl;
      } else if (boost::get<TS::TimeSeriesGroupPtr>(&tsdata)) {
        TS::TimeSeriesGroupPtr tsg =
            *(boost::get<TS::TimeSeriesGroupPtr>(&tsdata));

        for (const auto &llts : *tsg) {
          const auto &ts = llts.timeseries;

          if (ts.size() > 0) {
            const auto &tv = ts.front();
            double value =
                (boost::get<double>(&tv.value) != nullptr
                     ? *(boost::get<double>(&tv.value))
                     : Fmi::stod(*(boost::get<std::string>(&tv.value))));
            if (param_name == "longitude") {
              longitude_vector.push_back(value);
            } else {
              latitude_vector.push_back(value);
            }
          }
        }
      }
    }

    if (longitude_vector.size() != latitude_vector.size())
      throw Fmi::Exception(
          BCP,
          "Something wrong: latitude_vector.size() != longitude_vector.size()!",
          nullptr);

    for (unsigned int i = 0; i < longitude_vector.size(); i++)
      ret.push_back(TS::LonLat(longitude_vector.at(i), latitude_vector.at(i)));

    return ret;
  } catch (...) {
    throw Fmi::Exception::Trace(BCP, "Operation failed!");
  }
}

Json::Value get_edr_series_parameters(
    const std::vector<Spine::Parameter> &query_parameters,
    const std::map<std::string, edr_parameter> &edr_parameters) {
  try {
    auto parameters = Json::Value(Json::ValueType::objectValue);

    for (const auto &p : query_parameters) {
      auto parameter_name = parse_parameter_name(p.name());
      
	  if(lon_lat_level_param(parameter_name))
        continue;

      auto parameter = Json::Value(Json::ValueType::objectValue);
      parameter["type"] = Json::Value("Parameter");
	  const auto &edr_parameter = edr_parameters.at(parameter_name);
	  /*
		// Description field is optional
		// QEngine returns parameter description in finnish and skandinavian characters cause problems
		parameter["description"] = Json::Value(Json::ValueType::objectValue);
		parameter["description"]["en"] = Json::Value(edr_parameter.description);
	  */
      parameter["unit"] = Json::Value(Json::ValueType::objectValue);
      parameter["unit"]["label"] = Json::Value(Json::ValueType::objectValue);
      parameter["unit"]["label"]["en"] = Json::Value("Label of parameter...");
      parameter["unit"]["symbol"] = Json::Value(Json::ValueType::objectValue);
      parameter["unit"]["symbol"]["value"] =
          Json::Value("Symbol of parameter ...");
      parameter["unit"]["symbol"]["type"] =
          Json::Value("Type of parameter ...");
      parameter["observedProperty"] = Json::Value(Json::ValueType::objectValue);
      parameter["observedProperty"]["id"] =
          Json::Value("Id of observed property...");
      parameter["observedProperty"]["label"] =
          Json::Value(Json::ValueType::objectValue);
      parameter["observedProperty"]["label"]["en"] =
          Json::Value(edr_parameter.name);
      parameters[parameter_name] = parameter;
    }

    return parameters;
  } catch (...) {
    throw Fmi::Exception::Trace(BCP, "Operation failed!");
  }
}


void add_value(const TS::TimedValue &tv, Json::Value &values_array,
               Json::Value &data_type, unsigned int values_index) {
  try {
    const auto &val = tv.value;

    if (boost::get<double>(&val) != nullptr) {
      if (values_index == 0)
        data_type = Json::Value("float");
      values_array[values_index] = Json::Value(*(boost::get<double>(&val)));
    } else if (boost::get<int>(&val) != nullptr) {
      if (values_index == 0)
        data_type = Json::Value("int");
      values_array[values_index] = Json::Value(*(boost::get<int>(&val)));
    } else if (boost::get<std::string>(&val) != nullptr) {
      if (values_index == 0)
        data_type = Json::Value("string");
      values_array[values_index] =
          Json::Value(*(boost::get<std::string>(&val)));
    } else {
      Json::Value nulljson;
      if (values_index == 0)
        data_type = nulljson;
      values_array[values_index] = nulljson;
    }
  } catch (...) {
    throw Fmi::Exception::Trace(BCP, "Operation failed!");
  }
}

void add_value(const TS::TimedValue &tv, Json::Value &values_array,
               Json::Value &data_type, std::set<std::string> &timesteps,
               unsigned int values_index) {
  try {
    const auto &t = tv.time;
    timesteps.insert(boost::posix_time::to_iso_extended_string(t.utc_time())+"Z");
    add_value(tv, values_array, data_type, values_index);
  } catch (...) {
    throw Fmi::Exception::Trace(BCP, "Operation failed!");
  }
}

Json::Value add_prologue_one_point(boost::optional<int> level,
                                   const std::string &level_type,
                                   double longitude, double latitude) {
  try {
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
    x_array[0] = Json::Value(longitude);
    y_array[0] = Json::Value(latitude);

    if (level) {
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
    referencing_xy["system"]["id"] =
        Json::Value("http://www.opengis.net/def/crs/EPSG/0/4326");

    // Referencing z coordinate
    auto referencing_z = Json::Value(Json::ValueType::objectValue);
    if (level) {
      referencing_z["coordinates"][0] = Json::Value("z");
      referencing_z["system"]["type"] = Json::Value("VerticalCRS");
      referencing_z["system"]["id"] = Json::Value("TODO: " + level_type);
    }

    auto referencing_time = Json::Value(Json::ValueType::objectValue);
    referencing_time["coordinates"] = Json::Value(Json::ValueType::arrayValue);
    referencing_time["coordinates"][0] = Json::Value("t");
    referencing_time["system"] = Json::Value(Json::ValueType::objectValue);
    referencing_time["system"]["type"] = Json::Value("TemporalCRS");
    referencing_time["system"]["calendar"] = Json::Value("Georgian");
    referencing[0] = referencing_xy;
    if (level) {
      referencing[1] = referencing_z;
      referencing[2] = referencing_time;
    } else {
      referencing[1] = referencing_time;
    }

    domain["referencing"] = referencing;
    domain["domainType"] = Json::Value("PointSeries");
    coverage["domain"] = domain;

    return coverage;
  } catch (...) {
    throw Fmi::Exception::Trace(BCP, "Operation failed!");
  }
}

Json::Value
add_prologue_multi_point(boost::optional<int> level,
                         const std::string &level_type,
                         const std::vector<TS::LonLat> &coordinates) {
  try {
    Json::Value coverage;

    coverage["type"] = Json::Value("Coverage");
    auto domain = Json::Value(Json::ValueType::objectValue);
    domain["type"] = Json::Value("Domain");
    domain["axes"] = Json::Value(Json::ValueType::objectValue);

    if (coordinates.size() > 1) {
      auto composite = Json::Value(Json::ValueType::objectValue);
      composite["dataType"] = Json::Value("tuple");
      composite["coordinates"] = Json::Value(Json::ValueType::arrayValue);
      composite["coordinates"][0] = Json::Value("x");
      composite["coordinates"][1] = Json::Value("y");
      auto values = Json::Value(Json::ValueType::arrayValue);
      for (unsigned int i = 0; i < coordinates.size(); i++) {
        values[i] = Json::Value(Json::ValueType::arrayValue);
        values[i][0] = Json::Value(coordinates.at(i).lon);
        values[i][1] = Json::Value(coordinates.at(i).lat);
      }
      composite["values"] = values;
      domain["axes"]["composite"] = composite;
    } else {
      domain["axes"]["x"] = Json::Value(Json::ValueType::objectValue);
      domain["axes"]["x"]["values"] = Json::Value(Json::ValueType::arrayValue);
      auto &x_array = domain["axes"]["x"]["values"];
      domain["axes"]["y"] = Json::Value(Json::ValueType::objectValue);
      domain["axes"]["y"]["values"] = Json::Value(Json::ValueType::arrayValue);
      auto &y_array = domain["axes"]["y"]["values"];
      for (unsigned int i = 0; i < coordinates.size(); i++) {
        x_array[i] = Json::Value(coordinates.at(i).lon);
        y_array[i] = Json::Value(coordinates.at(i).lat);
      }
    }

    if (level) {
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
    referencing_xy["system"]["id"] =
        Json::Value("http://www.opengis.net/def/crs/EPSG/0/4326");

    auto referencing_z = Json::Value(Json::ValueType::objectValue);
    if (level) {
      referencing_z["coordinates"][0] = Json::Value("z");
      referencing_z["system"]["type"] = Json::Value("VerticalCRS");
      referencing_z["system"]["id"] = Json::Value("TODO: " + level_type);
    }

    auto referencing_time = Json::Value(Json::ValueType::objectValue);
    referencing_time["coordinates"] = Json::Value(Json::ValueType::arrayValue);
    referencing_time["coordinates"][0] = Json::Value("t");
    referencing_time["system"] = Json::Value(Json::ValueType::objectValue);
    referencing_time["system"]["type"] = Json::Value("TemporalCRS");
    referencing_time["system"]["calendar"] = Json::Value("Georgian");
    referencing[0] = referencing_xy;
    if (level) {
      referencing[1] = referencing_z;
      referencing[2] = referencing_time;
    } else {
      referencing[1] = referencing_time;
    }

    domain["referencing"] = referencing;
    domain["domainType"] = Json::Value("MultiPointSeries");
    coverage["domain"] = domain;

    return coverage;
  } catch (...) {
    throw Fmi::Exception::Trace(BCP, "Operation failed!");
  }
}

Json::Value
add_prologue_coverage_collection(const EDRMetaData &emd,
								 const std::vector<Spine::Parameter> &query_parameters,
								 bool levels_exists,
								 const std::string& domain_type)
{
  try 
	{
	  Json::Value coverage_collection;
	  
	  coverage_collection["type"] = Json::Value("CoverageCollection");
	  coverage_collection["parameters"] = get_edr_series_parameters(query_parameters, emd.parameters);
	  
	  auto referencing = Json::Value(Json::ValueType::arrayValue);
	  auto referencing_xy = Json::Value(Json::ValueType::objectValue);
	  referencing_xy["coordinates"] = Json::Value(Json::ValueType::arrayValue);
	  referencing_xy["coordinates"][0] = Json::Value("y");
	  referencing_xy["coordinates"][1] = Json::Value("x");
	  referencing_xy["system"] = Json::Value(Json::ValueType::objectValue);
	  referencing_xy["system"]["type"] = Json::Value("GeographicCRS");
	  referencing_xy["system"]["id"] =
        Json::Value("http://www.opengis.net/def/crs/EPSG/0/4326");
	  
	  auto referencing_z = Json::Value(Json::ValueType::objectValue);
	  if(levels_exists)
		{
		  referencing_z["coordinates"][0] = Json::Value("z");
		  referencing_z["system"]["type"] = Json::Value("VerticalCRS");
		  referencing_z["system"]["id"] = Json::Value("TODO: " + emd.vertical_extent.level_type);
		}
	  
	  auto referencing_time = Json::Value(Json::ValueType::objectValue);
	  referencing_time["coordinates"] = Json::Value(Json::ValueType::arrayValue);
	  referencing_time["coordinates"][0] = Json::Value("t");
	  referencing_time["system"] = Json::Value(Json::ValueType::objectValue);
	  referencing_time["system"]["type"] = Json::Value("TemporalCRS");
	  referencing_time["system"]["calendar"] = Json::Value("Georgian");
	  referencing[0] = referencing_xy;
	  if(levels_exists) 
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
Json::Value
add_prologue_coverage_collection(boost::optional<int> level,
                                 const std::string &level_type,
                                 const std::vector<TS::LonLat> &coordinates) {
  try {
    Json::Value coverage_collection;

    coverage_collection["type"] = Json::Value("CoverageCollection");

    auto referencing = Json::Value(Json::ValueType::arrayValue);
    auto referencing_xy = Json::Value(Json::ValueType::objectValue);
    referencing_xy["coordinates"] = Json::Value(Json::ValueType::arrayValue);
    referencing_xy["coordinates"][0] = Json::Value("y");
    referencing_xy["coordinates"][1] = Json::Value("x");
    referencing_xy["system"] = Json::Value(Json::ValueType::objectValue);
    referencing_xy["system"]["type"] = Json::Value("GeographicCRS");
    referencing_xy["system"]["id"] =
        Json::Value("http://www.opengis.net/def/crs/EPSG/0/4326");

    auto referencing_z = Json::Value(Json::ValueType::objectValue);
    if (level) {
      referencing_z["coordinates"][0] = Json::Value("z");
      referencing_z["system"]["type"] = Json::Value("VerticalCRS");
      referencing_z["system"]["id"] = Json::Value("TODO: " + level_type);
    }

    auto referencing_time = Json::Value(Json::ValueType::objectValue);
    referencing_time["coordinates"] = Json::Value(Json::ValueType::arrayValue);
    referencing_time["coordinates"][0] = Json::Value("t");
    referencing_time["system"] = Json::Value(Json::ValueType::objectValue);
    referencing_time["system"]["type"] = Json::Value("TemporalCRS");
    referencing_time["system"]["calendar"] = Json::Value("Georgian");
    referencing[0] = referencing_xy;
    if (level) {
      referencing[1] = referencing_z;
      referencing[2] = referencing_time;
    } else {
      referencing[1] = referencing_time;
    }

    coverage_collection["referencing"] = referencing;
    coverage_collection["domainType"] = Json::Value("Point");

    return coverage_collection;
  } catch (...) {
    throw Fmi::Exception::Trace(BCP, "Operation failed!");
  }
}

/*
Table C.1 — EDR Collection Object structure:
https://docs.ogc.org/is/19-086r4/19-086r4.html#toc63
-------------------------------------------------------
| Field Name      | Type                | Required	| Description |
-------------------------------------------------------
| links           |	link Array	        | Yes | Array of Link objects |
| id	          | String	            | Yes | Unique identifier string for
the collection, used as the value for the collection_id path parameter in all
queries on the collection |
| title	          | String	            | No  | A short text label for the
collection | | description     | String	            | No  | A text description
of the information provided by the collection | | keywords	      | String
Array	    | No  | Array of words and phrases that define the information that
the collection provides | | extent	      | extent object	    | Yes |
Object describing the spatio-temporal extent of the information provided by the
collection | | data_queries    | data_queries object	| No  | Object providing
query specific information |
| crs	          | String Array	    | No  | Array of coordinate
reference system names, which define the output coordinate systems supported by
the collection | | output_formats  | String Array	    | No  | Array of
data format names, which define the data formats to which information in the
collection can be output | | parameter_names | parameter_names object | Yes |
Describes the data values available in the collection |
*/

// Metadata of specified producers' specified  collections' instance/instances
Json::Value parse_edr_metadata_instances(const EDRProducerMetaData &epmd,
                                         const EDRQuery &edr_query) {
  try {
    Json::Value nulljson;

    if (epmd.empty())
      return nulljson;

    const auto &producer = epmd.begin()->first;

    auto result = Json::Value(Json::ValueType::objectValue);

    auto links = Json::Value(Json::ValueType::arrayValue);
    auto link_item = Json::Value(Json::ValueType::objectValue);
    link_item["href"] = Json::Value(
        (edr_query.host + "/collections/" + producer + "/instances"));
    link_item["rel"] = Json::Value("self");
    link_item["type"] = Json::Value("application/json");
    links[0] = link_item;

    result["links"] = links;

    auto instances = Json::Value(Json::ValueType::arrayValue);

    const auto &emds = epmd.begin()->second;

    unsigned int instance_index = 0;
    for (const auto &emd : emds) {
      auto instance_id = Fmi::to_iso_string(emd.temporal_extent.origin_time);

      if (edr_query.query_id ==
              EDRQueryId::SpecifiedCollectionSpecifiedInstance &&
          instance_id != edr_query.instance_id)
        continue;

      auto instance = Json::Value(Json::ValueType::objectValue);
      instance["id"] = Json::Value(instance_id);
      std::string title =
          ("Origintime: " +
           boost::posix_time::to_iso_extended_string(emd.temporal_extent.origin_time)+"Z");
      title += (" Starttime: " + boost::posix_time::to_iso_extended_string(
                                     emd.temporal_extent.start_time)+"Z");
      title += (" Endtime: " +
                boost::posix_time::to_iso_extended_string(emd.temporal_extent.end_time)+"Z");
      if (emd.temporal_extent.timestep)
        title +=
            (" Timestep: " + Fmi::to_string(emd.temporal_extent.timestep));
      instance["title"] = Json::Value(title);

      // Links
      auto instance_links = Json::Value(Json::ValueType::arrayValue);
      auto instance_link_item = Json::Value(Json::ValueType::objectValue);
      instance_link_item["href"] =
          Json::Value((edr_query.host + "/collections/" + producer +
                       "/" + instance_id));
      instance_link_item["hreflang"] = Json::Value("en");
      instance_link_item["rel"] = Json::Value("data");
      instance_links[0] = instance_link_item;
      instance["links"] = instance_links;

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
      spatial["crs"] = Json::Value("EPSG:4326");
      extent["spatial"] = spatial;
      // Temporal (optional)
	  extent["temporal"] = parse_temporal_extent(emd.temporal_extent);
      // Vertical (optional)
      if (emd.vertical_extent.levels.size() > 0) {
        auto vertical = Json::Value(Json::ValueType::objectValue);
        auto vertical_interval = Json::Value(Json::ValueType::arrayValue);
        for (unsigned int i = 0; i < emd.vertical_extent.levels.size(); i++)
          vertical_interval[i] = emd.vertical_extent.levels.at(i);
        vertical["interval"] = vertical_interval;
        vertical["vrs"] = emd.vertical_extent.vrs;
        extent["vertical"] = vertical;
      }
      instance["extent"] = extent;
      // Optional: data_queries
	  instance["data_queries"] = get_data_queries(edr_query, producer);

      // Parameter names (mandatory)
      auto parameter_names = Json::Value(Json::ValueType::objectValue);

      // Parameters:
      // Mandatory fields: id, type, observedProperty
      // Optional fields: label, description, data-type, unit, extent,
      // measurementType
      for (const auto &name : emd.parameter_names) {
        const auto &p = emd.parameters.at(name);
        auto param = Json::Value(Json::ValueType::objectValue);
        param["id"] = Json::Value(p.name);
        param["type"] = Json::Value("Parameter");
        param["description"] = Json::Value(p.description);
        // Observed property: Mandatory: label, Optional: id, description
        auto observedProperty = Json::Value(Json::ValueType::objectValue);	
        observedProperty["label"] = Json::Value(p.description);
        //		  observedProperty["id"] = Json::Value("http://....");
        //		  observedProperty["description"] =
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

  } catch (...) {
    throw Fmi::Exception::Trace(BCP, "Operation failed!");
  }
}

Json::Value parse_edr_metadata_collections(const EDRProducerMetaData &epmd,
                                           const EDRQuery &edr_query) {
  try {
    auto collections = Json::Value(Json::ValueType::arrayValue);

    Json::Value nulljson;
    unsigned int collection_index = 0;

    // Iterate QEngine metadata and add item into collection
    for (const auto &item : epmd) {
      const auto &producer = item.first;
      EDRMetaData collection_emd;
      const auto &emds = item.second;
      // Get the most recent instance
      for (unsigned int i = 0; i < emds.size(); i++) {
        if (i == 0 || collection_emd.temporal_extent.origin_time <
                          emds.at(i).temporal_extent.origin_time)
          collection_emd = emds.at(i);
      }

      auto &value = collections[collection_index];
      // Producer is Id
      value["id"] = Json::Value(producer);
      /*
      // Title (optional)
      value["title"] = Json::Value(producer);
      // Description (optional
      value["description"] = Json::Value(producer);
      // Keywords (optional)
      auto keywords = Json::Value(Json::ValueType::arrayValue);
      value["kywords"] = keywords;
      */
      // Array of links (mandatory)
      auto collection_link = Json::Value(Json::ValueType::objectValue);
      collection_link["href"] =
          Json::Value((edr_query.host + "/collections/" + producer));
      if (edr_query.query_id == EDRQueryId::SpecifiedCollection)
        collection_link["rel"] = Json::Value("self");
      else
        collection_link["rel"] = Json::Value("data");
      collection_link["type"] = Json::Value("application/json");
      collection_link["title"] = Json::Value("Collection metadata in JSON");

      auto links = Json::Value(Json::ValueType::arrayValue);
      links[0] = collection_link;
      /*
      // Add instance links if several present
      if (emds.size() > 1) {
        auto instance_link = Json::Value(Json::ValueType::objectValue);
        instance_link["href"] =
            Json::Value((edr_query.host + "/collections/" + producer +
                         "/instances"));
        instance_link["rel"] = Json::Value("data");
        instance_link["type"] = Json::Value("application/json");
        instance_link["title"] = Json::Value("Instance metadata in JSON");
	links[1] = instance_link;
      }
      */

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
      spatial["crs"] = Json::Value("EPSG:4326");
      extent["spatial"] = spatial;
      // Temporal (optional)
      extent["temporal"] = parse_temporal_extent(collection_emd.temporal_extent);
      // Vertical (optional)
      if (collection_emd.vertical_extent.levels.size() > 0) {
        auto vertical = Json::Value(Json::ValueType::objectValue);
        auto vertical_interval = Json::Value(Json::ValueType::arrayValue);
        for (unsigned int i = 0;
             i < collection_emd.vertical_extent.levels.size(); i++)
          vertical_interval[i] = collection_emd.vertical_extent.levels.at(i);
        vertical["interval"] = vertical_interval;
        vertical["vrs"] = collection_emd.vertical_extent.vrs;
        extent["vertical"] = vertical;
      }
      value["extent"] = extent;
      // Optional: data_queries
	  value["data_queries"] = get_data_queries(edr_query, producer);

      // Parameter names (mandatory)
      auto parameter_names = Json::Value(Json::ValueType::objectValue);

      // Parameters:
      // Mandatory fields: id, type, observedProperty
      // Optional fields: label, description, data-type, unit, extent,
      // measurementType
      for (const auto &name : collection_emd.parameter_names) {
        const auto &p = collection_emd.parameters.at(name);
        auto param = Json::Value(Json::ValueType::objectValue);
        param["id"] = Json::Value(p.name);
        param["type"] = Json::Value("Parameter");
        param["description"] = Json::Value(p.description);
        // Observed property: Mandatory: label, Optional: id, description
        auto observedProperty = Json::Value(Json::ValueType::objectValue);
        observedProperty["label"] = Json::Value(p.description);
        //		  observedProperty["id"] = Json::Value("http://....");
        //		  observedProperty["description"] =
        // Json::Value("Description of property...");
        param["observedProperty"] = observedProperty;
        parameter_names[p.name] = param;
      }

      value["parameter_names"] = parameter_names;
      auto output_formats = Json::Value(Json::ValueType::arrayValue);
      output_formats[0] = Json::Value("CoverageJSON");
      value["output_formats"] = output_formats;

      collection_index++;
    }

    // Collections of one producer
    if (edr_query.query_id == EDRQueryId::SpecifiedCollection &&
        collection_index == 1)
      return collections[0];

    return collections;
  } catch (...) {
    throw Fmi::Exception::Trace(BCP, "Operation failed!");
  }
}

Json::Value parse_edr_metadata(const EDRProducerMetaData &epmd,
                               const EDRQuery &edr_query) {
  //  std::cout << "EDRQuery: " << edr_query << std::endl;

  Json::Value nulljson;

  if (edr_query.query_id == EDRQueryId::AllCollections ||
      edr_query.query_id == EDRQueryId::SpecifiedCollection)
    return parse_edr_metadata_collections(epmd, edr_query);

  if (edr_query.query_id == EDRQueryId::SpecifiedCollectionAllInstances ||
      edr_query.query_id == EDRQueryId::SpecifiedCollectionSpecifiedInstance)
    return parse_edr_metadata_instances(epmd, edr_query);

  return nulljson;
}

EDRProducerMetaData
get_edr_metadata_grid(const std::string &producer,
		      const Engine::Grid::Engine &gEngine,
		      EDRMetaData *emd = nullptr) {
  try {    
    EDRProducerMetaData epmd;

    std::vector<std::string> parts;
    boost::algorithm::split(parts, producer, boost::algorithm::is_any_of("."));
    
    auto producerName = parts[0];
    auto geometryId = (parts.size() > 1 ? parts[1] : "");
    auto levelId = (parts.size() > 2 ? parts[2] : "");

    auto grid_meta_data = gEngine.getEngineMetadata(producerName.c_str());    
    
    /*
    if(grid_meta_data.empty())
      throw Fmi::Exception::Trace(BCP, "Failed to get metadata for grid producer '" + producer + "'!");
    */
    
    // Iterate Grid metadata and add items into collection
    for (const auto &gmd : grid_meta_data) {
      
      if(!geometryId.empty())
		{
		  if(!boost::iequals(Fmi::to_string(gmd.geometryId), geometryId))
			continue;
		}
      if(!levelId.empty())
		{
		  if(!boost::iequals(Fmi::to_string(gmd.levelId), levelId))
			continue;
		}
      
      EDRMetaData producer_emd;
	  
      if (gmd.levels.size() > 1) {
		producer_emd.vertical_extent.vrs = (Fmi::to_string(gmd.levelId)+";"+gmd.levelName+";"+gmd.levelDescription);
		
	/*
	  1;GROUND;Ground or water surface;
	  2;PRESSURE;Pressure level;
	  3;HYBRID;Hybrid level;
	  4;ALTITUDE;Altitude;
	  5;TOP;Top of atmosphere;
	  6;HEIGHT;Height above ground in meters;
	  7;MEANSEA;Mean sea level;
	  8;ENTATM;Entire atmosphere;
	  9;GROUND_DEPTH;Layer between two depths below land surface;
	  10;DEPTH;Depth below some surface;
	  11;PRESSURE_DELTA;Level at specified pressure difference from ground to level;
	  12;MAXTHETAE;Level where maximum equivalent potential temperature is found;
	  13;HEIGHT_LAYER;Layer between two metric heights above ground;
	  14;DEPTH_LAYER;Layer between two depths below land surface;
	  15;ISOTHERMAL;Isothermal level, temperature in 1/100 K;
	  16;MAXWIND;Maximum wind level;
	 */
        producer_emd.vertical_extent.level_type = gmd.levelName;
        for (const auto &level : gmd.levels)
          producer_emd.vertical_extent.levels.push_back(
              Fmi::to_string(level));
      }
         

      producer_emd.spatial_extent.bbox_ymin = std::min(gmd.latlon_bottomLeft.y(), gmd.latlon_bottomRight.y());
      producer_emd.spatial_extent.bbox_xmax = std::max(gmd.latlon_bottomRight.x(), gmd.latlon_topRight.x());
      producer_emd.spatial_extent.bbox_ymax = std::max(gmd.latlon_topLeft.y(), gmd.latlon_topRight.y());
      producer_emd.temporal_extent.origin_time = boost::posix_time::from_iso_string(gmd.analysisTime);
      auto begin_iter = gmd.times.begin();
      auto end_iter = gmd.times.end();
      end_iter--;
      const auto& end_time = *end_iter;
      const auto& start_time = *begin_iter;
      begin_iter++;
      const auto& start_time_plus_one = *begin_iter;
      auto timestep_duration = (boost::posix_time::from_iso_string(start_time_plus_one) - boost::posix_time::from_iso_string(start_time));
      
      producer_emd.temporal_extent.start_time = boost::posix_time::from_iso_string(start_time);
      producer_emd.temporal_extent.end_time =  boost::posix_time::from_iso_string(end_time);
      producer_emd.temporal_extent.timestep = (timestep_duration.total_seconds() / 60);
      producer_emd.temporal_extent.timesteps= gmd.times.size();

      for (const auto &p : gmd.parameters) {
        auto parameter_name = p.parameterName;
        boost::algorithm::to_lower(parameter_name);
        producer_emd.parameter_names.insert(parameter_name);
        producer_emd.parameters.insert(std::make_pair(
						      parameter_name, edr_parameter(parameter_name, p.parameterDescription, p.parameterUnits)));
      }
      std::string producerId = (gmd.producerName + "." + Fmi::to_string(gmd.geometryId) + "." + Fmi::to_string(gmd.levelId));
      boost::algorithm::to_lower(producerId);
      
      epmd[producerId].push_back(producer_emd);      
    }
    
    if (!producer.empty() && emd) {
      const auto &emds = epmd.begin()->second;
      for (unsigned int i = 0; i < emds.size(); i++) {
        if (i == 0 || emd->temporal_extent.origin_time <
                          emds.at(i).temporal_extent.origin_time)
          *emd = emds.at(i);
      }
    }
    
    return epmd;
  } catch (...) {    
    throw Fmi::Exception::Trace(BCP, "Operation failed!");
  }
}

EDRProducerMetaData
get_edr_metadata_qd(const std::string &producer,
                    const Engine::Querydata::Engine &qEngine,
                    EDRMetaData *emd = nullptr) {
  try {
    Engine::Querydata::MetaQueryOptions opts;
    if (!producer.empty())
      opts.setProducer(producer);
    auto qd_meta_data = qEngine.getEngineMetadata(opts);

    /*
    if(qd_meta_data.empty())
      throw Fmi::Exception::Trace(BCP, "Failed to get metadata for querydata producer '" + producer + "'!");
    */
    
    EDRProducerMetaData epmd;

    if(qd_meta_data.empty())
      return epmd;

    // Iterate QEngine metadata and add items into collection
    for (const auto &qmd : qd_meta_data) {
      EDRMetaData producer_emd;

      if (qmd.levels.size() > 1) {
        if (qmd.levels.front().type == "PressureLevel")
          producer_emd.vertical_extent.vrs =
              "TODO: PARAMETRICCRS['WMO standard atmosphere layer "
              "0',PDATUM['Mean Sea Level',ANCHOR['1013.25 hPa at "
              "15\u00b0C']],CS[parametric,1§],AXIS['pressure "
              "(hPa)',up],PARAMETRICUNIT['Hectopascal',100.0";
        else if (qmd.levels.front().type == "HybridLevel")
          producer_emd.vertical_extent.vrs =
              "TODO: PARAMETRICCRS['WMO Hybrid "
              "Levels',CS[parametric,1],AXIS['Hybrid',up]]";
        producer_emd.vertical_extent.level_type = qmd.levels.front().type;
        for (const auto &item : qmd.levels)
          producer_emd.vertical_extent.levels.push_back(
              Fmi::to_string(item.value));
      }

      const auto &rangeLon = qmd.wgs84Envelope.getRangeLon();
      const auto &rangeLat = qmd.wgs84Envelope.getRangeLat();
      producer_emd.spatial_extent.bbox_xmin = rangeLon.getMin();
      producer_emd.spatial_extent.bbox_ymin = rangeLat.getMin();
      producer_emd.spatial_extent.bbox_xmax = rangeLon.getMax();
      producer_emd.spatial_extent.bbox_ymax = rangeLat.getMax();
      producer_emd.temporal_extent.origin_time = qmd.originTime;
      producer_emd.temporal_extent.start_time = qmd.firstTime;
      producer_emd.temporal_extent.end_time = qmd.lastTime;
      producer_emd.temporal_extent.timestep = qmd.timeStep;
      producer_emd.temporal_extent.timesteps= qmd.nTimeSteps;

      for (const auto &p : qmd.parameters) {
        auto parameter_name = p.name;
        boost::algorithm::to_lower(parameter_name);
        producer_emd.parameter_names.insert(parameter_name);
        producer_emd.parameters.insert(std::make_pair(
						      parameter_name, edr_parameter(p.name, p.description)));
      }
      epmd[qmd.producer].push_back(producer_emd);
    }

    if (!producer.empty() && emd) {
      const auto &emds = epmd.begin()->second;
      for (unsigned int i = 0; i < emds.size(); i++) {
        if (i == 0 || emd->temporal_extent.origin_time <
                          emds.at(i).temporal_extent.origin_time)
          *emd = emds.at(i);
      }
    }

    return epmd;
  } catch (...) {
    throw Fmi::Exception::Trace(BCP, "Operation failed!");
  }
}

#ifndef WITHOUT_OBSERVATION
EDRProducerMetaData get_edr_metadata_obs(const std::string &producer,
                                         Engine::Observation::Engine &obsEngine,
                                         EDRMetaData *emd = nullptr) {
  try {
    std::map<std::string, Engine::Observation::MetaData> observation_meta_data;

    std::set<std::string> producers;
    if (!producer.empty())
      producers.insert(producer);
    else
      producers = obsEngine.getValidStationTypes();

    for (const auto &prod : producers)
      observation_meta_data.insert(
          std::make_pair(prod, obsEngine.metaData(prod)));

    EDRProducerMetaData epmd;

    // Iterate Observation engine metadata and add item into collection
    std::vector<std::string> params{""};
    for (const auto &item : observation_meta_data) {
      const auto &producer = item.first;
      const auto &obs_md = item.second;

      EDRMetaData producer_emd;

      producer_emd.spatial_extent.bbox_xmin = obs_md.bbox.xMin;
      producer_emd.spatial_extent.bbox_ymin = obs_md.bbox.yMin;
      producer_emd.spatial_extent.bbox_xmax = obs_md.bbox.xMax;
      producer_emd.spatial_extent.bbox_ymax = obs_md.bbox.yMax;
      producer_emd.temporal_extent.origin_time =
          boost::posix_time::second_clock::universal_time();
      producer_emd.temporal_extent.start_time = obs_md.period.begin();
      producer_emd.temporal_extent.end_time = obs_md.period.last();
      producer_emd.temporal_extent.timestep = obs_md.timestep;
      producer_emd.temporal_extent.timesteps = (obs_md.period.length().minutes() / obs_md.timestep);

      for (const auto &p : obs_md.parameters) {
        params[0] = p;
        auto observedProperties =
            obsEngine.observablePropertyQuery(params, "en");
        std::string description = p;
        if (observedProperties->size() > 0)
          description = observedProperties->at(0).observablePropertyLabel;

        auto parameter_name = p;
        boost::algorithm::to_lower(parameter_name);
        producer_emd.parameter_names.insert(parameter_name);
        producer_emd.parameters.insert(
            std::make_pair(parameter_name, edr_parameter(p, description)));
      }

      epmd[producer].push_back(producer_emd);
    }

    if (!producer.empty() && emd) {
      const auto &emds = epmd.begin()->second;
      for (unsigned int i = 0; i < emds.size(); i++) {
        if (i == 0 || emd->temporal_extent.origin_time <
                          emds.at(i).temporal_extent.origin_time)
          *emd = emds.at(i);
      }
    }

    return epmd;
  } catch (...) {
    throw Fmi::Exception::Trace(BCP, "Operation failed!");
  }
}
#endif

Json::Value format_output_data_one_point(
    TS::OutputData &outputData, const EDRMetaData &emd,
    boost::optional<int> level,
    const std::vector<Spine::Parameter> &query_parameters) {
  try {

	//	std::cout << "format_output_data_one_point" << std::endl;

    Json::Value coverage;

    auto ranges = Json::Value(Json::ValueType::objectValue);
    auto coordinates = get_coordinates(outputData, query_parameters);
    std::set<std::string> timesteps;
    for (unsigned int i = 0; i < outputData.size(); i++) {
      const auto &outdata = outputData[i].second;
      // iterate columns (parameters)
      for (unsigned int j = 0; j < outdata.size(); j++) {
        auto parameter_name = query_parameters[j].name();
        boost::algorithm::to_lower(parameter_name);
		if(lon_lat_level_param(parameter_name))
          continue;

        auto json_param_object = Json::Value(Json::ValueType::objectValue);
        auto tsdata = outdata.at(j);
        auto values = Json::Value(Json::ValueType::arrayValue);
        unsigned int values_index = 0;

        TS::TimeSeriesPtr ts = *(boost::get<TS::TimeSeriesPtr>(&tsdata));
        if (i == 0) {
          coverage = add_prologue_one_point(
              level, emd.vertical_extent.level_type, coordinates.front().lon,
              coordinates.front().lat);
          coverage["parameters"] =
              get_edr_series_parameters(query_parameters, emd.parameters);
          json_param_object["type"] = Json::Value("NdArray");
          auto axis_names = Json::Value(Json::ValueType::arrayValue);
          axis_names[0] = Json::Value("t");
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

        for (unsigned int k = 0; k < ts->size(); k++) {
          const auto &timed_value = ts->at(k);
          add_value(timed_value, values, json_param_object["dataType"],
                    timesteps, values_index);
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
  } catch (...) {
    throw Fmi::Exception::Trace(BCP, "Operation failed!");
  }
}

Json::Value format_output_data_position(
    TS::OutputData &outputData, const EDRMetaData &emd,
    const std::vector<Spine::Parameter> &query_parameters) 
{
  try {

	//	std::cout << "format_output_data_position" << std::endl;

    Json::Value coverage_collection;

	unsigned int longitude_index;
	unsigned int latitude_index;
	boost::optional<unsigned int> level_index;
	auto last_param = query_parameters.back();
	auto last_param_name = last_param.name();
	boost::algorithm::to_lower(last_param_name);
	if(last_param_name == "level")
	  {
		level_index = (query_parameters.size()-1);
		latitude_index = (query_parameters.size()-2);
		longitude_index = (query_parameters.size()-3);
	  }
	else
	  {
		latitude_index = (query_parameters.size()-1);
		longitude_index = (query_parameters.size()-2);
	  }
	
	coverage_collection["type"] = Json::Value("CoverageCollection");
	coverage_collection["domainType"] = Json::Value("Point");
	coverage_collection["parameters"] = get_edr_series_parameters(query_parameters, emd.parameters);
	
    // Referencing x,y coordinates
    auto referencing = Json::Value(Json::ValueType::arrayValue);
    auto referencing_xy = Json::Value(Json::ValueType::objectValue);
    referencing_xy["coordinates"] = Json::Value(Json::ValueType::arrayValue);
    referencing_xy["coordinates"][0] = Json::Value("y");
    referencing_xy["coordinates"][1] = Json::Value("x");
    referencing_xy["system"] = Json::Value(Json::ValueType::objectValue);
    referencing_xy["system"]["type"] = Json::Value("GeographicCRS");
    referencing_xy["system"]["id"] =
        Json::Value("http://www.opengis.net/def/crs/EPSG/0/4326");

    // Referencing z coordinate
    auto referencing_z = Json::Value(Json::ValueType::objectValue);
    if (level_index) {
      referencing_z["coordinates"][0] = Json::Value("z");
      referencing_z["system"]["type"] = Json::Value("VerticalCRS");
      referencing_z["system"]["id"] = Json::Value("TODO: " +  emd.vertical_extent.level_type);
    }

    auto referencing_time = Json::Value(Json::ValueType::objectValue);
    referencing_time["coordinates"] = Json::Value(Json::ValueType::arrayValue);
    referencing_time["coordinates"][0] = Json::Value("t");
    referencing_time["system"] = Json::Value(Json::ValueType::objectValue);
    referencing_time["system"]["type"] = Json::Value("TemporalCRS");
    referencing_time["system"]["calendar"] = Json::Value("Georgian");
    referencing[0] = referencing_xy;
    if (level_index) {
      referencing[1] = referencing_z;
      referencing[2] = referencing_time;
    } else {
      referencing[1] = referencing_time;
    }

    coverage_collection["referencing"] = referencing;

    auto coverages = Json::Value(Json::ValueType::arrayValue);

	unsigned int coverages_index = 0;
	//    std::set<std::string> timesteps;
    for (unsigned int i = 0; i < outputData.size(); i++) 
	  {
		const auto &outdata = outputData[i].second;
		// iterate columns (parameters)
		for (unsigned int j = 0; j < outdata.size(); j++) 
		  {
			auto parameter_name = query_parameters[j].name();
			boost::algorithm::to_lower(parameter_name);
			if(lon_lat_level_param(parameter_name))
			  continue;

			auto tsdata = outdata.at(j);
			auto tslon = outdata.at(longitude_index);
			auto tslat = outdata.at(latitude_index);
			TS::TimeSeriesPtr ts_data = *(boost::get<TS::TimeSeriesPtr>(&tsdata));
			TS::TimeSeriesPtr ts_lon = *(boost::get<TS::TimeSeriesPtr>(&tslon));
			TS::TimeSeriesPtr ts_lat = *(boost::get<TS::TimeSeriesPtr>(&tslat));
			TS::TimeSeriesPtr ts_level = nullptr;
			if(level_index)
			  {
				auto tslevel = outdata.at(*level_index);
				ts_level = *(boost::get<TS::TimeSeriesPtr>(&tslevel));
			  }
			
			unsigned int values_index = 0;
			for(unsigned int k = 0; k < ts_data->size(); k++)
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
			
				//const auto& data_value = ts_data->at(k);
				const auto& lon_value = ts_lon->at(k);
				const auto& lat_value = ts_lat->at(k);
				auto& x_values = x_object["values"];
				auto& y_values = y_object["values"];
				Json::Value data_type;
				add_value(lon_value, x_values, data_type, values_index);
				add_value(lat_value, y_values, data_type, values_index);
				if(ts_level)
				  {
					const auto& level_value = ts_level->at(k);
					auto& z_values = z_object["values"];
					add_value(level_value, z_values, data_type, values_index);
				  }
				auto& timestamp_values = timestamp_object["values"];
				timestamp_values[values_index] = Json::Value(boost::posix_time::to_iso_extended_string(lon_value.time.utc_time())+"Z");

				auto domain_object_axes = Json::Value(Json::ValueType::objectValue);
				domain_object_axes["x"] = x_object;
				domain_object_axes["y"] = y_object;
				if(ts_level)
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
				if(ts_level)
				  shape_object[3] = Json::Value(1);
				auto axis_object = Json::Value(Json::ValueType::arrayValue);
				axis_object[0] = Json::Value("t");
				axis_object[1] = Json::Value("x");
				axis_object[2] = Json::Value("y");
				if(ts_level)
				  axis_object[3] = Json::Value("z");
				auto values_array = Json::Value(Json::ValueType::arrayValue);
				const auto& data_value = ts_data->at(k);
				add_value(data_value, values_array, data_type, values_index);
				parameter_object["dataType"] = data_type;

				//				values_array[0] = Json::Value(1.5784); // todo
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
  } catch (...) {
    throw Fmi::Exception::Trace(BCP, "Operation failed!");
  }
}

Json::Value format_output_data_multi_point(
    TS::OutputData &outputData, const EDRMetaData &emd,
    boost::optional<int> level,
    const std::vector<Spine::Parameter> &query_parameters) {
  try {
    Json::Value coverage;

	//	  std::cout << "format_output_data_multi_point" << std::endl;

    auto ranges = Json::Value(Json::ValueType::objectValue);
    auto coordinates = get_coordinates(outputData, query_parameters);
    std::set<std::string> timesteps;
    for (unsigned int i = 0; i < outputData.size(); i++) {
      const auto &outdata = outputData[i].second;
      // iterate columns (parameters)
      for (unsigned int j = 0; j < outdata.size(); j++) {
        auto parameter_name = parse_parameter_name(query_parameters[j].name());	
		if(lon_lat_level_param(parameter_name))
          continue;

        auto json_param_object = Json::Value(Json::ValueType::objectValue);
        auto tsdata = outdata.at(j);
        auto values = Json::Value(Json::ValueType::arrayValue);
        unsigned int values_index = 0;

        TS::TimeSeriesGroupPtr tsg =
            *(boost::get<TS::TimeSeriesGroupPtr>(&tsdata));

        for (const auto &llts : *tsg) {
          for (unsigned int k = 0; k < llts.timeseries.size(); k++) {
            const auto ts = llts.timeseries;
            if (i == 0) {
              coverage = add_prologue_multi_point(
                  level, emd.vertical_extent.level_type, coordinates);
              coverage["parameters"] =
                  get_edr_series_parameters(query_parameters, emd.parameters);
              json_param_object["type"] = Json::Value("NdArray");
              auto shape = Json::Value(Json::ValueType::arrayValue);
              shape[0] = Json::Value(ts.size());
              auto axis_names = Json::Value(Json::ValueType::arrayValue);
              axis_names[0] = Json::Value("t");
              if (coordinates.size() > 1) {
                axis_names[1] = Json::Value("composite");
                if (level)
                  axis_names[2] = Json::Value("z");
                shape[1] = Json::Value(coordinates.size());
                if (level)
                  shape[2] = Json::Value(1);
              } else {
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
            add_value(timed_value, values, json_param_object["dataType"],
                      timesteps, values_index);

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
  } catch (...) {
    throw Fmi::Exception::Trace(BCP, "Operation failed!");
  }
}

Json::Value format_coverage_collection_point(const DataPerParameter& dpp, const EDRMetaData &emd, const std::vector<Spine::Parameter> &query_parameters)
{
  try 
	{
	  //	  std::cout << "format_coverage_collection_point" << std::endl;

	  Json::Value coverage_collection;

	  if(dpp.empty())
		return coverage_collection;

	  bool levels_present = false;
	  auto coverages = Json::Value(Json::ValueType::arrayValue);
	  for(const auto& dpp_item : dpp)
		{
		  const auto& parameter_name = dpp_item.first;
		  const auto& dpl = dpp_item.second;
		  
		  for(const auto& dpl_item : dpl)
			{
			  auto level = dpl_item.first;
			  // Levels present in some item
			  levels_present = (levels_present || (level != std::numeric_limits<double>::max()));

			  auto values = dpl_item.second;
			  auto data_values = Json::Value(Json::ValueType::arrayValue);
			  auto time_coord_values = Json::Value(Json::ValueType::arrayValue);
			  for(unsigned int i = 0; i < values.size(); i++)
				{
				  const auto& value = values.at(i);
				  auto coverage = Json::Value(Json::ValueType::objectValue);
				  coverage["type"] = Json::Value("Coverage");
				  auto domain = Json::Value(Json::ValueType::objectValue);
				  domain["type"] = Json::Value("Domain");
				  auto domain_axes = Json::Value(Json::ValueType::objectValue);
				  auto domain_axes_x = Json::Value(Json::ValueType::objectValue);
				  domain_axes_x["values"] = Json::Value(Json::ValueType::arrayValue);
				  auto data_type = Json::Value(Json::ValueType::objectValue);
				  domain_axes_x["values"][0] = Json::Value(value.lon);				  
				  auto domain_axes_y = Json::Value(Json::ValueType::objectValue);
				  domain_axes_y["values"] = Json::Value(Json::ValueType::arrayValue);
				  domain_axes_y["values"][0] = Json::Value(value.lat);				  				  
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
				  if(value.value)
					{
					  values[0] = Json::Value(*value.value);
					  range_item["dataType"] = Json::Value("float");
					}
				  else
					{
					  values[0] = Json::Value();
					  range_item["dataType"] = Json::Value("float");
					}
				  range_item["values"] = values;
				  auto ranges = Json::Value(Json::ValueType::objectValue);
				  ranges[parameter_name] = range_item;
				  coverage["ranges"] = ranges;
				  coverages[coverages.size()] = coverage;
				}
			}
		}
	  coverage_collection = add_prologue_coverage_collection(emd, query_parameters, levels_present, "Point");
	  coverage_collection["coverages"] = coverages;
	  
	  return coverage_collection;
	} 
  catch (...) 
	{
	  throw Fmi::Exception::Trace(BCP, "Operation failed!");
	}
}

Json::Value format_coverage_collection_trajectory(const DataPerParameter& dpp, const EDRMetaData &emd, const std::vector<Spine::Parameter> &query_parameters)
{
  try 
	{
	  //	  std::cout << "format_coverage_collection_trajectory" << std::endl;

	  Json::Value coverage_collection;

	  if(dpp.empty())
		return coverage_collection;

	  bool levels_present = false;
	  auto coverages = Json::Value(Json::ValueType::arrayValue);
	  for(const auto& dpp_item : dpp)
		{
		  const auto& parameter_name = dpp_item.first;
		  const auto& dpl = dpp_item.second;
		  
		  for(const auto& dpl_item : dpl)
			{
			  auto level = dpl_item.first;
			  // Levels present in some item
			  levels_present = (levels_present || (level != std::numeric_limits<double>::max()));

			  auto values = dpl_item.second;
			  auto data_values = Json::Value(Json::ValueType::arrayValue);
			  auto time_coord_values = Json::Value(Json::ValueType::arrayValue);
			  for(unsigned int i = 0; i < values.size(); i++)
				{
				  const auto& value = values.at(i);
				  auto time_coord_value = Json::Value(Json::ValueType::arrayValue);
				  time_coord_value[0] = Json::Value(value.time);
				  time_coord_value[1] = Json::Value(value.lon);
				  time_coord_value[2] = Json::Value(value.lat);
				  if(levels_present)
					time_coord_value[3] = Json::Value(level);				  
				  time_coord_values[i] = time_coord_value;
				  if(value.value)
					data_values[i] = Json::Value(*value.value);
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
			  if(levels_present)
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
	  coverage_collection = add_prologue_coverage_collection(emd, query_parameters, levels_present, "Trajectory");
	  coverage_collection["coverages"] = coverages;
	  
	  return coverage_collection;
	} 
  catch (...) 
	{
	  throw Fmi::Exception::Trace(BCP, "Operation failed!");
	}
}


DataPerParameter get_data_per_parameter(
    TS::OutputData &outputData, const EDRMetaData &emd,
    const std::set<int>& levels,
	const CoordinateFilter& coordinate_filter,
    const std::vector<Spine::Parameter> &query_parameters) 
{
  try 
	{	  
	  DataPerParameter dpp;

	  //	  std::cout << "get_output_data_per_parameter" << std::endl;
	  
	  bool levels_present = (levels.size() > 0);

	  // Get indexes of longitude, latitude, level
	  unsigned int longitude_index;
	  unsigned int latitude_index;
	  unsigned int level_index;
	  auto last_param = query_parameters.back();
	  auto last_param_name = last_param.name();
	  boost::algorithm::to_lower(last_param_name);
	  if(levels_present)
		{
		  level_index = (query_parameters.size()-1);
		  latitude_index = (query_parameters.size()-2);
		  longitude_index = (query_parameters.size()-3);
		}
	  else
		{
		  latitude_index = (query_parameters.size()-1);
		  longitude_index = (query_parameters.size()-2);
		}
	  
	  for (unsigned int i = 0; i < outputData.size(); i++) {
		const auto &outdata = outputData[i].second;
		
		// iterate columns (parameters)
		for (unsigned int j = 0; j < outdata.size(); j++) {
		  auto parameter_name = parse_parameter_name(query_parameters[j].name());
		  DataPerLevel dpl;
		  
		  if(lon_lat_level_param(parameter_name))
			continue;
		  
		  auto tsdata = outdata.at(j);			  			  
		  auto tslon = outdata.at(longitude_index);
		  auto tslat = outdata.at(latitude_index);
		  TS::TimeSeriesGroupPtr tsg_data = *(boost::get<TS::TimeSeriesGroupPtr>(&tsdata));
		  TS::TimeSeriesGroupPtr tsg_lon = *(boost::get<TS::TimeSeriesGroupPtr>(&tslon));
		  TS::TimeSeriesGroupPtr tsg_lat = *(boost::get<TS::TimeSeriesGroupPtr>(&tslat));
		  TS::TimeSeriesGroupPtr tsg_level = nullptr;
		  if(levels_present)
			{
			  auto tslevel = outdata.at(level_index);
			  tsg_level = *(boost::get<TS::TimeSeriesGroupPtr>(&tslevel));
			}
		  
		  unsigned int levels_index = 0;
		  for (unsigned int k = 0; k < tsg_data->size(); k++)
			{
			  const auto& llts_data = tsg_data->at(k);
			  const auto& llts_lon = tsg_lon->at(k);
			  const auto& llts_lat = tsg_lat->at(k);
			  
			  for (unsigned int l = 0; l < llts_data.timeseries.size(); l++) 
				{
				  const auto &data_value = llts_data.timeseries.at(l);
				  const auto &lon_value = llts_lon.timeseries.at(l);
				  const auto &lat_value = llts_lat.timeseries.at(l);
				  
				  time_coord_value tcv;
				  tcv.lon = *(boost::get<double>(&lon_value.value));;
				  tcv.lat = *(boost::get<double>(&lat_value.value));
				  tcv.time = (boost::posix_time::to_iso_extended_string(data_value.time.utc_time())+"Z");
				  
				  double level = std::numeric_limits<double>::max();
				  if (levels_present) 
					{
					  const auto& llts_level = tsg_level->at(k);
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
				  if(data_value.value != TS::None())
					tcv.value = *(boost::get<double>(&data_value.value));
				  bool accept = coordinate_filter.accept(tcv.lon, tcv.lat, level, data_value.time.utc_time());
				  if(accept)
					dpl[level].push_back(tcv);
				  if (levels_present) 
					{
					  levels_index++;
					  if(levels_index >= tsg_level->size())
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
    TS::OutputData &outputData, const EDRMetaData &emd,
    const std::set<int>& levels,
	const CoordinateFilter& coordinate_filter,
    const std::vector<Spine::Parameter> &query_parameters,
	EDRQueryType query_type)

{
  try
	{
	  if(outputData.empty())
		Json::Value();
	  
	  auto dpp =  get_data_per_parameter(outputData, emd, levels, coordinate_filter, query_parameters);

	  if(query_type == EDRQueryType::Trajectory)
		return format_coverage_collection_trajectory(dpp, emd, query_parameters);
	  else
		return format_coverage_collection_point(dpp, emd, query_parameters);
	} 
  catch (...) 
	{
    throw Fmi::Exception::Trace(BCP, "Operation failed!");
  }
}
} // namespace


Json::Value
formatOutputData(TS::OutputData &outputData, const EDRMetaData &emd,
				 EDRQueryType query_type,
                 const std::set<int>& levels,
				 const CoordinateFilter& coordinate_filter,
                 const std::vector<Spine::Parameter> &query_parameters) {
  try {
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
		if(levels.size() <= 1)
		  {
			boost::optional<int> level;
			if(levels.size() == 1)
			  level = *(levels.begin());
			return format_output_data_one_point(outputData, emd, level, query_parameters);
		  }

		// More than one level
		return format_output_data_position(outputData, emd, query_parameters);
	  }
	else if (boost::get<TS::TimeSeriesVectorPtr>(&tsdata_first)) 
	  {
		if(outdata_first.size() > 1)
		  std::cout << "formatOutputData - TS::TimeSeriesVectorPtr - Can do nothing -> report error! " << std::endl;
		std::vector<TS::TimeSeriesData> tsd;
		TS::TimeSeriesVectorPtr tsv = *(boost::get<TS::TimeSeriesVectorPtr>(&tsdata_first));
		for(unsigned int i = 0; i < tsv->size(); i++)
		  {
			TS::TimeSeriesPtr tsp(new TS::TimeSeries(tsv->at(i)));
			tsd.push_back(tsp);
		  }
		TS::OutputData od;
		od.push_back(std::make_pair("_obs_", tsd));
		// Zero or one levels
		if(levels.size() <= 1)
		  {
			boost::optional<int> level;
			if(levels.size() == 1)
			  level = *(levels.begin());
			return format_output_data_one_point(od, emd, level, query_parameters);
		  }
		// More than one level
		return format_output_data_position(od, emd, query_parameters);
    } 
	else if (boost::get<TS::TimeSeriesGroupPtr>(&tsdata_first)) 
	  {
		return format_output_data_coverage_collection(outputData, emd, levels, coordinate_filter, query_parameters, query_type);
	  }

    return empty_result;
  } catch (...) {
    throw Fmi::Exception::Trace(BCP, "Operation failed!");
  }
}

EDRMetaData getProducerMetaData(const std::string &producer,
                                const Engine::Grid::Engine &gEngine) {
  try {
    EDRMetaData emd;

    if (!producer.empty())
      get_edr_metadata_grid(producer, gEngine, &emd);

    return emd;
  } catch (...) {
    throw Fmi::Exception::Trace(BCP, "Operation failed!");
  }
}

EDRMetaData getProducerMetaData(const std::string &producer,
                                const Engine::Querydata::Engine &qEngine) {
  try {
    EDRMetaData emd;

    if (!producer.empty())
      get_edr_metadata_qd(producer, qEngine, &emd);

    return emd;
  } catch (...) {
    throw Fmi::Exception::Trace(BCP, "Operation failed!");
  }
}

#ifndef WITHOUT_OBSERVATION
EDRMetaData getProducerMetaData(const std::string &producer,
                                Engine::Observation::Engine &obsEngine) {
  try {
    EDRMetaData emd;

    if (!producer.empty())
      get_edr_metadata_obs(producer, obsEngine, &emd);

    return emd;
  } catch (...) {
    throw Fmi::Exception::Trace(BCP, "Operation failed!");
  }
}
#endif

Json::Value processEDRMetaDataQuery(const EDRQuery &edr_query,
                                    const Engine::Grid::Engine &gEngine) {
  try {
    Json::Value result;

    const std::string &producer = edr_query.collection_id;
    
    auto grid_metadata = get_edr_metadata_grid(producer, gEngine);

    if (producer.empty()) {
      auto edr_metadata_grid = parse_edr_metadata(grid_metadata, edr_query);

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
      result["collections"] = edr_metadata_grid;
    } else {
      result = parse_edr_metadata(grid_metadata, edr_query);
    }

    //    auto grid_metadata = 
    
    return result;
  } catch (...) {
    throw Fmi::Exception::Trace(BCP, "Operation failed!");
  }
}

Json::Value processEDRMetaDataQuery(const EDRQuery &edr_query,
                                    const Engine::Querydata::Engine &qEngine,
                                    const Engine::Grid::Engine &gEngine) {  
  try {
    const std::string &producer = edr_query.collection_id;

    Json::Value result;
    
    if (producer.empty()) {
      auto qd_metadata = get_edr_metadata_qd(producer, qEngine);
      auto grid_metadata = get_edr_metadata_grid(producer, gEngine);
      auto edr_metadata = parse_edr_metadata(qd_metadata, edr_query);
      auto edr_metadata_grid = parse_edr_metadata(grid_metadata, edr_query);
      // Append grid engine metadata after QEngine metadata
      for (const auto &item : edr_metadata_grid)
        edr_metadata.append(item);

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
    } else {

      EDRProducerMetaData epmd = get_edr_metadata_qd(producer, qEngine);
      if(epmd.empty())
        epmd = get_edr_metadata_grid(producer, gEngine);
      result = parse_edr_metadata(epmd, edr_query);
    }

    return result;
  } catch (...) {
    throw Fmi::Exception::Trace(BCP, "Operation failed!");
  }
}

Json::Value processEDRMetaDataQuery(const EDRQuery &edr_query,
                                    const Engine::Querydata::Engine &qEngine) {
  try {
    const std::string &producer = edr_query.collection_id;

    Json::Value result;
    auto qd_metadata = get_edr_metadata_qd(producer, qEngine);
    if (producer.empty()) {
      auto edr_metadata_qd = parse_edr_metadata(qd_metadata, edr_query);

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
      result["collections"] = edr_metadata_qd;
    } else {
      result = parse_edr_metadata(qd_metadata, edr_query);
    }

    return result;
  } catch (...) {
    throw Fmi::Exception::Trace(BCP, "Operation failed!");
  }
}

#ifndef WITHOUT_OBSERVATION
Json::Value processEDRMetaDataQuery(const EDRQuery &edr_query,
                                    const Engine::Querydata::Engine &qEngine,
                                    const Engine::Grid::Engine &gEngine,
                                    Engine::Observation::Engine &obsEngine) {
  try {
    Json::Value result;
    const std::string &producer = edr_query.collection_id;

    if (producer.empty()) {
      auto qd_metadata = get_edr_metadata_qd(producer, qEngine);
      auto grid_metadata = get_edr_metadata_grid(producer, gEngine);
      auto obs_metadata = get_edr_metadata_obs(producer, obsEngine);
      auto edr_metadata = parse_edr_metadata(qd_metadata, edr_query);
      auto edr_metadata_grid = parse_edr_metadata(grid_metadata, edr_query);
      auto edr_metadata_obs = parse_edr_metadata(obs_metadata, edr_query);
      // Append grid engine metadata after QEngine metadata
      for (const auto &item : edr_metadata_grid)
        edr_metadata.append(item);
      // Append observation engine metadata in the end
      for (const auto &item : edr_metadata_obs)
        edr_metadata.append(item);

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
    } else {
      auto obs_producers = obsEngine.getValidStationTypes();
      EDRProducerMetaData epmd;
      if (obs_producers.find(producer) != obs_producers.end())
        epmd = get_edr_metadata_obs(producer, obsEngine);
      else
        epmd = get_edr_metadata_qd(producer, qEngine);
      if(epmd.empty())
        epmd = get_edr_metadata_grid(producer, gEngine);

      result = parse_edr_metadata(epmd, edr_query);
    }

    return result;
  } catch (...) {
    throw Fmi::Exception::Trace(BCP, "Operation failed!");
  }
}

Json::Value processEDRMetaDataQuery(const EDRQuery &edr_query,
                                    const Engine::Querydata::Engine &qEngine,
                                    Engine::Observation::Engine &obsEngine) {
  try {
    Json::Value result;
    const std::string &producer = edr_query.collection_id;

    if (producer.empty()) {
      auto qd_metadata = get_edr_metadata_qd(producer, qEngine);
      auto obs_metadata = get_edr_metadata_obs(producer, obsEngine);
      auto edr_metadata = parse_edr_metadata(qd_metadata, edr_query);
      auto edr_metadata_obs = parse_edr_metadata(obs_metadata, edr_query);
      // Append observation engine metadata after QEngine metadata
      for (const auto &item : edr_metadata_obs)
        edr_metadata.append(item);

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
    } else {
      auto obs_producers = obsEngine.getValidStationTypes();
      EDRProducerMetaData epmd;
      if (obs_producers.find(producer) != obs_producers.end())
        epmd = get_edr_metadata_obs(producer, obsEngine);
      else
        epmd = get_edr_metadata_qd(producer, qEngine);

      result = parse_edr_metadata(epmd, edr_query);
    }

    return result;
  } catch (...) {
    throw Fmi::Exception::Trace(BCP, "Operation failed!");
  }
}
#endif

Json::Value processEDRMetaDataQuery(const Spine::LocationList& locations)
{
  try {
    Json::Value result;
	
	result["type"] = Json::Value("FeatureCollection");
	auto features = Json::Value(Json::ValueType::arrayValue);

	for(const auto& loc : locations)
	  {
		auto feature = Json::Value(Json::ValueType::objectValue);
		feature["type"] = Json::Value("Feature");
		feature["id"] = Json::Value(loc->geoid);
		auto geometry = Json::Value(Json::ValueType::objectValue);
		geometry["type"] = Json::Value("Point");
		auto coordinates = Json::Value(Json::ValueType::arrayValue);
		coordinates[0] = Json::Value(loc->longitude);
		coordinates[1] = Json::Value(loc->latitude);
		geometry["coordinates"] = coordinates;
		feature["geometry"] = geometry;
		features[features.size()] = feature;
	  }
	result["features"] = features;

    return result;
  } catch (...) {
    throw Fmi::Exception::Trace(BCP, "Operation failed!");
  }
}

} // namespace CoverageJson
} // namespace EDR
} // namespace Plugin
} // namespace SmartMet
