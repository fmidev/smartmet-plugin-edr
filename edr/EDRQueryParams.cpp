#include "EDRQueryParams.h"
#include "EDRDefs.h"
#include "EDRMetaData.h"
#include "Plugin.h"
#include "UtilityFunctions.h"
#include <engines/observation/Keywords.h>
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
                   const EDRQuery& /* edrQuery */,
                   const EDRMetaData& /* edrMetaData */)
{
  try
  {
    // Metadata query:
    //
    //   ** uri field count and index: uri parts are ignored upto 'collections'
    //
    //        1            2       [ 3        [ 4         ]][ 3/5      ]] ** count
    //        0            1       [ 2        [ 3         ]][ 2/4      ]] ** index
    //   /edr/collections[/COLLNAME[/instances[/INSTANCEID]][/locations]]
    //
    // Data query: other cases
    //
    //   /edr/collections/COLLNAME[/instances/INSTANCEID]/location/LOCID?...
    //   /edr/collections/COLLNAME[/instances/INSTANCEID]/{position|area|...}?...
    //
    // Data query types (wkt may be passed thru as is):
    //
    //   position   coords=wkt|POINT|MULTIPOINT|MULTIPOINTZ
    //   area       coords=wkt|POLYGON|MULTIPOLYGON&z=lo/hi
    //   corridor   coords=LINESTRING[ZM|Z|M]&corridor-width=width&width-units={km|mi}
    //   radius     coords=wkt|POINT|MULTIPOINT|MULTIPOINTZ&within=radius&within-units={km|mi}
    //   trajectory coords=LINESTRING[ZM|Z|M]
    //   cube       coords=wkt|POLYGON&minz=lo&maxz=hi|bbox=bllon,bllat,trlon,trlat&z=lo/hi
    //
    //   locations/LOCID

    std::vector<std::string> resParts;
    std::string res = req.getResource();
    boost::algorithm::split(resParts, res, boost::algorithm::is_any_of("/"));
    auto numParts = resParts.size();
    size_t resIdx = 0;
    size_t idx = 0;

    for (auto const& part : resParts)
    {
      if (part == "collections")
      {
        resIdx = idx;
        break;
      }

      idx++;
    }

    if (resIdx == 0)
      throw EDRException("URI has no 'collections' part");

    if ((numParts > 0) && (resParts[numParts - 1].empty()))
      numParts--;

    auto lastIdx = numParts - 1;

    if ((lastIdx - resIdx) < 2)
      return false;

    resIdx += ((resParts[resIdx + 2] == "instances") ? 4 : 2);

    if ((lastIdx < resIdx) || ((lastIdx == resIdx) && (resParts[resIdx] == "locations")))
      return false;

    return true;
  }
  catch (...)
  {
    throw Fmi::Exception::Trace(BCP, "Resolving host failed!");
  }
}

std::string resolve_host(const Spine::HTTP::Request& theRequest, const std::string& base_url)
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
    std::optional<std::string> apikey;
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

    return (protocol + host + base_url);
  }
  catch (...)
  {
    throw Fmi::Exception::Trace(BCP, "Resolving host failed!");
  }
}

std::string utcDateTime(const Fmi::DateTime& datetime)
{
  if (datetime.is_not_a_date_time())
    return "not_a_datetime";

  return Fmi::to_iso_string(datetime) + "Z";
}

