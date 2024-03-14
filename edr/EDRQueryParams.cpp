#include "EDRQueryParams.h"
#include "EDRDefs.h"
#include "EDRMetaData.h"
#include <macgyver/AnsiEscapeCodes.h>
#include <macgyver/StringConversion.h>
#include <spine/Convenience.h>
#include <spine/FmiApiKey.h>
#include <string>

namespace SmartMet
{
namespace Plugin
{
namespace EDR
{
#define EDRException(description) Fmi::Exception(BCP, description)

namespace
{
bool is_data_query(const Spine::HTTP::Request& req,
                   const EDRQuery& edrQuery,
                   const EDRMetaData& edrMetaData)
{
  try
  {
    auto pcount = req.getParameterCount();
    // If more than one parameters -> data query
    if (pcount > 1)
      return true;
    if (pcount == 1)
    {
      // If only one parameter and it is 'f' -> metadata query, except AVI queries
      auto fparam = req.getParameter("f");
      return (fparam && edrMetaData.isAviProducer() && !edrQuery.instance_id.empty());
    }

    // If no parameters it is metadata query except for non empty AVI query
    return (edrMetaData.isAviProducer() && !edrQuery.instance_id.empty());
  }
  catch (...)
  {
    throw Fmi::Exception::Trace(BCP, "Resolving host failed!");
  }
}

std::string resolve_host(const Spine::HTTP::Request& theRequest)
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

    // Apikey
    boost::optional<std::string> apikey;
    try
    {
      // Deduce apikey for layer filtering
      apikey = Spine::FmiApiKey::getFmiApiKey(theRequest);
    }
    catch (...)
    {
      throw Fmi::Exception::Trace(BCP, "Failed to get apikey from the query");
    }

    if (apikey)
      host.append(("/fmi-apikey/" + *apikey));

