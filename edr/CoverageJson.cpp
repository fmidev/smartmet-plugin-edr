#include "CoverageJson.h"
#include <engines/querydata/Engine.h>
#include <macgyver/Exception.h>
#include <macgyver/StringConversion.h>
#include <timeseries/TimeSeriesOutput.h>
#ifndef WITHOUT_OBSERVATION
#include <engines/observation/Engine.h>
#include <engines/observation/ObservableProperty.h>
#endif

namespace SmartMet {
namespace Plugin {
namespace EDR {
namespace CoverageJson {
namespace {
std::vector<TS::LonLat>
get_coordinates(const TS::OutputData &outputData,
                const std::vector<Spine::Parameter> &query_parameters) {
  try {
    std::vector<TS::LonLat> ret;

    if (outputData.empty())
      return ret;

    const auto &outdata = outputData.at(0).second;
    std::vector<double> longitude_vector;
    std::vector<double> latitude_vector;

    std::set<std::string> lonlat_strings;
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

          if (param_name == "longitude")
            longitude_vector.push_back(*(boost::get<double>(&tv.value)));
          else
            latitude_vector.push_back(*(boost::get<double>(&tv.value)));
        }
      } else if (boost::get<TS::TimeSeriesVectorPtr>(&tsdata)) {
        std::cout
            << "get_coordinates -> TS::TimeSeriesVectorPtr - Can do nothing"
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
      auto parameter_name = p.name();
      boost::algorithm::to_lower(parameter_name);
      if (parameter_name == "longitude" || parameter_name == "latitude")
        continue;
      const auto &edr_parameter = edr_parameters.at(parameter_name);

      auto parameter = Json::Value(Json::ValueType::objectValue);
      parameter["type"] = Json::Value("Parameter");
      parameter["description"] = Json::Value(Json::ValueType::objectValue);
      parameter["description"]["en"] = Json::Value(edr_parameter.description);
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
          Json::Value(edr_parameter.description);
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
    timesteps.insert(boost::posix_time::to_iso_extended_string(t.utc_time()));
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
        ("https://smartmet.fmi.fi/edr/collections/" + producer + "/instances"));
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
           boost::posix_time::to_iso_string(emd.temporal_extent.origin_time));
      title += (" Starttime: " + boost::posix_time::to_iso_string(
                                     emd.temporal_extent.start_time));
      title += (" Endtime: " +
                boost::posix_time::to_iso_string(emd.temporal_extent.end_time));
      if (emd.temporal_extent.timestep)
        title +=
            (" Timestep: " + Fmi::to_string(*emd.temporal_extent.timestep));
      instance["title"] = Json::Value(title);