std::string nearestTimeNow(const EDRMetaData& emd, const std::string& datetime = "")
{
  Fmi::DateTime now;

  if (datetime.empty())
    now = Fmi::SecondClock::universal_time();
  else
    now = Fmi::DateTime::from_iso_string(datetime);

  // No time periods (never, should not be called if so)
  //
  if (emd.temporal_extent.time_periods.empty())
    return utcDateTime(now);

  // Note: single time instansts are always used

  auto const& time_periods =
      (emd.temporal_extent.single_time_periods.empty() ? emd.temporal_extent.time_periods
                                                       : emd.temporal_extent.single_time_periods);

  // Before (or match) 1'st time period start ?
  //
  if (now <= time_periods.front().start_time)
    return utcDateTime(time_periods.front().start_time);

  const edr_temporal_extent_period* tp1 = nullptr;
  const edr_temporal_extent_period* tp2 = nullptr;

  // After (or match) last time period (start or) end ?
  //
  if (now >= time_periods.back().start_time)
  {
    if (time_periods.back().end_time.is_not_a_date_time())
      return utcDateTime(time_periods.back().start_time);

    if (now >= time_periods.back().end_time)
      return utcDateTime(time_periods.back().end_time);

    tp1 = tp2 = &time_periods.back();
  }
  else
  {
    // Search for time period(s) within or between which current time is

    for (const auto& tp : time_periods)
    {
      auto const& t1 = tp.start_time;
      auto const& t2 = (tp.end_time.is_not_a_date_time() ? t1 : tp.end_time);

      // After period's end ?
      //
      if (t2 < now)
      {
        tp1 = &tp;
        continue;
      }

      // Match start or end ?
      //
      if ((t1 == now) || (t2 == now))
        return utcDateTime(now);

      // Before period's end
      //
      tp2 = &tp;

      break;
    }

    // No time period(s) found (never) ?
    //
    if ((!tp1) && (!tp2))
      return utcDateTime(now);

    if (!tp1)
      tp1 = tp2;
    else if (!tp2)
      tp2 = tp1;
  }

  // Between 2 time periods ?
  //
  if (tp1 != tp2)
  {
    auto const& t1 = (tp1->end_time.is_not_a_date_time() ? tp1->start_time : tp1->end_time);
    auto const& t2 = tp2->start_time;

    auto td1 = now - t1;
    auto td2 = t2 - now;

    return utcDateTime((td1 > td2) ? t2 : t1);
  }

  // Within time period or after last time instant
  //
  // No period end time ?
  //
  if (tp1->end_time.is_not_a_date_time())
    return utcDateTime(tp1->start_time);

  // No time step ?
  //
  if (tp1->timestep <= 0)
  {
    auto td1 = now - tp1->start_time;
    auto td2 = tp1->end_time - now;

    return utcDateTime((td1 > td2) ? tp1->end_time : tp1->start_time);
  }

  // Nearest timestep

  auto diffminutes = (now - tp1->start_time).total_minutes();
  auto stepminutes = (diffminutes / tp1->timestep) * tp1->timestep;
  auto t = tp1->start_time + Fmi::date_time::Minutes(stepminutes);

  if ((2 * (diffminutes - stepminutes)) >= tp1->timestep)
  {
    auto t2 = t + Fmi::date_time::Minutes(tp1->timestep);

    if (t2 <= tp1->end_time)
      t = t2;
  }

  return utcDateTime(t);
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
    itsEDRQuery.host = resolve_host(req, config.defaultUrl());

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

    auto invalid_query_type = parseEDRQuery(state, config, resource);

    EDRMetaData emd = state.getProducerMetaData(itsEDRQuery.collection_id);

    // Allow missing vertical extent when processing cube query for non grid source data
    // (e.g. observations, surface querydata and aviation messages; BRAINSTORM-2827)

    if (emd.vertical_extent.levels.empty() && itsEDRQuery.query_type == EDRQueryType::Cube &&
        emd.metadata_source == SourceEngine::Grid)
      throw EDRException("Error! Cube query not possible for '" + itsEDRQuery.collection_id +
                         "', because there is no vertical extent!");

    if (is_data_query(req, itsEDRQuery, emd))
    {
      if (!itsEDRQuery.instance_id.empty())
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
      parseLocations(emd, coords);
    }
    else if (itsEDRQuery.query_type == EDRQueryType::Cube)
    {
      parseCube();
    }
    else if ((itsEDRQuery.query_type == EDRQueryType::Position) ||
             (itsEDRQuery.query_type == EDRQueryType::Radius) ||
             (itsEDRQuery.query_type == EDRQueryType::Area) ||
             (itsEDRQuery.query_type == EDRQueryType::Trajectory) ||
             (itsEDRQuery.query_type == EDRQueryType::Corridor))
      throw EDRException("Missing coords option!");

    // EDR datetime
    parseDateTime(state, itsEDRQuery.collection_id, emd);

    // (MetOcean profile) custom dimensions to filter parameters
    CustomDimensions customDimensions(req);

    // EDR parameter names and z
    bool noReqParams = false;
    auto parameter_names = parseParameterNamesAndZ(
        state, emd, grid_producer, noReqParams, customDimensions);

    if (parameter_names.empty())
    {
      if (emd.parameters.empty())
        throw EDRException("No metadata for " + itsEDRQuery.collection_id + "!");

      throw EDRException("No matching parameter names given!");
    }

    // If f-option is missing, use default output format
    output_format = Spine::optional_string(req.getParameter("f"), emd.default_output_format);

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
    {
      // BRAINSTORM-3116 'level' kludge, FIXME !
      //
      if (itsEDRQuery.collection_id != SOUNDING_PRODUCER)
        parameter_names += ",level";
      else
        parameter_names += ",edr_observation_level";
    }

    req.addParameter("param", parameter_names);

    auto language = Spine::optional_string(req.getParameter("lang"), config.defaultLanguage());
    itsEDRQuery.language = language;

#ifndef WITHOUT_AVI
    parseICAOCodesAndAviProducer(emd);
#endif

    validateRequestParametersWithMetaData(emd);
  }
  catch (...)
  {
    // The stack traces are useless when the user has made a typo
    throw Fmi::Exception::Trace(BCP, "EDR plugin failed to parse query string options!")
        .disableStackTrace();
  }
}