    return (protocol + host + "/edr");
  }
  catch (...)
  {
    throw Fmi::Exception::Trace(BCP, "Resolving host failed!");
  }
}

}  // anonymous namespace

EDRQueryParams::EDRQueryParams(const State& state,
                               const Spine::HTTP::Request& request,
                               const Config& config)
    : req(request)
{
  try
  {
    auto resource = req.getResource();
    itsEDRQuery.host = resolve_host(req);

    if (config.getEDRAPI().isEDRAPIQuery(resource))
    {
      itsEDRQuery.query_id = EDRQueryId::APIQuery;
      itsEDRQuery.instance_id = resource;
      return;
    }

    std::vector<std::string> resource_parts;
    boost::algorithm::split(resource_parts, resource, boost::algorithm::is_any_of("/"));
    if (resource_parts.size() > 1 && resource_parts.front().empty())
      resource_parts.erase(resource_parts.begin());

    auto invalid_query_type = parseEDRQuery(state, resource);

    EDRMetaData emd = state.getProducerMetaData(itsEDRQuery.collection_id);

    // Allow missing vertical extent when processing cube query for non grid source data
    // (e.g. observations, surface querydata and aviation messages; BRAINSTORM-2827)

    if (emd.vertical_extent.levels.empty() && itsEDRQuery.query_type == EDRQueryType::Cube &&
        emd.metadata_source == SourceEngine::Grid)
      throw EDRException("Error! Cube query not possible for '" + itsEDRQuery.collection_id +
                         "', because there is no vertical extent!");

    if (is_data_query(req, itsEDRQuery, emd))
    {
      if (itsEDRQuery.query_id == EDRQueryId::SpecifiedCollectionSpecifiedInstance &&
          !itsEDRQuery.instance_id.empty())
        req.addParameter("origintime", itsEDRQuery.instance_id);
      itsEDRQuery.query_id = EDRQueryId::DataQuery;
    }

    const auto& supportedDataQueries = config.getSupportedDataQueries(itsEDRQuery.collection_id);

    if (!invalid_query_type.empty() || (itsEDRQuery.query_id == EDRQueryId::DataQuery &&
                                        supportedDataQueries.find(to_string(
                                            itsEDRQuery.query_type)) == supportedDataQueries.end()))
    {
      auto data_query_list = boost::algorithm::join(supportedDataQueries, ",");
      auto requested_query_type =
          (!invalid_query_type.empty() ? invalid_query_type : to_string(itsEDRQuery.query_type));
      throw EDRException("Invalid query type '" + requested_query_type +
                         "'. The following queries are supported for collection " +
                         itsEDRQuery.collection_id + ": " + data_query_list);
    }

    // If meta data query return from here
    if (itsEDRQuery.query_id != EDRQueryId::DataQuery)
    {
      /*
      auto fparam = req.getParameter("f");
      if(fparam && *fparam != "application/json")
        throw EDRException("Invalid format! Format must be 'application/json' for metadata query!");
      */
      return;
    }

    // From here on EDR data query
    // Querydata and obervation producers do not have dots (.) in id, but grid
    // producers always do have
    bool grid_producer = (itsEDRQuery.collection_id.find('.') != std::string::npos);
    if (!grid_producer)
      req.addParameter("producer", itsEDRQuery.collection_id);

    // If query_type is position, radius, trajectory -> coords is required
    auto coords = Spine::optional_string(req.getParameter("coords"), "");
    if (!coords.empty())
    {
      parseCoords(coords);
    }
    else if (itsEDRQuery.query_type == EDRQueryType::Locations)
    {
      parseLocations(emd);
    }
    else if (itsEDRQuery.query_type == EDRQueryType::Cube)
    {
      parseCube();
    }

    // EDR datetime
    parseDateTime(state, emd);

    // EDR parameter names and z
    auto parameter_names = parseParameterNamesAndZ(state, emd, grid_producer);

    if (parameter_names.empty())
    {
      if (emd.parameters.empty())
        throw EDRException("No metadata for " + itsEDRQuery.collection_id + "!");

      throw EDRException("Missing parameter-name option!");
    }

    // If f-option is misssing, default output format is CoverageJSON
    output_format = Spine::optional_string(req.getParameter("f"), COVERAGE_JSON_FORMAT);

    const auto& supportedOutputFormats =
        config.getSupportedOutputFormats(itsEDRQuery.collection_id);
    if (supportedOutputFormats.find(output_format) == supportedOutputFormats.end())
    {
      auto output_format_list = boost::algorithm::join(supportedOutputFormats, ",");
      throw EDRException("Invalid output format '" + output_format +
                         "'. The following output formats are supported for collection " +
                         itsEDRQuery.collection_id + ": " + output_format_list);
    }

    // Longitude, latitude are always needed
    parameter_names += ",longitude,latitude";
    // If producer has levels we need to query them
    if (!emd.vertical_extent.levels.empty())
      parameter_names += ",level";

    req.addParameter("param", parameter_names);

    auto language = Spine::optional_string(req.getParameter("lang"), config.defaultLanguage());
    itsEDRQuery.language = language;

#ifndef WITHOUT_AVI
    parseICAOCodesAndAviProducer(emd);
#endif
  }
  catch (...)
  {
    // The stack traces are useless when the user has made a typo
    throw Fmi::Exception::Trace(BCP, "EDR plugin failed to parse query string options!")
        .disableStackTrace();
  }
}

bool EDRQueryParams::isAviProducer(const EDRProducerMetaData& avi_metadata,
                                   const std::string& producer) const
{
  auto producer_name = Fmi::trim_copy(boost::algorithm::to_upper_copy(producer));

  return (avi_metadata.find(producer_name) != avi_metadata.end());
}

std::string EDRQueryParams::parseLocations(const State& state,
                                           const std::vector<std::string>& resource_parts)
{
  try
  {
    itsEDRQuery.query_type = to_query_type_id(resource_parts.at(5));

    if (itsEDRQuery.query_type == EDRQueryType::InvalidQueryType)
      return resource_parts.at(5);

    if (itsEDRQuery.query_type == EDRQueryType::Locations)
      itsEDRQuery.query_id = EDRQueryId::SpecifiedCollectionLocations;

    return {};
  }
  catch (...)
  {
    throw Fmi::Exception::Trace(BCP, "Operation failed!");
  }
}

std::string EDRQueryParams::parseInstances(const State& state,
                                           const std::vector<std::string>& resource_parts)
{
  try
  {
    itsEDRQuery.instance_id = resource_parts.at(4);
    itsEDRQuery.query_id = EDRQueryId::SpecifiedCollectionSpecifiedInstance;
    if (itsEDRQuery.query_type == EDRQueryType::Locations && resource_parts.size() != 5)
      throw EDRException(
          "Missing 'locationId' in Locations query! Format "
          "'/edr/collections/{collectionId}/locations/{locationId}'");

    if (resource_parts.size() > 5 && !resource_parts.at(5).empty())
      return parseLocations(state, resource_parts);

    return {};
  }
  catch (...)
  {
    throw Fmi::Exception::Trace(BCP, "Operation failed!");
  }
}

std::string EDRQueryParams::parseResourceParts3AndBeyond(
    const State& state, const std::vector<std::string>& resource_parts)
{
  try
  {
    if (resource_parts.at(3) == "instances")
    {
      itsEDRQuery.query_id = EDRQueryId::SpecifiedCollectionAllInstances;
      if (resource_parts.size() > 4 && !resource_parts.at(4).empty())
        return parseInstances(state, resource_parts);
    }

    itsEDRQuery.query_type = to_query_type_id(resource_parts.at(3));

    if (itsEDRQuery.query_type == EDRQueryType::InvalidQueryType)
      return resource_parts.at(3);

    if (itsEDRQuery.query_type == EDRQueryType::Locations)
      itsEDRQuery.query_id = EDRQueryId::SpecifiedCollectionLocations;

    if (resource_parts.size() > 4)
      itsEDRQuery.instance_id = resource_parts.at(4);

    return {};
  }
  catch (...)
  {
    throw Fmi::Exception::Trace(BCP, "Operation failed!");
  }
}

std::string EDRQueryParams::parseResourceParts2AndBeyond(
    const State& state, const std::vector<std::string>& resource_parts)
{
  try
  {
    itsEDRQuery.collection_id = resource_parts.at(2);

    if (!state.isValidCollection(itsEDRQuery.collection_id))
      throw EDRException(("Collection '" + itsEDRQuery.collection_id + "' not found"));

    itsEDRQuery.query_id = EDRQueryId::SpecifiedCollection;

    if (resource_parts.size() > 3 && !resource_parts.at(3).empty())
      return parseResourceParts3AndBeyond(state, resource_parts);

    return {};
  }
  catch (...)
  {
    throw Fmi::Exception::Trace(BCP, "Operation failed!");
  }
}

std::string EDRQueryParams::parseEDRQuery(const State& state, const std::string& resource)
{
  try
  {
    // If query_type is position, radius, trajectory -> coords is required
    auto coords = Spine::optional_string(req.getParameter("coords"), "");
    if ((itsEDRQuery.query_type == EDRQueryType::Position ||
         itsEDRQuery.query_type == EDRQueryType::Radius ||
         itsEDRQuery.query_type == EDRQueryType::Trajectory ||
         itsEDRQuery.query_type == EDRQueryType::Area ||
         itsEDRQuery.query_type == EDRQueryType::Corridor) &&
        coords.empty())
      throw EDRException(
          "Query parameter 'coords' must be defined for Position, Radius, Trajectory, Area, "
          "Corridor query");

    std::vector<std::string> resource_parts;
    boost::algorithm::split(resource_parts, resource, boost::algorithm::is_any_of("/"));
    if (resource_parts.size() > 1 && resource_parts.front().empty())
      resource_parts.erase(resource_parts.begin());

    if (resource_parts.empty())
      return {};

    // Data query must start with 'edr'
    if (resource_parts.at(0) != "edr")
      return {};

    if (resource_parts.size() == 1)
      return {};

    // First keyword must be collections
    if (resource_parts.at(1) != "collections")
      throw EDRException(("Invalid path '/edr/" + resource_parts.at(1) + "'"));

    // By default get all collections
    itsEDRQuery.query_id = EDRQueryId::AllCollections;

    if (resource_parts.size() > 2 && !resource_parts.at(2).empty())
      return parseResourceParts2AndBeyond(state, resource_parts);

    return {};
  }
  catch (...)
  {
    throw Fmi::Exception::Trace(BCP, "Operation failed!");
  }
}

std::string EDRQueryParams::parsePosition(const std::string& coords)
{
  try
  {
    auto wkt = coords;

    // If query_type is Position and MULTIPOINTZ
    boost::algorithm::to_upper(wkt);
    boost::algorithm::trim(wkt);
    auto geometry_name = wkt.substr(0, wkt.find('('));
    if (geometry_name == "MULTIPOINTZ")
    {
      boost::algorithm::trim(geometry_name);
      auto len = (wkt.rfind(')') - wkt.find('(') - 1);
      auto geometry_values = wkt.substr(wkt.find('(') + 1, len);
      std::vector<std::string> coordinates;
      boost::algorithm::split(coordinates, geometry_values, boost::algorithm::is_any_of(","));

      wkt = "MULTIPOINT(";
      for (auto& coordinate_item : coordinates)
      {
        boost::algorithm::trim(coordinate_item);
        if (coordinate_item.front() == '(' && coordinate_item.back() == ')')
          coordinate_item = coordinate_item.substr(1, coordinate_item.length() - 2);
        std::vector<std::string> parts;
        boost::algorithm::split(parts, coordinate_item, boost::algorithm::is_any_of(" "));
        if (parts.size() != 3)
          throw EDRException(
              "Invalid MULTIPOINTZ definition, longitude, latitude, level expected: " + coords);
        wkt.append(parts[0] + " " + parts[1] + ",");
        itsCoordinateFilter.add(Fmi::stod(parts[0]), Fmi::stod(parts[1]), Fmi::stod(parts[2]));
      }
      wkt.resize(wkt.size() - 1);
      wkt.append(")");
    }

    return wkt;
  }
  catch (...)
  {
    throw Fmi::Exception::Trace(BCP, "Operation failed!");
  }
}

std::string EDRQueryParams::parseTrajectoryAndCorridor(const std::string& coords)
{
  try
  {
    auto wkt = coords;

    // If query_type is Trajectory, check if is 2D, 3D, 4D
    boost::algorithm::to_upper(wkt);
    boost::algorithm::trim(wkt);
    auto geometry_name = wkt.substr(0, wkt.find('('));
    boost::algorithm::trim(geometry_name);
    auto len = (wkt.rfind(')') - wkt.find('(') - 1);
    auto geometry_values = wkt.substr(wkt.find('(') + 1, len);
    auto radius = wkt.substr(wkt.rfind(')') + 1);

    wkt = "LINESTRING(";
    if (geometry_name == "LINESTRINGZM")
    {
      // lon,lat,level,epoch time
      std::vector<std::string> coordinates;
      boost::algorithm::split(coordinates, geometry_values, boost::algorithm::is_any_of(","));
      for (const auto& coordinate_item : coordinates)
      {
        std::vector<std::string> parts;
        boost::algorithm::split(parts, coordinate_item, boost::algorithm::is_any_of(" "));
        if (parts.size() != 4)
          throw EDRException(
              "Invalid LINESTRINGZM definition, longitude, latitude, level, epoch time "
              "expected: " +
              coords);
        wkt.append(parts[0] + " " + parts[1] + ",");
        itsCoordinateFilter.add(Fmi::stod(parts[0]),
                                Fmi::stod(parts[1]),
                                Fmi::stod(parts[2]),
                                Fmi::date_time::from_time_t(Fmi::stod(parts[3])));
      }
    }
    else if (geometry_name == "LINESTRINGZ")
    {
      // lon,lat,level
      std::vector<std::string> coordinates;
      boost::algorithm::split(coordinates, geometry_values, boost::algorithm::is_any_of(","));
      for (const auto& item : coordinates)
      {
        std::vector<std::string> parts;
        boost::algorithm::split(parts, item, boost::algorithm::is_any_of(" "));
        if (parts.size() != 3)
          throw EDRException(
              "Invalid LINESTRINGZ definition, longitude, latitude, level expected: " + coords);
        wkt.append(parts[0] + " " + parts[1] + ",");
        itsCoordinateFilter.add(Fmi::stod(parts[0]), Fmi::stod(parts[1]), Fmi::stod(parts[2]));
      }
    }
    else if (geometry_name == "LINESTRINGM")
    {
      // lon,lat,epoch time
      std::vector<std::string> coordinates;
      boost::algorithm::split(coordinates, geometry_values, boost::algorithm::is_any_of(","));
      for (const auto& item : coordinates)
      {
        std::vector<std::string> parts;
        boost::algorithm::split(parts, item, boost::algorithm::is_any_of(" "));
        if (parts.size() != 3)
          throw EDRException(
              "Invalid LINESTRINGM definition, longitude, latitude, epoch time expected: " +
              coords);
        wkt.append(parts[0] + " " + parts[1] + ",");
        itsCoordinateFilter.add(Fmi::stod(parts[0]),
                                Fmi::stod(parts[1]),
                                Fmi::date_time::from_time_t(Fmi::stod(parts[2])));
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

    return wkt;
  }
  catch (...)
  {
    throw Fmi::Exception::Trace(BCP, "Operation failed!");
  }
}

void EDRQueryParams::parseCoords(const std::string& coordinates)
{
  try
  {
    auto coords = coordinates;
    // EDR within + within-units
    auto within = Spine::optional_string(req.getParameter("within"), "");
    auto within_units = Spine::optional_string(req.getParameter("within-units"), "");

    if (itsEDRQuery.query_type == EDRQueryType::Radius && (within.empty() || within_units.empty()))
      throw EDRException(
          "Query parameter 'within' and 'within-units' must be defined for Radius query");

    if (itsEDRQuery.query_type == EDRQueryType::Corridor)
    {
      auto corridor_width = Spine::optional_string(req.getParameter("corridor-width"), "");
      auto width_units = Spine::optional_string(req.getParameter("width-units"), "");
      if (corridor_width.empty() || width_units.empty())
        throw EDRException(
            "Query parameter 'corridor-width' and 'width-units' must be defined for Corridor "
            "query");

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
        throw EDRException("Invalid within-units option '" + within_units +
                           "' used, 'km' and 'mi' supported");
      coords += (":" + Fmi::to_string(radius));
    }

    auto wkt = coords;
    if (itsEDRQuery.query_type == EDRQueryType::Position)
      wkt = parsePosition(coords);
    else if (itsEDRQuery.query_type == EDRQueryType::Trajectory ||
             itsEDRQuery.query_type == EDRQueryType::Corridor)
      wkt = parseTrajectoryAndCorridor(coords);
    else if (itsEDRQuery.query_type == EDRQueryType::Cube)
    {
      auto minz = Spine::optional_string(req.getParameter("minz"), "");
      auto maxz = Spine::optional_string(req.getParameter("minz"), "");
      if (minz.empty() || maxz.empty())
        throw EDRException("No minz, maxz parameter defined for Cube query!");
      req.addParameter("z", minz + "/" + maxz);
    }

    auto crs = Spine::optional_string(req.getParameter("crs"), "EPSG:4326");
    if (!crs.empty() && crs != "EPSG:4326" && crs != "WGS84" && crs != "CRS84" && crs != "CRS:84")
      throw EDRException("Invalid crs: " + crs + ". Only EPSG:4326 is supported");
    crs = "EPSG:4326";

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
  catch (...)
  {
    throw Fmi::Exception::Trace(BCP, "Operation failed!");
  }
}

void EDRQueryParams::parseLocations(const EDRMetaData& emd)
{
  try
  {
    if (!emd.locations)
      throw EDRException("Location query is not supported by collection '" +
                         itsEDRQuery.collection_id + "'");

    auto location_id = itsEDRQuery.instance_id;

    if (emd.locations->find(location_id) == emd.locations->end())
    {
      throw EDRException("Invalid locationId '" + location_id + "' for collection '" +
                         itsEDRQuery.collection_id +
                         "'! Please check metadata for valid locationIds!");
    }

    const auto& location_info = emd.locations->at(location_id);

    // Currently locationId is either fmisid, geoid or icao code
    if (location_info.type == "ICAO")
      req.addParameter("icao", location_id);
    else if (location_info.type == "fmisid")
      req.addParameter("fmisid", location_id);
    else
      req.addParameter("geoid", location_id);
  }
  catch (...)
  {
    throw Fmi::Exception::Trace(BCP, "Operation failed!");
  }
}

void EDRQueryParams::parseCube()
{
  try
  {
    auto bbox = Spine::optional_string(req.getParameter("bbox"), "");
    if (bbox.empty())
      throw EDRException("Query parameter 'bbox' or 'coords' must be defined for Cube");

    std::vector<std::string> parts;
    boost::algorithm::split(parts, bbox, boost::algorithm::is_any_of(","));
    if (parts.size() != 4)
      throw EDRException(
          "Invalid bbox parameter, format is bbox=lower left corner x,lower left corner "
          "y,upper left corner x,upper left corner y"
          "<upper right coordinate>, e.g.  bbox=19.4,59.6,31.6,70.1");
    auto lower_left_x = parts[0];
    auto lower_left_y = parts[1];
    auto upper_right_x = parts[2];
    auto upper_right_y = parts[3];
    boost::algorithm::trim(lower_left_x);
    boost::algorithm::trim(lower_left_y);
    boost::algorithm::trim(upper_right_x);
    boost::algorithm::trim(upper_right_y);
    auto wkt = ("POLYGON((" + lower_left_x + " " + lower_left_y + "," + lower_left_x + " " +
                upper_right_y + "," + upper_right_x + " " + upper_right_y + "," + upper_right_x +
                " " + lower_left_y + "," + lower_left_x + " " + lower_left_y + "))");
    req.addParameter("wkt", wkt);
    req.removeParameter("bbox");
  }
  catch (...)
  {
    throw Fmi::Exception::Trace(BCP, "Operation failed!");
  }
}

void EDRQueryParams::parseDateTime(const State& state, const EDRMetaData& emd)
{
  try
  {
    // EDR datetime
    auto datetime = Spine::optional_string(req.getParameter("datetime"), "");

    if (datetime == "null")
    {
      if (emd.temporal_extent.time_periods.empty())
      {
        datetime = Fmi::to_iso_string(Fmi::SecondClock::universal_time());
      }
      else
      {
        if (emd.temporal_extent.time_periods.back().end_time.is_not_a_date_time())
          datetime = (Fmi::to_iso_string(emd.temporal_extent.time_periods.front().start_time) +
                      "/" + Fmi::to_iso_string(emd.temporal_extent.time_periods.back().start_time));
        else
          datetime = (Fmi::to_iso_string(emd.temporal_extent.time_periods.front().start_time) +
                      "/" + Fmi::to_iso_string(emd.temporal_extent.time_periods.back().end_time));
      }
    }

    if (datetime.empty())
      datetime = itsCoordinateFilter.getDatetime();

#ifndef WITHOUT_AVI
    if (datetime.empty() && isAviProducer(state.getAviMetaData(), itsEDRQuery.collection_id))
      datetime = Fmi::to_iso_string(Fmi::SecondClock::universal_time());
#endif

    if (datetime.empty())
      throw EDRException("Missing datetime option");

    if (datetime.find('/') != std::string::npos)
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
  }
  catch (...)
  {
    throw Fmi::Exception::Trace(BCP, "Operation failed!");
  }
}

void EDRQueryParams::handleGridParameter(std::string& p,
                                         const std::string& producerId,
                                         const std::string& geometryId,
                                         const std::string& levelId,
                                         const std::string& z) const
{
  try
  {
    p.append(":" + producerId);
    if (!geometryId.empty())
      p.append(":" + geometryId);
    if (!levelId.empty())
      p.append(":" + levelId);
    if (!z.empty())
      p.append(":" + z);
  }
  catch (...)
  {
    throw Fmi::Exception::Trace(BCP, "Operation failed!");
  }
}

std::string EDRQueryParams::cleanParameterNames(const std::string& parameter_names,
                                                const EDRMetaData& emd,
                                                bool grid_producer,
                                                const std::string& z) const
{
  try
  {
    std::string producerId;
    std::string geometryId;
    std::string levelId;
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

    std::string cleaned_param_names;

    if (parameter_names.empty())
    {
      for (const auto &param : emd.parameters)
      {
        if (!cleaned_param_names.empty())
          cleaned_param_names += ",";

        if (!grid_producer)
        {
          cleaned_param_names += param.first;
          continue;
        }

        std::string p(param.first);
        handleGridParameter(p, producerId, geometryId, levelId, z);

        cleaned_param_names += p;
      }

      return cleaned_param_names;
    }

    std::list<std::string> param_names;
    boost::algorithm::split(param_names, parameter_names, boost::algorithm::is_any_of(","));

    for (auto p : param_names)
    {
      boost::algorithm::to_lower(p);
      if (p.find(':') != std::string::npos)
        p = p.substr(0, p.find(':'));
      if (emd.parameters.find(p) == emd.parameters.end())
      {
        std::cerr << (Spine::log_time_str() + ANSI_FG_MAGENTA + " [edr] Unknown parameter '" + p +
                      "' ignored!" + ANSI_FG_DEFAULT)
                  << std::endl;
      }
      if (emd.parameters.find(p) != emd.parameters.end())
      {
        if (!cleaned_param_names.empty())
          cleaned_param_names += ",";

        if (grid_producer)
          handleGridParameter(p, producerId, geometryId, levelId, z);

        cleaned_param_names += p;
      }
    }
    return cleaned_param_names;
  }
  catch (...)
  {
    throw Fmi::Exception::Trace(BCP, "Operation failed!");
  }
}

std::string EDRQueryParams::parseParameterNamesAndZ(const State& state,
                                                    const EDRMetaData& emd,
                                                    bool grid_producer)
{
  try
  {
    auto z = Spine::optional_string(req.getParameter("z"), "");
    double min_level = std::numeric_limits<double>::min();
    double max_level = std::numeric_limits<double>::max();
    bool range = false;
    if (z.find('/') != std::string::npos)
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
    if (z.empty() && !emd.vertical_extent.levels.empty())
    {
      for (const auto& l : emd.vertical_extent.levels)
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

    // EDR parameter-name
    auto parameter_names = Spine::optional_string(req.getParameter("parameter-name"), "");

#ifndef WITHOUT_AVI
    if (parameter_names.empty() && isAviProducer(state.getAviMetaData(), itsEDRQuery.collection_id))
    {
      parameter_names = "message";
      req.addParameter("parameter-name", parameter_names);
    }
#endif
    return cleanParameterNames(parameter_names, emd, grid_producer, z);
  }
  catch (...)
  {
    throw Fmi::Exception::Trace(BCP, "Operation failed!");
  }
}

void EDRQueryParams::parseICAOCodesAndAviProducer(const EDRMetaData& emd)
{
  try
  {
    auto icao_ids = req.getParameter("icao");
    if (icao_ids)
    {
      const std::string icaoreq = *icao_ids;
      std::vector<std::string> parts;
      boost::algorithm::split(parts, icaoreq, boost::algorithm::is_any_of(","));
      icaos.insert(this->icaos.begin(), parts.begin(), parts.end());
    }

    if (emd.isAviProducer())
      requestWKT = Spine::optional_string(req.getParameter("wkt"), "");
  }
  catch (...)
  {
    throw Fmi::Exception::Trace(BCP, "Operation failed!");
  }
}

}  // namespace EDR
}  // namespace Plugin
}  // namespace SmartMet