      // Links
      auto instance_links = Json::Value(Json::ValueType::arrayValue);
      auto instance_link_item = Json::Value(Json::ValueType::objectValue);
      instance_link_item["href"] =
          Json::Value(("https://smartmet.fmi.fi/edr/collections/" + producer +
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
      auto temporal = Json::Value(Json::ValueType::objectValue);
      auto temporal_interval = Json::Value(Json::ValueType::arrayValue);
      auto trs = Json::Value("TIMECRS[\"DateTime\",TDATUM[\"Gregorian "
                             "Calendar\"],CS[TemporalDateTime,1],AXIS[\"Time "
                             "(T)\",future]"); // What
                                               // this
                                               // should
                                               // be
      auto interval_string =
          (boost::posix_time::to_iso_string(emd.temporal_extent.start_time) +
           "/" +
           boost::posix_time::to_iso_string(emd.temporal_extent.end_time));
      if (emd.temporal_extent.timestep)
        interval_string +=
            ("/" + Fmi::to_string(*emd.temporal_extent.timestep) + "M");
      temporal_interval[0] = Json::Value(interval_string);
      temporal["interval"] = temporal_interval;
      temporal["trs"] = trs;
      extent["temporal"] = temporal;
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

      auto output_formats = Json::Value(Json::ValueType::arrayValue);
      output_formats[0] = Json::Value("CoverageJSON");
      instance["output_formats"] = output_formats;
      auto crs = Json::Value(Json::ValueType::arrayValue);
      crs[0] = Json::Value("EPSG:4326");
      instance["crs"] = crs;

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
          Json::Value(("https://smartmet.fmi.fi/edr/collections/" + producer));
      if (edr_query.query_id == EDRQueryId::SpecifiedCollection)
        collection_link["rel"] = Json::Value("self");
      else
        collection_link["rel"] = Json::Value("data");
      collection_link["type"] = Json::Value("application/json");
      collection_link["title"] = Json::Value("Collection metadata in JSON");

      auto links = Json::Value(Json::ValueType::arrayValue);
      links[0] = collection_link;
      // Add instance links if several present
      if (emds.size() > 1) {
        auto instance_link = Json::Value(Json::ValueType::objectValue);
        instance_link["href"] =
            Json::Value(("https://smartmet.fmi.fi/edr/collections/" + producer +
                         "/instances"));
        instance_link["rel"] = Json::Value("data");
        instance_link["type"] = Json::Value("application/json");
        instance_link["title"] = Json::Value("Instance metadata in JSON");
        links[1] = instance_link;
        /*
  for(const auto& emd : emds)
        {
          auto instance_link = Json::Value(Json::ValueType::objectValue);
          auto instance_id =
  Fmi::to_iso_string(emd.temporal_extent.origin_time); instance_link["href"] =
  Json::Value(("https://smartmet.fmi.fi/edr/collections/" + producer + "/" +
  instance_id)); instance_link["rel"] = Json::Value("data");
          instance_link["type"] = Json::Value("application/json");
          instance_link["title"] = Json::Value("Instance metadata in JSON");
          links[links.size()] = instance_link;
        }
        */
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
      spatial["crs"] = Json::Value("EPSG:4326");
      extent["spatial"] = spatial;
      // Temporal (optional)
      auto temporal = Json::Value(Json::ValueType::objectValue);
      auto temporal_interval = Json::Value(Json::ValueType::arrayValue);
      auto trs = Json::Value("TIMECRS[\"DateTime\",TDATUM[\"Gregorian "
                             "Calendar\"],CS[TemporalDateTime,1],AXIS[\"Time "
                             "(T)\",future]"); // What
                                               // this
                                               // should
                                               // be
      auto interval_string = (boost::posix_time::to_iso_string(
                                  collection_emd.temporal_extent.start_time) +
                              "/" +
                              boost::posix_time::to_iso_string(
                                  collection_emd.temporal_extent.end_time));
      if (collection_emd.temporal_extent.timestep)
        interval_string +=
            ("/" + Fmi::to_string(*collection_emd.temporal_extent.timestep) +
             "M");
      temporal_interval[0] = Json::Value(interval_string);
      temporal["interval"] = temporal_interval;
      temporal["trs"] = trs;
      extent["temporal"] = temporal;
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

      // Data queries (optional)
      // ...

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
get_edr_metadata_qd(const std::string &producer,
                    const Engine::Querydata::Engine &qEngine,
                    EDRMetaData *emd = nullptr) {
  try {
    Engine::Querydata::MetaQueryOptions opts;
    if (!producer.empty())
      opts.setProducer(producer);
    auto qd_meta_data = qEngine.getEngineMetadata(opts);

    EDRProducerMetaData epmd;

    // Iterate QEngine metadata and add items into collection
    for (const auto &qmd : qd_meta_data) {
      EDRMetaData producer_emd;

      if (qmd.levels.size() > 1) {
        edr_vertical_extent eve;
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
        if (parameter_name == "longitude" || parameter_name == "latitude")
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

Json::Value format_output_data_multi_point(
    TS::OutputData &outputData, const EDRMetaData &emd,
    boost::optional<int> level,
    const std::vector<Spine::Parameter> &query_parameters) {
  try {
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
        if (parameter_name == "longitude" || parameter_name == "latitude")
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

Json::Value format_output_data_coverage_collection(
    TS::OutputData &outputData, const EDRMetaData &emd,
    boost::optional<int> level,
    const std::vector<Spine::Parameter> &query_parameters) {
  try {
    Json::Value coverage_collection;

    auto coverages = Json::Value(Json::ValueType::arrayValue);
    auto coordinates = get_coordinates(outputData, query_parameters);

    /*
    std::cout << "DATA:\n";
    for(auto item : outputData)
          {
            std::cout << "key, item.first: "<< item.first<< std::endl;
            unsigned int counter = 0;
            for(auto item2 : item.second)
                  {
                    std::cout << "counter #" << counter++ << std::endl;
                    TS::operator<<(std::cout, item2) << std::endl;
                  }
          }

    std::cout << "COORDINATES:\n";
    for(const auto c : coordinates)
          std::cout << c.lon << ", " << c.lat << std::endl;
*/

    std::set<std::string> timesteps;
    unsigned int coverage_index = 0;
    for (unsigned int i = 0; i < outputData.size(); i++) {
      const auto &outdata = outputData[i].second;
      // iterate columns (parameters)
      for (unsigned int j = 0; j < outdata.size(); j++) {
        auto parameter_name = query_parameters[j].name();
        boost::algorithm::to_lower(parameter_name);
        if (parameter_name == "longitude" || parameter_name == "latitude")
          continue;

        auto tsdata = outdata.at(j);
        auto values = Json::Value(Json::ValueType::arrayValue);

        TS::TimeSeriesGroupPtr tsg =
            *(boost::get<TS::TimeSeriesGroupPtr>(&tsdata));

        unsigned int coordinate_index = 0;
        for (const auto &llts : *tsg) {
          for (unsigned int k = 0; k < llts.timeseries.size(); k++) {
            const auto ts = llts.timeseries;
            const auto &timed_value = ts.at(k);

            auto coverage = Json::Value(Json::ValueType::objectValue);
            coverage["type"] = Json::Value("Coverage");

            auto coverage_domain = Json::Value(Json::ValueType::objectValue);
            coverage_domain["type"] = Json::Value("Domain");
            auto domain_axes = Json::Value(Json::ValueType::objectValue);
            auto domain_axes_x = Json::Value(Json::ValueType::objectValue);
            domain_axes_x["values"] = Json::Value(Json::ValueType::arrayValue);
            domain_axes_x["values"][0] =
                Json::Value(coordinates.at(coordinate_index).lon);
            auto domain_axes_y = Json::Value(Json::ValueType::objectValue);
            domain_axes_y["values"] = Json::Value(Json::ValueType::arrayValue);
            domain_axes_y["values"][0] =
                Json::Value(coordinates.at(coordinate_index).lat);
            auto domain_axes_t = Json::Value(Json::ValueType::objectValue);
            domain_axes_t["values"] = Json::Value(Json::ValueType::arrayValue);
            domain_axes_t["values"][0] =
                Json::Value(boost::posix_time::to_iso_extended_string(
                    timed_value.time.utc_time()));
            domain_axes["x"] = domain_axes_x;
            domain_axes["y"] = domain_axes_y;
            if (level) {
              auto domain_axes_z = Json::Value(Json::ValueType::objectValue);
              domain_axes_z["values"] =
                  Json::Value(Json::ValueType::arrayValue);
              domain_axes_z["values"][0] = Json::Value(*level);
              domain_axes["z"] = domain_axes_z;
            }
            domain_axes["t"] = domain_axes_t;
            coverage_domain["axes"] = domain_axes;
            coverage["domain"] = coverage_domain;

            auto range_item = Json::Value(Json::ValueType::objectValue);
            range_item["type"] = Json::Value("NdArray");
            auto data_type = Json::Value(Json::ValueType::objectValue);
            auto shape = Json::Value(Json::ValueType::arrayValue);
            shape[0] = Json::Value(1);
            shape[1] = Json::Value(1);
            shape[2] = Json::Value(1);
            if (level)
              shape[3] = Json::Value(1);

            range_item["shape"] = shape;
            auto axis_names = Json::Value(Json::ValueType::arrayValue);
            axis_names[0] = Json::Value("x");
            axis_names[1] = Json::Value("y");
            if (level) {
              axis_names[2] = Json::Value("z");
              axis_names[3] = Json::Value("t");
            } else {
              axis_names[2] = Json::Value("t");
            }
            range_item["axisNames"] = axis_names;
            auto values = Json::Value(Json::ValueType::arrayValue);
            add_value(timed_value, values, data_type, 0);
            range_item["dataType"] = data_type;
            range_item["values"] = values;
            auto ranges = Json::Value(Json::ValueType::objectValue);
            ranges[parameter_name] = range_item;
            coverage["ranges"] = ranges;

            if (i == 0) {
              coverage_collection = add_prologue_coverage_collection(
                  level, emd.vertical_extent.level_type, coordinates);
              coverage_collection["parameters"] =
                  get_edr_series_parameters(query_parameters, emd.parameters);
            }
            coverages[coverage_index] = coverage;

            coverage_index++;
          }
          coordinate_index++;
        }
      }
    }

    coverage_collection["coverages"] = coverages;

    return coverage_collection;
  } catch (...) {
    throw Fmi::Exception::Trace(BCP, "Operation failed!");
  }
}
} // namespace

Json::Value
formatOutputData(TS::OutputData &outputData, const EDRMetaData &emd,
                 boost::optional<int> level,
                 const std::vector<Spine::Parameter> &query_parameters) {
  try {
    Json::Value empty_result;

    if (outputData.empty())
      return empty_result;

    const auto &outdata_first = outputData.at(0).second;

    if (outdata_first.empty())
      return empty_result;

    const auto &tsdata_first = outdata_first.at(0);

    if (boost::get<TS::TimeSeriesPtr>(&tsdata_first)) {
      return format_output_data_one_point(outputData, emd, level,
                                          query_parameters);
    } else if (boost::get<TS::TimeSeriesVectorPtr>(&tsdata_first)) {
      std::cout << "TS::TimeSeriesVectorPtr - Can do nothing" << std::endl;
    } else if (boost::get<TS::TimeSeriesGroupPtr>(&tsdata_first)) {
      //		return format_output_data_multi_point(outputData, emd,
      // levels, query_parameters);
      return format_output_data_coverage_collection(outputData, emd, level,
                                                    query_parameters);
    }

    return empty_result;
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
      link["href"] = Json::Value("https://smartmet.fmi.fi/edr/collections");
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
      link["href"] = Json::Value("https://smartmet.fmi.fi/edr/collections");
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

} // namespace CoverageJson
} // namespace EDR
} // namespace Plugin
} // namespace SmartMet
