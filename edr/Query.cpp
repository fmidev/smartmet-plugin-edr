// ======================================================================
/*!
 * \brief Utility functions for parsing the request
 */
// ======================================================================

#include "Query.h"
#include "Config.h"
#include "Hash.h"
#include "State.h"
#include <engines/geonames/Engine.h>
#ifndef WITHOUT_OBSERVATION
#include <engines/observation/Engine.h>
#endif
#include <boost/algorithm/string/classification.hpp>
#include <boost/algorithm/string/split.hpp>
#include <boost/foreach.hpp>
#include <gis/CoordinateTransformation.h>
#include <grid-files/common/GeneralFunctions.h>
#include <grid-files/common/ShowFunction.h>
#include <macgyver/AnsiEscapeCodes.h>
#include <macgyver/DistanceParser.h>
#include <macgyver/Exception.h>
#include <macgyver/StringConversion.h>
#include <macgyver/TimeParser.h>
#include <newbase/NFmiPoint.h>
#include <spine/Convenience.h>
#include <timeseries/ParameterFactory.h>
#include <timeseries/TimeSeriesGeneratorOptions.h>
#include <algorithm>

using namespace std;

namespace SmartMet
{
namespace Plugin
{
namespace EDR
{
const char *default_timezone = "localtime";

namespace
{
std::string resolve_host(const Spine::HTTP::Request &theRequest)
{
  try
  {
    auto host_header = theRequest.getHeader("Host");
    if (!host_header)
    {
      // This should never happen, host header is mandatory in HTTP 1.1
      return "http://smartmet.fmi.fi/edr";
    }

    // http/https scheme selection based on 'X-Forwarded-Proto' header
    auto host_protocol = theRequest.getProtocol();
    std::string protocol((host_protocol ? *host_protocol : "http") + "://");

    std::string host = *host_header;

    return (protocol + host + "/edr");
  }
  catch (...)
  {
    throw Fmi::Exception::Trace(BCP, "Resolving host failed!");
  }
}

void add_sql_data_filter(const Spine::HTTP::Request &req,
                         const std::string &param_name,
                         TS::DataFilter &dataFilter)
{
  try
  {
    auto param = req.getParameter(param_name);

    if (param)
      dataFilter.setDataFilter(param_name, *param);
  }
  catch (...)
  {
    throw Fmi::Exception::Trace(BCP, "Operation failed!");
  }
}

void report_unsupported_option(const std::string &name, const boost::optional<std::string> &value)
{
  if (value)
    std::cerr << (Spine::log_time_str() + ANSI_FG_RED + " [timeseries] Deprecated option '" + name +
                  ANSI_FG_DEFAULT)
              << std::endl;
}

Fmi::ValueFormatterParam valueformatter_params(const Spine::HTTP::Request &req)
{
  Fmi::ValueFormatterParam opt;
  opt.missingText = Spine::optional_string(req.getParameter("missingtext"), "nan");
  opt.floatField = Spine::optional_string(req.getParameter("floatfield"), "fixed");

  if (Spine::optional_string(req.getParameter("format"), "") == "WXML")
    opt.missingText = "NaN";
  return opt;
}

std::string transform_coordinates(const std::string &wkt, const std::string &crs)
{
  try
  {
    std::string ret;

    auto iter = wkt.begin();
    while (iter != wkt.end())
    {
      // Read till digit encountered
      while (!std::isdigit(*iter))
      {
        ret.push_back(*iter);
        iter++;
      }

      // Wkt coordinates are always of format {lon} {lat}
      std::string lon_str;
      std::string lat_str;
      // Longitude
      while (std::isdigit(*iter))
      {
        lon_str.push_back(*iter);
        iter++;
      }
      // Space between longitude and latitude
      while (!std::isdigit(*iter))
        iter++;
      // Latitude
      while (iter != wkt.end() && std::isdigit(*iter))
      {
        lat_str.push_back(*iter);
        iter++;
      }
      // Do the transformation
      auto lon_coord = Fmi::stod(lon_str);
      auto lat_coord = Fmi::stod(lat_str);
      Fmi::CoordinateTransformation transformation(crs, "WGS84");
      if (!transformation.transform(lon_coord, lat_coord))
        throw Fmi::Exception::Trace(
            BCP, "Failed to transform coordinates to WGS84: " + lon_str + ", " + lat_str);

      ret.append(Fmi::to_string(lon_coord) + " " + Fmi::to_string(lat_coord));
      // Read till next digit is encountered
      while (iter != wkt.end() && !std::isdigit(*iter))
      {
        ret.push_back(*iter);
        iter++;
      }
    }

    return ret;
  }
  catch (...)
  {
    throw Fmi::Exception::Trace(BCP,
                                "Failed to transform coordinates to WGS84: " + crs + ", " + wkt);
  }
}

}  // namespace

// ----------------------------------------------------------------------
/*!
 * \brief The constructor parses the query string
 */
// ----------------------------------------------------------------------

Query::Query(const State &state, const Spine::HTTP::Request &request, Config &config)
    : valueformatter(valueformatter_params(request)), timeAggregationRequested(false)
{
  try
  {
    format = "ascii";
    Spine::HTTP::Request req = request;
    auto resource = req.getResource();
    //	  std::cout << "BEFORE: " << resource << ", " <<
    // Spine::HTTP::urlencode(resource) << ", " <<
    // Spine::HTTP::urldecode(resource) << std::endl; 	  resource =
    // Spine::HTTP::urlencode(resource);
    std::vector<std::string> resource_parts;
    boost::algorithm::split(resource_parts, resource, boost::algorithm::is_any_of("/"));
    if (resource_parts.size() > 1 && resource_parts.at(0).empty())
      resource_parts.erase(resource_parts.begin());
    std::string data_query_list;

    if (resource_parts.size() > 0)
    {
      if (resource_parts.at(0) == "edr")
      {
        if (resource_parts.size() > 1 && resource_parts.at(1) == "collections")
        {
          itsEDRQuery.query_id = EDRQueryId::AllCollections;
          if (resource_parts.size() > 2 && !resource_parts.at(2).empty())
          {
            itsEDRQuery.collection_id = resource_parts.at(2);
            itsEDRQuery.query_id = EDRQueryId::SpecifiedCollection;
            if (resource_parts.size() > 3 && !resource_parts.at(3).empty())
            {
              if (resource_parts.at(3) == "instances")
              {
                itsEDRQuery.query_id = EDRQueryId::SpecifiedCollectionAllInstances;
                if (resource_parts.size() > 4 && !resource_parts.at(4).empty())
                {
                  itsEDRQuery.instance_id = resource_parts.at(4);
                  itsEDRQuery.query_id = EDRQueryId::SpecifiedCollectionSpecifiedInstance;
                  if (resource_parts.size() > 5 && !resource_parts.at(5).empty())
                  {
                    auto query_type_id = to_query_type_id(resource_parts.at(5));
                    itsEDRQuery.query_type = query_type_id;
                    if (itsEDRQuery.query_type == EDRQueryType::Locations)
                      itsEDRQuery.query_id = EDRQueryId::SpecifiedCollectionLocations;
                  }
                }
              }
              else
              {
                itsEDRQuery.query_type = to_query_type_id(resource_parts.at(3));
                if (itsEDRQuery.query_type == EDRQueryType::Locations)
                  itsEDRQuery.query_id = EDRQueryId::SpecifiedCollectionLocations;
                if (resource_parts.size() > 4)
                {
                  itsEDRQuery.instance_id = resource_parts.at(4);
                }
              }
            }
          }
        }
      }
    }

    const auto &dataQueries = config.getSupportedQueries();
    for (const auto &dataQueryItem : dataQueries)
    {
      const auto &producer = dataQueryItem.first;
      const auto &queries = dataQueryItem.second;
      for (const auto &dq : queries)
      {
        itsEDRQuery.data_queries[producer].insert(to_query_type_id(dq));
        if (!data_query_list.empty())
          data_query_list += ",";
        data_query_list += dq;
      }
    }

    std::set<EDRQueryType> supportedQueries =
        itsEDRQuery.data_queries.find(itsEDRQuery.collection_id) != itsEDRQuery.data_queries.end()
            ? itsEDRQuery.data_queries.at(itsEDRQuery.collection_id)
            : itsEDRQuery.data_queries.at(DEFAULT_DATA_QUERIES);

    if (itsEDRQuery.query_id == EDRQueryId::DataQuery &&
        supportedQueries.find(itsEDRQuery.query_type) == supportedQueries.end())
    {
      throw Fmi::Exception(BCP,
                           "Invalid query type '" + to_string(itsEDRQuery.query_type) +
                               "'. The following queries supported: " + data_query_list +
                               " for collection " + itsEDRQuery.collection_id + "!");
    }

    itsEDRQuery.host = resolve_host(req);

    req.removeParameter("f");  // remove f parameter, curretly only JSON is supported

    EDRMetaData emd = state.getProducerMetaData(itsEDRQuery.collection_id);

    if (!req.getQueryString().empty())
    {
      if (itsEDRQuery.query_id == EDRQueryId::SpecifiedCollectionSpecifiedInstance &&
          !itsEDRQuery.instance_id.empty())
        req.addParameter("origintime", itsEDRQuery.instance_id);
      itsEDRQuery.query_id = EDRQueryId::DataQuery;
    }

    // If meta data query return from here
    if (itsEDRQuery.query_id != EDRQueryId::DataQuery)
      return;

    // From here on EDR data query
    // Querydata and obervation producers do not have dots (.) in id, but grid
    // producers always do have
    bool grid_producer = (itsEDRQuery.collection_id.find(".") != std::string::npos);
    if (!grid_producer)
      req.addParameter("producer", itsEDRQuery.collection_id);

    // If query_type is position, radius, trajectory -> coords is required
    auto coords = Spine::optional_string(req.getParameter("coords"), "");
    if ((itsEDRQuery.query_type == EDRQueryType::Position ||
         itsEDRQuery.query_type == EDRQueryType::Radius ||
         itsEDRQuery.query_type == EDRQueryType::Trajectory ||
         itsEDRQuery.query_type == EDRQueryType::Area ||
         itsEDRQuery.query_type == EDRQueryType::Corridor) &&
        coords.empty())
      throw Fmi::Exception(BCP,
                           "Query parameter 'coords' must be defined for Position, Radius, "
                           "Trajectory, Area, Corridor query!");

    // If Trajectory + LINESTRINGZ,LINESTRINGM, LINESTRINGZM, use these
    // variables
    //	  boost::posix_time::ptime
    // first_time(boost::posix_time::not_a_date_time); 	  boost::posix_time::ptime
    // last_time(boost::posix_time::not_a_date_time); 	  std::set<double> levels;

    if (!coords.empty())
    {
      // EDR within + within-units
      auto within = Spine::optional_string(req.getParameter("within"), "");
      auto within_units = Spine::optional_string(req.getParameter("within-units"), "");
      if (itsEDRQuery.query_type == EDRQueryType::Radius &&
          (within.empty() || within_units.empty()))
        throw Fmi::Exception(BCP,
                             "Query parameter 'within' and 'within-units' "
                             "must be defined for Radius query");

      if (itsEDRQuery.query_type == EDRQueryType::Corridor)
      {
        auto corridor_width = Spine::optional_string(req.getParameter("corridor-width"), "");
        auto width_units = Spine::optional_string(req.getParameter("width-units"), "");
        if (corridor_width.empty() || width_units.empty())
          throw Fmi::Exception(BCP,
                               "Query parameter 'corridor-width' and 'width-units' must be "
                               "defined for Corridor query!");

        within = corridor_width;
        within_units = width_units;
      }

      if ((itsEDRQuery.query_type == EDRQueryType::Radius ||
           itsEDRQuery.query_type == EDRQueryType::Corridor) &&
          !within.empty() && !within_units.empty())
      {
        auto radius = Fmi::stod(within);
        if (within_units == "mi")
          radius = (radius * 1.60934);
        else if (within_units != "km")
          throw Fmi::Exception(
              BCP,
              "Invalid within-units option '" + within_units + "' used, 'km' and 'mi' supported!");
        coords += (":" + Fmi::to_string(radius));
      }
      auto wkt = coords;
      if (itsEDRQuery.query_type == EDRQueryType::Position)
      {
        // If query_type is Position and MULTIPOINTZ
        boost::algorithm::to_upper(wkt);
        boost::algorithm::trim(wkt);
        auto geometry_name = wkt.substr(0, wkt.find("("));
        if (geometry_name == "MULTIPOINTZ")
        {
          boost::algorithm::trim(geometry_name);
          auto len = (wkt.rfind(")") - wkt.find("(") - 1);
          auto geometry_values = wkt.substr(wkt.find("(") + 1, len);
          std::vector<string> coordinates;
          boost::algorithm::split(coordinates, geometry_values, boost::algorithm::is_any_of(","));

          wkt = "MULTIPOINT(";
          for (auto &coordinate_item : coordinates)
          {
            boost::algorithm::trim(coordinate_item);
            if (coordinate_item.front() == '(' && coordinate_item.back() == ')')
              coordinate_item = coordinate_item.substr(1, coordinate_item.length() - 2);
            std::vector<string> parts;
            boost::algorithm::split(parts, coordinate_item, boost::algorithm::is_any_of(" "));
            if (parts.size() != 3)
              throw Fmi::Exception(BCP,
                                   "Invalid MULTIPOINTZ definition, longitude, "
                                   "latitude, level expected: " +
                                       coords);
            wkt.append(parts[0] + " " + parts[1] + ",");
            itsCoordinateFilter.add(Fmi::stod(parts[0]), Fmi::stod(parts[1]), Fmi::stod(parts[2]));
          }
          wkt.resize(wkt.size() - 1);
          wkt.append(")");
        }
      }

      if (itsEDRQuery.query_type == EDRQueryType::Trajectory ||
          itsEDRQuery.query_type == EDRQueryType::Corridor)
      {
        // If query_type is Trajectory, check if is 2D, 3D, 4D
        boost::algorithm::to_upper(wkt);
        boost::algorithm::trim(wkt);
        auto geometry_name = wkt.substr(0, wkt.find("("));
        boost::algorithm::trim(geometry_name);
        auto len = (wkt.rfind(")") - wkt.find("(") - 1);
        auto geometry_values = wkt.substr(wkt.find("(") + 1, len);
        auto radius = wkt.substr(wkt.rfind(")") + 1);

        wkt = "LINESTRING(";
        if (geometry_name == "LINESTRINGZM")
        {
          // lon,lat,level,epoch time
          std::vector<string> coordinates;
          boost::algorithm::split(coordinates, geometry_values, boost::algorithm::is_any_of(","));
          for (const auto &coordinate_item : coordinates)
          {
            std::vector<string> parts;
            boost::algorithm::split(parts, coordinate_item, boost::algorithm::is_any_of(" "));
            if (parts.size() != 4)
              throw Fmi::Exception(BCP,
                                   "Invalid LINESTRINGZM definition, longitude, latitude, "
                                   "level, epoch time expected: " +
                                       coords);
            wkt.append(parts[0] + " " + parts[1] + ",");
            itsCoordinateFilter.add(Fmi::stod(parts[0]),
                                    Fmi::stod(parts[1]),
                                    Fmi::stod(parts[2]),
                                    boost::posix_time::from_time_t(Fmi::stod(parts[3])));
          }
        }
        else if (geometry_name == "LINESTRINGZ")
        {
          // lon,lat,level
          std::vector<string> coordinates;
          boost::algorithm::split(coordinates, geometry_values, boost::algorithm::is_any_of(","));
          for (const auto &item : coordinates)
          {
            std::vector<string> parts;
            boost::algorithm::split(parts, item, boost::algorithm::is_any_of(" "));
            if (parts.size() != 3)
              throw Fmi::Exception(BCP,
                                   "Invalid LINESTRINGZ definition, longitude, "
                                   "latitude, level expected: " +
                                       coords);
            wkt.append(parts[0] + " " + parts[1] + ",");
            itsCoordinateFilter.add(Fmi::stod(parts[0]), Fmi::stod(parts[1]), Fmi::stod(parts[2]));
          }
        }
        else if (geometry_name == "LINESTRINGM")
        {
          // lon,lat,epoch time
          std::vector<string> coordinates;
          boost::algorithm::split(coordinates, geometry_values, boost::algorithm::is_any_of(","));
          for (const auto &item : coordinates)
          {
            std::vector<string> parts;
            boost::algorithm::split(parts, item, boost::algorithm::is_any_of(" "));
            if (parts.size() != 3)
              throw Fmi::Exception(BCP,
                                   "Invalid LINESTRINGM definition, longitude, "
                                   "latitude, epoch time expected: " +
                                       coords);
            wkt.append(parts[0] + " " + parts[1] + ",");
            itsCoordinateFilter.add(Fmi::stod(parts[0]),
                                    Fmi::stod(parts[1]),
                                    boost::posix_time::from_time_t(Fmi::stod(parts[2])));
          }
        }
        else
        {
          wkt.append(geometry_values);
        }
        if (wkt.back() == ',')
          wkt.resize(wkt.size() - 1);
        wkt.append(")");
        if (!radius.empty())
          wkt.append(radius);
      }

      auto crs = Spine::optional_string(req.getParameter("crs"), "");
      if (!crs.empty() && crs != "EPSG:4326" && crs != "WGS84")
        throw Fmi::Exception(BCP, "Invalid crs: " + crs + "! Only EPSG:4326 is supported");

      /*
            // Maybe later: support other CRSs than WGS84
      if(!crs.empty() && crs != "EPSG:4326")
            {
              // Have to convert coordinates into WGS84
              auto transformed_wkt = transform_coordinates(wkt, crs);
              wkt = transformed_wkt;
            }
      */
      req.addParameter("wkt", wkt);
    }
    else
    {
      if (itsEDRQuery.query_type == EDRQueryType::Locations)
      {
        if (resource_parts.size() != 5)
          throw Fmi::Exception(BCP,
                               "Missing 'locationId' in Locations query! Format "
                               "'/edr/collections/{collectionId}/locations/{locationId}'");

        const auto &location_id = resource_parts.at(4);

        if (!emd.locations)
          throw Fmi::Exception(
              BCP,
              "Location query is not supported by collection '" + itsEDRQuery.collection_id + "'!");

        if (emd.locations->find(location_id) == emd.locations->end())
          throw Fmi::Exception(BCP,
                               "Invalid locationId '" + location_id + "' for collection '" +
                                   itsEDRQuery.collection_id +
                                   "'! Please check metadata for valid locationIds!");

        const auto &location_info = emd.locations->at(location_id);

        // Currently locationId is either fmisid, geoid or icao code (type "avid")
        if (location_info.type == "fmisid")
          req.addParameter("fmisid", location_id);
        else if (location_info.type != "avid")
          req.addParameter("geoid", location_id);
        else
          req.addParameter("icao", location_id);
      }
      else if (itsEDRQuery.query_type == EDRQueryType::Cube)
      {
        auto bbox = Spine::optional_string(req.getParameter("bbox"), "");
        if (bbox.empty())
          throw Fmi::Exception(BCP, "Query parameter 'bbox' must be defined for Cube!");

        std::vector<string> parts;
        boost::algorithm::split(parts, bbox, boost::algorithm::is_any_of(","));
        if (parts.size() != 2)
          throw Fmi::Exception(BCP,
                               "Invalid bbox parameter, format is bbox=<lower left coordinate>, "
                               "<upper right coordinate>, e.g.  bbox=19.4 59.6, 31.6 70.1");
        auto lower_left_coord = parts[0];
        auto upper_right_coord = parts[1];
        boost::algorithm::trim(lower_left_coord);
        boost::algorithm::trim(upper_right_coord);
        parts.clear();
        boost::algorithm::split(parts, lower_left_coord, boost::algorithm::is_any_of(" "));
        if (parts.size() != 2)
          throw Fmi::Exception(BCP,
                               "Invalid bbox parameter, format is bbox=<lower left coordinate>, "
                               "<upper right coordinate>, e.g.  bbox=19.4 59.6, 31.6 70.1");
        auto lower_left_x = parts[0];
        auto lower_left_y = parts[1];
        parts.clear();
        boost::algorithm::split(parts, upper_right_coord, boost::algorithm::is_any_of(" "));
        if (parts.size() != 2)
          throw Fmi::Exception(BCP,
                               "Invalid bbox parameter, format is bbox=<lower left coordinate>, "
                               "<upper right coordinate>, e.g.  bbox=19.4 59.6, 31.6 70.1");
        auto upper_right_x = parts[0];
        auto upper_right_y = parts[1];

        auto wkt =
            ("POLYGON((" + lower_left_x + " " + lower_left_y + "," + lower_left_x + " " +
             upper_right_y + "," + upper_right_x + " " + upper_right_y + "," + upper_right_x + " " +
             lower_left_y + "," + lower_left_x + " " + lower_left_y + "))");

        req.addParameter("wkt", wkt);
        req.removeParameter("bbox");
      }
    }

    // EDR datetime
    auto datetime = Spine::optional_string(req.getParameter("datetime"), "");

    if (datetime.empty())
      datetime = itsCoordinateFilter.getDatetime();

    if (datetime.empty())
      throw Fmi::Exception(BCP, "Missing datetime option!");

    if (datetime.find("/") != std::string::npos)
    {
      std::vector<std::string> datetime_parts;
      boost::algorithm::split(datetime_parts, datetime, boost::algorithm::is_any_of("/"));
      auto starttime = datetime_parts.at(0);
      auto endtime = datetime_parts.at(1);
      if (starttime == "..")
        req.addParameter("starttime", "data");
      else
        req.addParameter("starttime", starttime);
      if (endtime != "..")
        req.addParameter("endtime", endtime);
    }
    else
    {
      if (boost::algorithm::ends_with(datetime, ".."))
      {
        datetime.resize(datetime.size() - 2);
        req.addParameter("starttime", datetime);
      }
      else if (boost::algorithm::starts_with(datetime, ".."))
      {
        datetime = datetime.substr(2);
        req.addParameter("endtime", datetime);
      }
      else
      {
        req.addParameter("starttime", datetime);
        req.addParameter("endtime", datetime);
      }
    }

    // EDR parameter-name
    auto parameter_names = Spine::optional_string(req.getParameter("parameter-name"), "");

    // Check valid parameters for requested producer
    std::list<string> param_names;
    boost::algorithm::split(param_names, parameter_names, boost::algorithm::is_any_of(","));

    if (emd.vertical_extent.levels.empty() && itsEDRQuery.query_type == EDRQueryType::Cube)
      throw Fmi::Exception(BCP,
                           "Error! Cube query not possible for '" + itsEDRQuery.collection_id +
                               "', because there is no vertical extent!");

    std::string producerId = "";
    std::string geometryId = "";
    std::string levelId = "";
    if (grid_producer)
    {
      std::vector<std::string> producer_parts;
      boost::algorithm::split(
          producer_parts, itsEDRQuery.collection_id, boost::algorithm::is_any_of("."));

      producerId = producer_parts[0];
      if (producer_parts.size() > 1)
        geometryId = producer_parts[1];
      if (producer_parts.size() > 2)
        levelId = producer_parts[2];
    }

    // EDR z
    auto z = Spine::optional_string(req.getParameter("z"), "");
    double min_level = std::numeric_limits<double>::min();
    double max_level = std::numeric_limits<double>::max();
    bool range = false;
    if (z.find("/") != std::string::npos)
    {
      std::vector<std::string> parts;
      boost::algorithm::split(parts, z, boost::algorithm::is_any_of("/"));
      min_level = Fmi::stod(parts[0]);
      max_level = Fmi::stod(parts[1]);
      z.clear();
      range = true;
    }
    if (!range && z.empty())
      z = itsCoordinateFilter.getLevels();
    // If no level given get all levels
    if (z.empty() && emd.vertical_extent.levels.size() > 0)
    {
      for (const auto &l : emd.vertical_extent.levels)
      {
        double d_level = Fmi::stod(l);
        if (d_level < min_level || d_level > max_level)
          continue;
        if (!z.empty())
          z.append(",");
        z.append(l);
      }
    }
    if (!z.empty() && !grid_producer)
      req.addParameter("levels", z);

    std::string cleaned_param_names;
    for (auto p : param_names)
    {
      boost::algorithm::to_lower(p);
      if (p.find(":") != std::string::npos)
        p = p.substr(0, p.find(":"));
      if (emd.parameters.find(p) == emd.parameters.end())
      {
        std::cerr << (Spine::log_time_str() + ANSI_FG_MAGENTA + " [edr] Unknown parameter '" + p +
                      "' ignored!" ANSI_FG_DEFAULT)
                  << std::endl;
      }
      if (emd.parameters.find(p) != emd.parameters.end())
      {
        if (!cleaned_param_names.empty())
          cleaned_param_names += ",";
        if (grid_producer)
        {
          p.append(":" + producerId);
          if (!geometryId.empty())
            p.append(":" + geometryId);
          if (!levelId.empty())
            p.append(":" + levelId);
          if (!z.empty())
            p.append(":" + z);
        }
        cleaned_param_names += p;
      }
    }
    parameter_names = cleaned_param_names;

    if (parameter_names.empty())
    {
      if (emd.parameters.empty())
        throw Fmi::Exception(BCP, "No metadata for " + itsEDRQuery.collection_id + "!");
      else
        throw Fmi::Exception(BCP, "Missing parameter-name option!");
    }

    // Longitude, latitude are always needed
    parameter_names += ",longitude,latitude";
    // If producer has levels we need to query them
    if (emd.vertical_extent.levels.size() > 0)
      parameter_names += ",level";

    req.addParameter("param", parameter_names);

    language = Spine::optional_string(req.getParameter("lang"), config.defaultLanguage());
    itsEDRQuery.language = language;

    report_unsupported_option("adjustfield", req.getParameter("adjustfield"));
    report_unsupported_option("width", req.getParameter("width"));
    report_unsupported_option("fill", req.getParameter("fill"));
    report_unsupported_option("showpos", req.getParameter("showpos"));
    report_unsupported_option("uppercase", req.getParameter("uppercase"));

    time_t tt = time(nullptr);
    if ((config.itsLastAliasCheck + 10) < tt)
    {
      config.itsAliasFileCollection.checkUpdates(false);
      config.itsLastAliasCheck = tt;
    }

    itsAliasFileCollectionPtr = &config.itsAliasFileCollection;

    forecastSource = Spine::optional_string(req.getParameter("source"), "");

    // ### Attribute list ( attr=name1:value1,name2:value2,name3:$(alias1) )

    string attr = Spine::optional_string(req.getParameter("attr"), "");
    if (!attr.empty())
    {
      bool ind = true;
      uint loopCount = 0;
      while (ind)
      {
        loopCount++;
        if (loopCount > 10)
          throw Fmi::Exception(BCP, "The alias definitions seem to contain an eternal loop!");

        ind = false;
        std::string alias;
        if (itsAliasFileCollectionPtr->replaceAlias(attr, alias))
        {
          attr = alias;
          ind = true;
        }
      }

      std::vector<std::string> partList;
      splitString(attr, ';', partList);
      for (const auto &part : partList)
      {
        std::vector<std::string> list;
        splitString(part, ':', list);
        if (list.size() == 2)
        {
          std::string name = list[0];
          std::string value = list[1];

          attributeList.addAttribute(name, value);
        }
      }
    }

    // attributeList.print(std::cout,0,0);

    if (!emd.isAviProducer)
    {
      T::Attribute *v1 = attributeList.getAttributeByNameEnd("Grib1.IndicatorSection.EditionNumber");
      T::Attribute *v2 = attributeList.getAttributeByNameEnd("Grib2.IndicatorSection.EditionNumber");

      T::Attribute *lat = attributeList.getAttributeByNameEnd("LatitudeOfFirstGridPoint");
      T::Attribute *lon = attributeList.getAttributeByNameEnd("LongitudeOfFirstGridPoint");

      if (v1 != nullptr && lat != nullptr && lon != nullptr)
      {
        // Using coordinate that is inside the GRIB1 grid

        double latitude = toDouble(lat->mValue.c_str()) / 1000;
        double longitude = toDouble(lon->mValue.c_str()) / 1000;

        std::string val = std::to_string(latitude) + "," + std::to_string(longitude);
        Spine::HTTP::Request tmpReq;
        tmpReq.addParameter("latlon", val);
        loptions.reset(
            new Engine::Geonames::LocationOptions(state.getGeoEngine().parseLocations(tmpReq)));
      }
      else if (v2 != nullptr && lat != nullptr && lon != nullptr)
      {
        // Using coordinate that is inside the GRIB2 grid

        double latitude = toDouble(lat->mValue.c_str()) / 1000000;
        double longitude = toDouble(lon->mValue.c_str()) / 1000000;

        std::string val = std::to_string(latitude) + "," + std::to_string(longitude);
        Spine::HTTP::Request tmpReq;
        tmpReq.addParameter("latlon", val);
        loptions.reset(
            new Engine::Geonames::LocationOptions(state.getGeoEngine().parseLocations(tmpReq)));
      }
      else
      {
        loptions.reset(
            new Engine::Geonames::LocationOptions(state.getGeoEngine().parseLocations(req)));

        auto lon_coord = Spine::optional_double(req.getParameter("x"), kFloatMissing);
        auto lat_coord = Spine::optional_double(req.getParameter("y"), kFloatMissing);
        auto source_crs = Spine::optional_string(req.getParameter("crs"), "");

        if (lon_coord != kFloatMissing && lat_coord != kFloatMissing && !source_crs.empty())
        {
          // Transform lon_coord, lat_coord to lonlat parameter
          if (source_crs != "ESPG:4326")
          {
            Fmi::CoordinateTransformation transformation(source_crs, "WGS84");
            transformation.transform(lon_coord, lat_coord);
          }
          auto tmpReq = req;
          tmpReq.addParameter("lonlat",
                              (Fmi::to_string(lon_coord) + "," + Fmi::to_string(lat_coord)));
          loptions.reset(
              new Engine::Geonames::LocationOptions(state.getGeoEngine().parseLocations(tmpReq)));
        }
      }

      // Store WKT-geometries
      wktGeometries = state.getGeoEngine().getWktGeometries(*loptions, language);
    }
    else
    {
      loptions.reset(new SmartMet::Engine::Geonames::LocationOptions());
      loptions->setLocations(SmartMet::Spine::TaggedLocationList());

      requestWKT = Spine::optional_string(req.getParameter("wkt"), "");
    }

    toptions = TS::parseTimes(req);

#ifdef MYDEBUG
    std::cout << "Time options: " << std::endl << toptions << std::endl;
#endif

    latestTimestep = toptions.startTime;

    starttimeOptionGiven = (!!req.getParameter("starttime") || !!req.getParameter("now"));
    std::string endtime = Spine::optional_string(req.getParameter("endtime"), "");
    endtimeOptionGiven = (endtime != "now");

#ifndef WITHOUT_OBSERVATION
    latestObservation = (endtime == "now");
    allplaces = (Spine::optional_string(req.getParameter("places"), "") == "all");
#endif

    debug = Spine::optional_bool(req.getParameter("debug"), false);

    timezone = Spine::optional_string(req.getParameter("tz"), default_timezone);

    step = Spine::optional_double(req.getParameter("step"), 0.0);
    leveltype = Spine::optional_string(req.getParameter("leveltype"), "");
    format = Spine::optional_string(req.getParameter("format"), "ascii");
    areasource = Spine::optional_string(req.getParameter("areasource"), "");
    crs = Spine::optional_string(req.getParameter("crs"), "");

    // Either create the requested locale or use the default one constructed
    // by the Config parser. TODO: If constructing from strings is slow, we
    // should cache locales instead.

    auto opt_locale = req.getParameter("locale");
    if (!opt_locale)
      outlocale = config.defaultLocale();
    else
      outlocale = locale(opt_locale->c_str());

    timeformat = Spine::optional_string(req.getParameter("timeformat"), config.defaultTimeFormat());

    maxdistanceOptionGiven = !!req.getParameter("maxdistance");
    maxdistance =
        Spine::optional_string(req.getParameter("maxdistance"), config.defaultMaxDistance());

    findnearestvalidpoint = Spine::optional_bool(req.getParameter("findnearestvalid"), false);

    boost::optional<std::string> tmp = req.getParameter("origintime");
    if (tmp)
    {
      if (*tmp == "latest" || *tmp == "newest")
        origintime = boost::posix_time::ptime(boost::date_time::pos_infin);
      else if (*tmp == "oldest")
        origintime = boost::posix_time::ptime(boost::date_time::neg_infin);
      else
        origintime = Fmi::TimeParser::parse(*tmp);
    }

#ifndef WITHOUT_OBSERVARION
    Query::parse_producers(req, state.getQEngine(), state.getGridEngine(), state.getObsEngine(),
                             state.getAviMetaData());
#else
    Query::parse_producers(req, state.getQEngine(), state.getGridEngine(), state.getAviMetaData());
#endif

#ifndef WITHOUT_OBSERVATION
    Query::parse_parameters(req, state.getObsEngine());
#else
    Query::parse_parameters(req);
#endif

    Query::parse_levels(req);

    // wxml requires 8 parameters

    timestring = Spine::optional_string(req.getParameter("timestring"), "");
    if (format == "wxml")
    {
      timeformat = "xml";
      poptions.add(TS::ParameterFactory::instance().parse("origintime"));
      poptions.add(TS::ParameterFactory::instance().parse("xmltime"));
      poptions.add(TS::ParameterFactory::instance().parse("weekday"));
      poptions.add(TS::ParameterFactory::instance().parse("timestring"));
      poptions.add(TS::ParameterFactory::instance().parse("name"));
      poptions.add(TS::ParameterFactory::instance().parse("geoid"));
      poptions.add(TS::ParameterFactory::instance().parse("longitude"));
      poptions.add(TS::ParameterFactory::instance().parse("latitude"));
    }

    // This must be done after params is no longer being modified
    // by the above wxml code!

    Query::parse_precision(req, config);

    timeformatter.reset(Fmi::TimeFormatter::create(timeformat));

#ifndef WITHOUT_OBSERVATION
    // observation params
    numberofstations = Spine::optional_int(req.getParameter("numberofstations"), 1);
#endif

#ifndef WITHOUT_OBSERVATION
    auto name = req.getParameter("fmisid");
    if (name)
    {
      const string fmisidreq = *name;
      vector<string> parts;
      boost::algorithm::split(parts, fmisidreq, boost::algorithm::is_any_of(","));

      for (const string &sfmisid : parts)
      {
        int f = Fmi::stoi(sfmisid);
        this->fmisids.push_back(f);
      }
    }

    // sid is an alias for fmisid
    name = req.getParameter("sid");
    if (name)
    {
      const string sidreq = *name;
      vector<string> parts;
      boost::algorithm::split(parts, sidreq, boost::algorithm::is_any_of(","));

      for (const string &ssid : parts)
      {
        int f = Fmi::stoi(ssid);
        this->fmisids.push_back(f);
      }
    }
#endif

#ifndef WITHOUT_OBSERVATION
    name = req.getParameter("wmo");
    if (name)
    {
      const string wmoreq = *name;
      vector<string> parts;
      boost::algorithm::split(parts, wmoreq, boost::algorithm::is_any_of(","));

      for (const string &swmo : parts)
      {
        int w = Fmi::stoi(swmo);
        this->wmos.push_back(w);
      }
    }
#endif

#ifndef WITHOUT_OBSERVATION
    name = req.getParameter("lpnn");
    if (name)
    {
      const string lpnnreq = *name;
      vector<string> parts;
      boost::algorithm::split(parts, lpnnreq, boost::algorithm::is_any_of(","));

      for (const string &slpnn : parts)
      {
        int l = Fmi::stoi(slpnn);
        this->lpnns.push_back(l);
      }
    }
#endif

    name = req.getParameter("icao");
    if (name)
    {
      const string icaoreq = *name;
      vector<string> parts;
      boost::algorithm::split(parts, icaoreq, boost::algorithm::is_any_of(","));

      this->icaos.insert(this->icaos.begin(), parts.begin(), parts.end());
    }

#ifndef WITHOUT_OBSERVATION
    if (!!req.getParameter("bbox"))
    {
      string bbox = *req.getParameter("bbox");
      vector<string> parts;
      boost::algorithm::split(parts, bbox, boost::algorithm::is_any_of(","));
      std::string lat2(parts[3]);
      if (lat2.find(':') != string::npos)
        lat2 = lat2.substr(0, lat2.find(':'));

      // Bounding box must contain exactly 4 elements
      if (parts.size() != 4)
      {
        throw Fmi::Exception(BCP, "Invalid bounding box '" + bbox + "'!");
      }

      if (!parts[0].empty())
      {
        boundingBox["minx"] = Fmi::stod(parts[0]);
      }
      if (!parts[1].empty())
      {
        boundingBox["miny"] = Fmi::stod(parts[1]);
      }
      if (!parts[2].empty())
      {
        boundingBox["maxx"] = Fmi::stod(parts[2]);
      }
      if (!parts[3].empty())
      {
        boundingBox["maxy"] = Fmi::stod(lat2);
      }
    }
    // Data filter options
    add_sql_data_filter(req, "station_id", dataFilter);
    add_sql_data_filter(req, "data_quality", dataFilter);
    // Sounding filtering options
    add_sql_data_filter(req, "sounding_type", dataFilter);
    add_sql_data_filter(req, "significance", dataFilter);

    std::string dataCache = Spine::optional_string(req.getParameter("useDataCache"), "true");
    useDataCache = (dataCache == "true" || dataCache == "1");

#endif

    if (!!req.getParameter("weekday"))
    {
      const string sweekdays = *req.getParameter("weekday");
      vector<string> parts;
      boost::algorithm::split(parts, sweekdays, boost::algorithm::is_any_of(","));

      for (const string &wday : parts)
      {
        int h = Fmi::stoi(wday);
        this->weekdays.push_back(h);
      }
    }

    startrow = Spine::optional_size(req.getParameter("startrow"), 0);
    maxresults = Spine::optional_size(req.getParameter("maxresults"), 0);
    groupareas = Spine::optional_bool(req.getParameter("groupareas"), true);
  }
  catch (...)
  {
    // The stack traces are useless when the user has made a typo
    throw Fmi::Exception::Trace(BCP, "TimeSeries plugin failed to parse query string options!")
        .disableStackTrace();
  }
}

bool Query::isAviProducer(const EDRProducerMetaData &avi, const std::string &producer) const
{
  return (avi.find(producer) != avi.end());
}

// ----------------------------------------------------------------------
/*!
 * \brief Parse producer/model options
 */
// ----------------------------------------------------------------------

#ifndef WITHOUT_OBSERVATION
void Query::parse_producers(const Spine::HTTP::Request &theReq,
                            const Engine::Querydata::Engine &theQEngine,
                            const Engine::Grid::Engine *theGridEngine,
                            const Engine::Observation::Engine *theObsEngine,
                            const EDRProducerMetaData &avi)
#else
void Query::parse_producers(const Spine::HTTP::Request &theReq,
                            const Engine::Querydata::Engine &theQEngine,
                            const Engine::Grid::Engine &theGridEngine,
                            const EDRProducerMetaData &avi)
#endif
{
  try
  {
    string opt = Spine::optional_string(theReq.getParameter("model"), "");

    string opt2 = Spine::optional_string(theReq.getParameter("producer"), "");

    if (opt == "default" || opt2 == "default")
    {
      // Use default if it's forced by any option
      return;
    }

    // observation uses stationtype-parameter
    if (opt.empty() && opt2.empty())
      opt2 = Spine::optional_string(theReq.getParameter("stationtype"), "");

    std::list<std::string> resultProducers;
    const std::string &prodOpt = opt.empty() ? opt2 : opt;

    // Handle time separation:: either 'model' or 'producer' keyword used
    if (!opt.empty())
      boost::algorithm::split(resultProducers, opt, boost::algorithm::is_any_of(";"));
    else if (!opt2.empty())
      boost::algorithm::split(resultProducers, opt2, boost::algorithm::is_any_of(";"));

    for (auto &p : resultProducers)
    {
      // Avi collection names are in upper case and only one producer name is allowed

      if (!isAviProducer(avi, p))
        boost::algorithm::to_lower(p);
      else if (resultProducers.size() > 1)
        throw Fmi::Exception(BCP, "Only one producer is allowed for avi query: '" + prodOpt + "'");
      else
      {
        AreaProducers ap = { p };
        timeproducers.push_back(ap);

        return;
      }

      if (p == "itmf")
      {
        p = Engine::Observation::FMI_IOT_PRODUCER;
        iot_producer_specifier = "itmf";
      }
    }

    // Verify the producer names are valid

#ifndef WITHOUT_OBSERVATION
    std::set<std::string> observations;
    if (theObsEngine != nullptr)
      observations = theObsEngine->getValidStationTypes();
#endif

    for (const auto &p : resultProducers)
    {
      bool ok = theQEngine.hasProducer(p);
#ifndef WITHOUT_OBSERVATION
      if (!ok)
        ok = (observations.find(p) != observations.end());
#endif
      if (!ok && theGridEngine)
        ok = theGridEngine->isGridProducer(p);

      if (!ok)
        throw Fmi::Exception(BCP, "Unknown producer name '" + p + "'");
    }

    // Now split into location parts

    for (const auto &tproducers : resultProducers)
    {
      AreaProducers producers;
      boost::algorithm::split(producers, tproducers, boost::algorithm::is_any_of(","));

      // FMI producer is deprecated, use OPENDATA instead
      // std::replace(producers.begin(), producers.end(), std::string("fmi"),
      // std::string("opendata"));

      timeproducers.push_back(producers);
    }
  }
  catch (...)
  {
    throw Fmi::Exception::Trace(BCP, "Operation failed!");
  }
}

// ----------------------------------------------------------------------
/*!
 * \brief parse Level query
 *
 * Empty result implies all levels are wanted
 */
// ----------------------------------------------------------------------

void Query::parse_levels(const Spine::HTTP::Request &theReq)
{
  try
  {
    // Get the option string

    string opt = Spine::optional_string(theReq.getParameter("level"), "");
    if (!opt.empty())
    {
      levels.insert(Fmi::stoi(opt));
    }

    // Allow also "levels"
    opt = Spine::optional_string(theReq.getParameter("levels"), "");
    if (!opt.empty())
    {
      vector<string> parts;
      boost::algorithm::split(parts, opt, boost::algorithm::is_any_of(","));
      for (const string &tmp : parts)
        levels.insert(Fmi::stoi(tmp));
    }

    // Get pressures and heights to extract data

    opt = Spine::optional_string(theReq.getParameter("pressure"), "");
    if (!opt.empty())
    {
      pressures.insert(Fmi::stod(opt));
    }

    opt = Spine::optional_string(theReq.getParameter("pressures"), "");
    if (!opt.empty())
    {
      vector<string> parts;
      boost::algorithm::split(parts, opt, boost::algorithm::is_any_of(","));
      for (const string &tmp : parts)
        pressures.insert(Fmi::stod(tmp));
    }

    opt = Spine::optional_string(theReq.getParameter("height"), "");
    if (!opt.empty())
    {
      heights.insert(Fmi::stod(opt));
    }

    opt = Spine::optional_string(theReq.getParameter("heights"), "");
    if (!opt.empty())
    {
      vector<string> parts;
      boost::algorithm::split(parts, opt, boost::algorithm::is_any_of(","));
      for (const string &tmp : parts)
        heights.insert(Fmi::stod(tmp));
    }
  }
  catch (...)
  {
    throw Fmi::Exception::Trace(BCP, "Operation failed!");
  }
}

// ----------------------------------------------------------------------
/*!
 * \brief Parse precision options
 */
// ----------------------------------------------------------------------

void Query::parse_precision(const Spine::HTTP::Request &req, const Config &config)
{
  try
  {
    string precname =
        Spine::optional_string(req.getParameter("precision"), config.defaultPrecision());

    const Precision &prec = config.getPrecision(precname);

    precisions.reserve(poptions.size());

    for (const TS::OptionParsers::ParameterList::value_type &p : poptions.parameters())
    {
      Precision::Map::const_iterator it = prec.parameter_precisions.find(p.name());
      if (it == prec.parameter_precisions.end())
      {
        precisions.push_back(prec.default_precision);  // unknown gets default
      }
      else
      {
        precisions.push_back(it->second);  // known gets configured value
      }
    }
  }
  catch (...)
  {
    throw Fmi::Exception::Trace(BCP, "Operation failed!");
  }
}

#ifndef WITHOUT_OBSERVATION
void Query::parse_parameters(const Spine::HTTP::Request &theReq,
                             const Engine::Observation::Engine *theObsEngine)
#else
void Query::parse_parameters(const Spine::HTTP::Request &theReq)
#endif
{
  try
  {
    // Get the option string
    string opt =
        Spine::required_string(theReq.getParameter("param"), "The 'param' option is required!");

    // Protect against empty selection
    if (opt.empty())
      throw Fmi::Exception(BCP, "The 'param' option is empty!");

    // Split
    using Names = list<string>;
    Names tmpNames;
    boost::algorithm::split(tmpNames, opt, boost::algorithm::is_any_of(","));

    Names names;
    bool ind = true;
    uint loopCount = 0;
    while (ind)
    {
      loopCount++;
      if (loopCount > 10)
        throw Fmi::Exception(BCP, "The alias definitions seem to contain an eternal loop!");

      ind = false;
      names.clear();

      for (const auto &tmpName : tmpNames)
      {
        std::string alias;
        if (itsAliasFileCollectionPtr->getAlias(tmpName, alias))
        {
          Names tmp;
          boost::algorithm::split(tmp, alias, boost::algorithm::is_any_of(","));
          for (auto tt = tmp.begin(); tt != tmp.end(); ++tt)
          {
            names.push_back(*tt);
          }
          ind = true;
        }
        else if (itsAliasFileCollectionPtr->replaceAlias(tmpName, alias))
        {
          Names tmp;
          boost::algorithm::split(tmp, alias, boost::algorithm::is_any_of(","));
          for (const auto &tt : tmp)
            names.push_back(tt);

          ind = true;
        }
        else
        {
          names.push_back(tmpName);
        }
      }

      if (ind)
        tmpNames = names;
    }

#ifndef WITHOUT_OBSERVATION

    // Determine whether any producer implies observations are needed

    bool obsProducersExist = false;

    if (theObsEngine != nullptr)
    {
      std::set<std::string> obsEngineStationTypes = theObsEngine->getValidStationTypes();
      for (const auto &areaproducers : timeproducers)
      {
        if (obsProducersExist)
          break;

        for (const auto &producer : areaproducers)
        {
          if (obsEngineStationTypes.find(producer) != obsEngineStationTypes.end())
          {
            obsProducersExist = true;
            break;
          }
        }
      }
    }
#endif

    // Validate and convert
    for (const string &paramname : names)
    {
      try
      {
        TS::ParameterAndFunctions paramfuncs =
            TS::ParameterFactory::instance().parseNameAndFunctions(paramname, true);

        poptions.add(paramfuncs.parameter, paramfuncs.functions);
      }
      catch (...)
      {
        Fmi::Exception exception(BCP, "Parameter parsing failed for '" + paramname + "'!", NULL);
        throw exception;
      }
    }
    poptions.expandParameter("data_source");

    std::string aggregationIntervalStringBehind =
        Spine::optional_string(theReq.getParameter("interval"), "0m");
    std::string aggregationIntervalStringAhead = ("0m");

    // check if second aggregation interval is defined
    if (aggregationIntervalStringBehind.find(':') != string::npos)
    {
      aggregationIntervalStringAhead =
          aggregationIntervalStringBehind.substr(aggregationIntervalStringBehind.find(':') + 1);
      aggregationIntervalStringBehind =
          aggregationIntervalStringBehind.substr(0, aggregationIntervalStringBehind.find(':'));
    }

    int agg_interval_behind(Spine::duration_string_to_minutes(aggregationIntervalStringBehind));
    int agg_interval_ahead(Spine::duration_string_to_minutes(aggregationIntervalStringAhead));

    if (agg_interval_behind < 0 || agg_interval_ahead < 0)
      throw Fmi::Exception(BCP, "The 'interval' option must be positive!");

    // set aggregation interval if it has not been set in parameter parser
    for (const TS::ParameterAndFunctions &paramfuncs : poptions.parameterFunctions())
    {
      if (paramfuncs.functions.innerFunction.getAggregationIntervalBehind() ==
          std::numeric_limits<unsigned int>::max())
        const_cast<TS::DataFunction &>(paramfuncs.functions.innerFunction)
            .setAggregationIntervalBehind(agg_interval_behind);
      if (paramfuncs.functions.innerFunction.getAggregationIntervalAhead() ==
          std::numeric_limits<unsigned int>::max())
        const_cast<TS::DataFunction &>(paramfuncs.functions.innerFunction)
            .setAggregationIntervalAhead(agg_interval_ahead);

      if (paramfuncs.functions.outerFunction.getAggregationIntervalBehind() ==
          std::numeric_limits<unsigned int>::max())
        const_cast<TS::DataFunction &>(paramfuncs.functions.outerFunction)
            .setAggregationIntervalBehind(agg_interval_behind);
      if (paramfuncs.functions.outerFunction.getAggregationIntervalAhead() ==
          std::numeric_limits<unsigned int>::max())
        const_cast<TS::DataFunction &>(paramfuncs.functions.outerFunction)
            .setAggregationIntervalAhead(agg_interval_ahead);
    }

    // store maximum aggregation intervals per parameter for later use
    for (const TS::ParameterAndFunctions &paramfuncs : poptions.parameterFunctions())
    {
      std::string paramname(paramfuncs.parameter.name());

      if (maxAggregationIntervals.find(paramname) == maxAggregationIntervals.end())
        maxAggregationIntervals.insert(make_pair(paramname, AggregationInterval(0, 0)));

      if (paramfuncs.functions.innerFunction.type() == TS::FunctionType::TimeFunction)
      {
        if (maxAggregationIntervals[paramname].behind <
            paramfuncs.functions.innerFunction.getAggregationIntervalBehind())
          maxAggregationIntervals[paramname].behind =
              paramfuncs.functions.innerFunction.getAggregationIntervalBehind();
        if (maxAggregationIntervals[paramname].ahead <
            paramfuncs.functions.innerFunction.getAggregationIntervalAhead())
          maxAggregationIntervals[paramname].ahead =
              paramfuncs.functions.innerFunction.getAggregationIntervalAhead();
      }
      else if (paramfuncs.functions.outerFunction.type() == TS::FunctionType::TimeFunction)
      {
        if (maxAggregationIntervals[paramname].behind <
            paramfuncs.functions.outerFunction.getAggregationIntervalBehind())
          maxAggregationIntervals[paramname].behind =
              paramfuncs.functions.outerFunction.getAggregationIntervalBehind();
        if (maxAggregationIntervals[paramname].ahead <
            paramfuncs.functions.outerFunction.getAggregationIntervalAhead())
          maxAggregationIntervals[paramname].ahead =
              paramfuncs.functions.outerFunction.getAggregationIntervalAhead();
      }

      if (maxAggregationIntervals[paramname].behind > 0 ||
          maxAggregationIntervals[paramname].ahead > 0)
        timeAggregationRequested = true;
    }
  }
  catch (...)
  {
    throw Fmi::Exception::Trace(BCP, "Operation failed!");
  }
}

double Query::maxdistance_kilometers() const
{
  return Fmi::DistanceParser::parse_kilometer(maxdistance);
}

double Query::maxdistance_meters() const
{
  return Fmi::DistanceParser::parse_meter(maxdistance);
}

}  // namespace EDR
}  // namespace Plugin
}  // namespace SmartMet

// ======================================================================