bool EDRQueryParams::isAviProducer(const EDRProducerMetaData& avi_metadata,
                                   const std::string& producer)
{
  // BRAINSTORM-3274; Using lower case for collection names
  //
  auto producer_name = Fmi::trim_copy(boost::algorithm::to_lower_copy(producer));

  return (avi_metadata.find(producer_name) != avi_metadata.end());
}

std::string EDRQueryParams::parseLocations(const State& /* state */,
                                           const std::vector<std::string>& resource_parts)
{
  try
  {
    itsEDRQuery.query_type = to_query_type_id(resource_parts.at(5));

    if (itsEDRQuery.query_type == EDRQueryType::InvalidQueryType)
      return resource_parts.at(5);

    if (itsEDRQuery.query_type == EDRQueryType::Locations)
    {
      itsEDRQuery.query_id = EDRQueryId::SpecifiedCollectionLocations;

      if (resource_parts.size() > 6)
        itsEDRQuery.location_id = resource_parts.at(6);
    }

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

    if (!state.getPlugin().isValidInstance(itsEDRQuery.collection_id, itsEDRQuery.instance_id))
      throw EDRException("Instance id '" + itsEDRQuery.instance_id + "' is not valid");

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
    auto instances = (resource_parts.at(3) == "instances");

    if (instances)
    {
      auto epmd = state.getProducerMetaData(resource_parts.at(2));
      if (!epmd.sourceHasInstances())
        throw EDRException("Collection '" + resource_parts.at(2) + "' does not have instances!");

      itsEDRQuery.query_id = EDRQueryId::SpecifiedCollectionAllInstances;
      if (resource_parts.size() > 4 && !resource_parts.at(4).empty())
        return parseInstances(state, resource_parts);
    }

    itsEDRQuery.query_type = to_query_type_id(resource_parts.at(3));

    if (itsEDRQuery.query_type == EDRQueryType::InvalidQueryType)
      return resource_parts.at(3);

    if (itsEDRQuery.query_type == EDRQueryType::Locations)
    {
      itsEDRQuery.query_id = EDRQueryId::SpecifiedCollectionLocations;

      if (resource_parts.size() > 4)
        itsEDRQuery.location_id = resource_parts.at(4);
    }

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
    // BRAINSTORM-3274; Using lower case for collection names
    //
    itsEDRQuery.collection_id = boost::algorithm::to_lower_copy(resource_parts.at(2));

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

std::string EDRQueryParams::parseEDRQuery(const State& state,
                                          const Config& config,
                                          const std::string& resource)
{
  try
  {
    // Omit baseurl when parsing the uri.
    //
    // Initially base url was expected to be "/edr" but now it may be other/longer too;
    // drop off 'extra' uri parts (if any) from the start, leaving an array starting
    // from fixed 'edr' to satisfy parsing of the rest of the uri

    const std::string& baseUrl = config.defaultUrl();

    auto pos = resource.find(baseUrl);
    if (pos == std::string::npos)
      throw EDRException("URI does not match registration url");

    std::vector<std::string> resource_parts;
    std::string edrResource = "edr" + resource.substr(pos + baseUrl.size());
    boost::algorithm::split(resource_parts, edrResource, boost::algorithm::is_any_of("/"));
    if (resource_parts.size() > 1 && resource_parts.back().empty())
      resource_parts.pop_back();

    if (resource_parts.size() < 2)
      return {};

    // First keyword must be collections
    if (resource_parts.at(1) != "collections")
      throw EDRException("Invalid path '" + resource + "'");

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
      auto z = Spine::optional_string(req.getParameter("z"), "");
      bool zIsRange = false;
      std::string zLo;
      std::string zHi;

      UtilityFunctions::parseRangeListValue(z, zIsRange, zLo, zHi);
    }

    auto crs = Spine::optional_string(req.getParameter("crs"), "OGC:CRS84");
    if (!crs.empty() && crs != "OGC:CRS84" && crs != "CRS:84")
      throw EDRException("Invalid crs: " + crs + ". Only OGC:CRS84 is supported");
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

void EDRQueryParams::parseLocations(const EDRMetaData& emd, std::string& coords)
{
  try
  {
    if (!emd.locations)
      throw EDRException("Location query is not supported by collection '" +
                         itsEDRQuery.collection_id + "'");

    auto location_id = itsEDRQuery.location_id;

    if (location_id == "all")
    {
      // BRAINSTORM-3288
      //
      // Convert location "all" query to area query using a small buffer (0.1 degrees)
      //
      // Note: removed initial buffering by adding ":1" to the end of the wkt due to crashes in
      //       gis -engine since it resulted to 1000 degrees instead of meters. The purpose was
      //       only to pass on the buffering to avi engine query by later extracting the buffering
      //       value from the wkt and passing it on to avi -engine as maxdistance
      //
      itsEDRQuery.query_type = EDRQueryType::Area;

      auto xmin(Fmi::to_string(emd.spatial_extent.bbox_xmin - 0.1) + " ");
      auto ymin(Fmi::to_string(emd.spatial_extent.bbox_ymin - 0.1) + ",");
      auto xmax(Fmi::to_string(emd.spatial_extent.bbox_xmax + 0.1) + " ");
      auto ymax(Fmi::to_string(emd.spatial_extent.bbox_ymax + 0.1) + ",");

      coords = "POLYGON((" + xmin + ymin + xmax + ymin + xmax + ymax + xmin + ymax + xmin + ymin;
      coords.pop_back();
      coords += "))";

      parseCoords(coords);

      return;
    }

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

    auto z = Fmi::trim_copy(Spine::optional_string(req.getParameter("z"), ""));
    bool zIsRange = false;
    std::string zLo;
    std::string zHi;

    auto parts = UtilityFunctions::parseBBoxAndZ(bbox, z, zIsRange, zLo, zHi);
    size_t idx = ((parts.size() == 4) ? 2 : 3);

    const auto& lower_left_x = parts[0];
    const auto& lower_left_y = parts[1];
    const auto& upper_right_x = parts[idx++];
    const auto& upper_right_y = parts[idx++];
    auto wkt = ("POLYGON((" + lower_left_x + " " + lower_left_y + "," + lower_left_x + " " +
                upper_right_y + "," + upper_right_x + " " + upper_right_y + "," + upper_right_x +
                " " + lower_left_y + "," + lower_left_x + " " + lower_left_y + "))");
    req.addParameter("wkt", wkt);

    if (z.empty() && zIsRange)
      // z range from bbox
      req.addParameter("z", zLo + "/" + zHi);

    req.removeParameter("bbox");
  }
  catch (...)
  {
    throw Fmi::Exception::Trace(BCP, "Operation failed!");
  }
}

void EDRQueryParams::parseDateTime(const State& /* state */,
                                   const std::string& /* producer */,
                                   const EDRMetaData& emd)
{
  try
  {
    // EDR datetime
    //
    // BRAINSTORM-3272; Removed nonstd "&datetime=null" handling and allowing missing
    //                  datetime option; if missing and is not set by coordinatefilter,
    //                  using nearest metadata time instant to current time by default.
    //
    auto datetime = Spine::optional_string(req.getParameter("datetime"), "");

    if (datetime.empty())
      datetime = itsCoordinateFilter.getDatetime();

    if (datetime.empty())
    {
      if (emd.temporal_extent.time_periods.empty())
      {
        datetime = utcDateTime(Fmi::SecondClock::universal_time());
      }
      else
      {
        // Using current time for avi collections and neareast metadata time instant
        // to current time for others

        if (emd.isAviProducer())
        {
          datetime = Fmi::SecondClock::universal_time().to_iso_string();
          req.addParameter("usecurrenttime", "1");
        }
        else
          datetime = nearestTimeNow(emd);
      }
    }

    if (datetime.empty())
      throw EDRException("Missing datetime option");

    // https://docs.ogc.org/is/19-086r6/19-086r6.html#req_core_rc-time-response
    // Do not allow use of durations
    if (datetime.find('/') != std::string::npos)
    {
      std::vector<std::string> datetime_parts;
      boost::algorithm::split(datetime_parts, datetime, boost::algorithm::is_any_of("/"));
      if (datetime_parts.size() !=
          2)  // Cannot have size()==1, because above check. size() > 2 is however not OK
        throw EDRException("Invalid 'datetime' parameter value '" + datetime + "'");
      auto starttime = datetime_parts.at(0);
      auto endtime = datetime_parts.at(1);

      bool countError = datetime_parts.size() != 2;
      bool startError = !starttime.empty() && (toupper(starttime[0]) == 'P');
      bool endError = !endtime.empty() && (toupper(endtime[0]) == 'P');

      if (countError)
        throw EDRException("Invalid 'datetime' parameter value '" + datetime + "'");

      if (startError)
        throw EDRException("Invalid 'starttime' parameter value '" + starttime + "'");

      if (endError)
        throw EDRException("Invalid 'endtime' parameter value '" + endtime + "'");

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

      if (!datetime.empty() && (toupper(datetime[0]) == 'P'))
        throw EDRException("Invalid 'datetime' parameter value '" + datetime + "'");
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
                                         const std::string& z)
{
  try
  {
    p.append(":" + producerId);
    if (!geometryId.empty())
      p.append(":" + geometryId);
    if (!levelId.empty())
      p.append(":" + levelId);
    if (!z.empty())
    {
      std::vector<std::string> levels;
      std::string param = p;
      p.clear();

      boost::algorithm::split(levels, z, boost::algorithm::is_any_of(","));
      for (auto const& level : levels)
        p.append(param + ":" + level + ",");

      p.pop_back();
    }
  }
  catch (...)
  {
    throw Fmi::Exception::Trace(BCP, "Operation failed!");
  }
}

EDRQueryParams::CustomDimensions::CustomDimensions(const Spine::HTTP::Request& req)
{
  try
  {
    auto req_standard_names =
      boost::algorithm::trim_copy(Spine::optional_string(req.getParameter("standard_name"), ""));
    auto req_methods =
      boost::algorithm::trim_copy(Spine::optional_string(req.getParameter("method"), ""));
    auto req_durations =
      boost::algorithm::trim_copy(Spine::optional_string(req.getParameter("duration"), ""));
    auto req_levels =
      boost::algorithm::trim_copy(Spine::optional_string(req.getParameter("level"), ""));

    if (! req_standard_names.empty())
    {
      std::vector<std::string> standard_name_vec;
      boost::algorithm::split(
          standard_name_vec, req_standard_names, boost::algorithm::is_any_of(","));

      for (auto const &standard_name : standard_name_vec)
      {
        auto s = boost::algorithm::trim_copy(standard_name);
        if (s.empty())
          throw EDRException("Empty standard_name value not allowed");
        standard_names.emplace_back(boost::algorithm::to_lower_copy(s));
      }
    }

    if (! req_methods.empty())
    {
      std::vector<std::string> method_vec;
      boost::algorithm::split(method_vec, req_methods, boost::algorithm::is_any_of(","));

      for (auto const &method : method_vec)
      {
        auto s = boost::algorithm::trim_copy(method);
        if (s.empty())
          throw EDRException("Empty method value not allowed");
        methods.emplace_back(boost::algorithm::to_lower_copy(s));
      }
    }

    if (! req_durations.empty())
    {
      std::vector<std::string> duration_vec;
      boost::algorithm::split(duration_vec, req_durations, boost::algorithm::is_any_of(","));

      for (auto const &duration : duration_vec)
      {
        auto s = boost::algorithm::trim_copy(duration);
        if (s.empty())
          throw EDRException("Empty duration value not allowed");
        durations.emplace_back(boost::algorithm::to_lower_copy(s));
      }
    }

    if (! req_levels.empty())
    {
      std::vector<std::string> level_vec;
      boost::algorithm::split(level_vec, req_levels, boost::algorithm::is_any_of(","));

      for (auto const &level : level_vec)
      {
        auto s = boost::algorithm::trim_copy(level);
        if (s.empty())
          throw EDRException("Empty level value not allowed");
        try
        {
          levels.push_back(atof(s.c_str()));
        }
        catch (...)
        {
          throw EDRException("Invalid level value '" + s + "'");
        }
      }
    }
  }
  catch (...)
  {
    throw Fmi::Exception::Trace(BCP, "Operation failed!");
  }
}

bool EDRQueryParams::CustomDimensions::matches(const std::string &parameterName,
                                               const ParameterInfo &parameterInfo) const
{
  try
  {
    if (standard_names.empty() && methods.empty() && durations.empty() && levels.empty())
      return true;

    auto it = parameterInfo.find(parameterName);
    if ((it == parameterInfo.end()) || it->second.metocean.standard_name.empty())
      return false;

    auto const &stdname = it->second.metocean.standard_name;

    if (
        (! standard_names.empty()) &&
        (std::find(standard_names.begin(), standard_names.end(), stdname) == standard_names.end())
       )
      return false;

    auto const &method = it->second.metocean.method;

    if (
        (! methods.empty()) &&
        (std::find(methods.begin(), methods.end(), method) == methods.end())
       )
      return false;

    auto const &duration = it->second.metocean.duration;

    if (
        (! durations.empty()) &&
        (std::find(durations.begin(), durations.end(), duration) == durations.end())
       )
      return false;

    auto level = it->second.metocean.level;

    if ((! levels.empty()) && (std::find(levels.begin(), levels.end(), level) == levels.end()))
      return false;

    return true;
  }
  catch (...)
  {
    throw Fmi::Exception::Trace(BCP, "Operation failed!");
  }
}

std::string EDRQueryParams::cleanParameterNames(const std::string& parameter_names,
                                                const EDRMetaData& emd,
                                                bool grid_producer,
                                                const std::string& z,
                                                const ParameterInfo &parameterInfo,
                                                const CustomDimensions &customDimensions) const
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
      for (const auto& param : emd.parameters)
      {
        if (! customDimensions.matches(param.first, parameterInfo))
          continue;

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
                  << '\n';
        continue;
      }

      if (! customDimensions.matches(p, parameterInfo))
        continue;

      if (!cleaned_param_names.empty())
        cleaned_param_names += ",";

      if (grid_producer)
        handleGridParameter(p, producerId, geometryId, levelId, z);

      cleaned_param_names += p;
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
                                                    bool grid_producer,
                                                    bool& noReqParams,
                                                    const CustomDimensions &customDimensions)
{
  try
  {
    auto z = Spine::optional_string(req.getParameter("z"), "");
    double min_level = std::numeric_limits<double>::min();
    double max_level = std::numeric_limits<double>::max();
    bool hasZParam = !z.empty();
    bool range = false;

    std::string zLo;
    std::string zHi;
    if (UtilityFunctions::parseRangeListValue(z, range, zLo, zHi))
    {
      if (range)
      {
        min_level = Fmi::stod(zLo);
        max_level = Fmi::stod(zHi);
        z.clear();
      }
    }
    else
      z = itsCoordinateFilter.getLevels();

    // If no level given get all levels
    using Engine::Observation::ObsLevelType;
    const auto* zParameter = "levels";

    if (z.empty() && !emd.vertical_extent.levels.empty())
    {
      // Sounding collection metadata contains level range only, some other vertical observation
      // collection might list all levels (e.g. mast ?)
      //
      // Pressures (and altitudes too if used) must currently be handled as integer values
      // (level values are floored for sounding) since DataFilter class (Settings) only handles
      // integer filters currently. Therefore also using request parameter "levels" for
      // sounding pressures (and altitudes if used) instead of "pressures" (or "heights") since
      // it's values are parsed/stored as integers.
      //
      // If levels were not given, accept vertical_extent levels without min/max check
      // (checking level 0 against numeric_limits<double>::min() would reject it)

      range = emd.vertical_extent.is_level_range;

      for (const auto& value : emd.vertical_extent.levels)
      {
        std::string level(value);

        if (hasZParam)
        {
          double d_level = Fmi::stod(value);
          level = std::to_string((int)floor(d_level));

          if (!range)
          {
            if (d_level < min_level || d_level > max_level)
              continue;
          }
          else if (z.empty())
          {
            if (d_level < min_level)
              level = Fmi::to_string(min_level);
          }
          else if (d_level > max_level)
            level = Fmi::to_string(max_level);
        }

        if (!z.empty())
          z.append(",");
        z.append(level);
      }
    }

    if (!z.empty())
      req.addParameter(zParameter, z);
    req.addParameter("levelrange", range ? "1" : "");

    // EDR parameter-name
    auto parameter_names = Spine::optional_string(req.getParameter("parameter-name"), "");
    noReqParams = parameter_names.empty();

#ifndef WITHOUT_AVI
    if (parameter_names.empty() && isAviProducer(state.getAviMetaData(), itsEDRQuery.collection_id))
    {
      req.addParameter("parameter-name", "message");
      return "message";
    }
#endif
    auto const &parameterInfo = state.getPlugin().getConfigParameterInfo();
    return cleanParameterNames(
        parameter_names, emd, grid_producer, z, parameterInfo, customDimensions);
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
      const std::string& icaoreq = *icao_ids;
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

void EDRQueryParams::validateRequestParameterNamesWithMetaData(const EDRMetaData& emd) const {}
void EDRQueryParams::validateRequestDateTimeWithMetaData(const EDRMetaData& emd) const {}
void EDRQueryParams::validateRequestLevelsWithMetaData(const EDRMetaData& emd) const {}
void EDRQueryParams::validateRequestCoordinatesWithMetaData(const EDRMetaData& emd) const {}

void EDRQueryParams::validateRequestParametersWithMetaData(const EDRMetaData& emd) const
{
  validateRequestParameterNamesWithMetaData(emd);
  validateRequestDateTimeWithMetaData(emd);
  validateRequestLevelsWithMetaData(emd);
  validateRequestCoordinatesWithMetaData(emd);
}
}  // namespace EDR
}  // namespace Plugin
}  // namespace SmartMet
