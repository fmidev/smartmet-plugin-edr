#include "EDRQueryParams.h"
#include "EDRException.h"
#include "VerticalReference.h"
#include "EDRDefs.h"
#include "EDRMetaData.h"
#include "LonLatDistance.h"
#include "Plugin.h"
#include "UtilityFunctions.h"
#include <engines/observation/Keywords.h>
#include <macgyver/AnsiEscapeCodes.h>
#include <macgyver/StringConversion.h>
#include <macgyver/TimeParser.h>
#include <spine/Convenience.h>
#include <spine/FmiApiKey.h>
#include <fmt/format.h>
#include <ogr_api.h>
#include <ogr_geometry.h>
#include <algorithm>
#include <cmath>
#include <limits>
#include <memory>
#include <string>

namespace SmartMet
{
namespace Plugin
{
namespace EDR
{

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
    //   corridor   coords=[MULTI]LINESTRING[ZM|Z|M]&corridor-width=width&width-units={km|mi}
    //   radius     coords=wkt|POINT|MULTIPOINT|MULTIPOINTZ&within=radius&within-units={km|mi}
    //   trajectory coords=[MULTI]LINESTRING[ZM|Z|M]
    //   cube       coords=wkt|POLYGON&minz=lo&maxz=hi|bbox=bllon,bllat,trlon,trlat&z=lo/hi
    //
    //   locations/LOCID
    //   locations?locationId=LOCID  (alternative to the path format above)

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
      throw EDRNotFound("URI has no 'collections' part");

    if ((numParts > 0) && (resParts[numParts - 1].empty()))
      numParts--;

    auto lastIdx = numParts - 1;

    if ((lastIdx - resIdx) < 2)
      return false;

    resIdx += ((resParts[resIdx + 2] == "instances") ? 4 : 2);

    if (lastIdx < resIdx)
      return false;

    if ((lastIdx == resIdx) && (resParts[resIdx] == "locations"))
    {
      // claude: BRAINSTORM-3499:
      //
      // 'locations' with no LOCID path part is a listing (metadata) query, unless a
      // locationId query parameter is given instead - then it's still a data query
      //
      return Spine::optional_string(req.getParameter("locationId"), "").empty() == false;
    }

    return true;
  }
  catch (...)
  {
    throw Fmi::Exception::Trace(BCP, "Resolving host failed!");
  }
}

// A client-supplied Host header is reflected into the self-referential URLs in EDR
// responses, so accept only a plain hostname[:port]. This blocks CRLF/control-character
// injection (response splitting) and arbitrary markup from a spoofed header.
static bool is_valid_host(const std::string& host)
{
  if (host.empty() || host.size() > 253)
    return false;
  for (char c : host)
  {
    const bool ok = (c >= 'A' && c <= 'Z') || (c >= 'a' && c <= 'z') ||
                    (c >= '0' && c <= '9') || c == '.' || c == '-' || c == ':';
    if (!ok)
      return false;
  }
  return true;
}

std::string resolve_host(const Spine::HTTP::Request& theRequest, const std::string& base_url)
{
  try
  {
    auto host_header = theRequest.getHeader("Host");
    if (!host_header || !is_valid_host(*host_header))
    {
      // No Host header (mandatory in HTTP/1.1), or a malformed/spoofed one: fall back
      // to the canonical URL rather than reflecting attacker-controlled input.
      return "http://smartmet.fmi.fi/edr";
    }

    // http/https scheme selection based on 'X-Forwarded-Proto' header. That header is
    // client-controlled, so accept only the two valid scheme values.
    auto host_protocol = theRequest.getProtocol();
    std::string scheme = (host_protocol ? *host_protocol : "http");
    if (scheme != "http" && scheme != "https")
      scheme = "http";
    std::string protocol(scheme + "://");

    std::string host = *host_header;

    // Apikey
    std::optional<std::string> apikey;
    try
    {
      apikey = Spine::FmiApiKey::getFmiApiKey(theRequest);
    }
    catch (...)
    {
      throw Fmi::Exception::Trace(BCP, "Failed to get apikey from the query");
    }

    if (apikey && Spine::FmiApiKey::shouldReturnApiKey(theRequest))
    {
      host.append(("/fmi-apikey/" + *apikey));
    }

    return (protocol + host + base_url);
  }
  catch (...)
  {
    throw Fmi::Exception::Trace(BCP, "Resolving host failed!");
  }
}

// Distance in kilometres for the within-units and width-units values m, km and mi
double distance_in_km(double value, const std::string& units, const char* name)
{
  if (units == "m")
    return value / 1000;
  if (units == "km")
    return value;
  if (units == "mi")
    return value * 1.60934;
  throw EDRException("Invalid " + std::string(name) + " option '" + units +
                     "' used, 'm', 'km' and 'mi' are supported");
}

// Output format selected by the Accept header (RFC 9110 content negotiation). The default
// format is used if there is no Accept header or it accepts anything.
std::string negotiate_output_format(const std::optional<std::string>& accept,
                                    const std::set<std::string>& supported_formats,
                                    const std::string& default_format)
{
  if (!accept || Fmi::trim_copy(*accept).empty())
    return default_format;

  // Media type -> output format
  static const std::vector<std::pair<std::string, std::string>> media_types{
      {COVERAGE_JSON_MIME_TYPE, COVERAGE_JSON_FORMAT},
      {"application/prs.coverage+json", COVERAGE_JSON_FORMAT},
      {"application/geo+json", GEO_JSON_FORMAT},
      {"application/json", ""},  // the default format if it is JSON
      {"text/xml", IWXXM_FORMAT},
      {"application/xml", IWXXM_FORMAT},
      {"application/zip", IWXXMZIP_FORMAT},
      {"text/plain", TAC_FORMAT}};

  std::string best_format;
  double best_q = 0;

  std::vector<std::string> ranges;
  boost::algorithm::split(ranges, *accept, boost::algorithm::is_any_of(","));
  for (const auto& range : ranges)
  {
    std::vector<std::string> parts;
    boost::algorithm::split(parts, range, boost::algorithm::is_any_of(";"));
    const auto type = boost::algorithm::to_lower_copy(Fmi::trim_copy(parts[0]));
    double q = 1;
    for (std::size_t i = 1; i < parts.size(); i++)
    {
      auto param = Fmi::trim_copy(parts[i]);
      if (boost::algorithm::istarts_with(param, "q="))
      {
        try
        {
          q = std::stod(param.substr(2));
        }
        catch (...)
        {
          q = 0;
        }
      }
    }
    if (q <= best_q)
      continue;

    std::string format;
    if (type == "*/*" || type == "application/*")
      format = default_format;
    else
    {
      for (const auto& item : media_types)
        if (item.first == type)
        {
          format = item.second;
          if (format.empty() && (default_format == COVERAGE_JSON_FORMAT ||
                                 default_format == GEO_JSON_FORMAT))
            format = default_format;
          break;
        }
    }

    if (!format.empty() && supported_formats.find(format) != supported_formats.end())
    {
      best_format = format;
      best_q = q;
    }
  }

  if (best_format.empty())
    throw EDRNotAcceptable("None of the media types in the Accept header '" + *accept +
                           "' is supported, use the f option to select one of the output "
                           "formats of the collection");
  return best_format;
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
    itsEDRQuery.crs_registry = &config.getCRSRegistry();

    // The landing page and the conformance declaration are generated documents. The API
    // definition (<root>/api) is answered before the query is parsed.
    {
      std::string path = resource;
      while (path.size() > 1 && path.back() == '/')
        path.pop_back();
      const auto& root = config.defaultUrl();
      if (path == root || path == root + "/conformance")
      {
        itsEDRQuery.query_id = EDRQueryId::APIQuery;
        itsEDRQuery.instance_id = (path == root ? "landing" : "conformance");
        return;
      }
    }

    std::vector<std::string> resource_parts;
    boost::algorithm::split(resource_parts, resource, boost::algorithm::is_any_of("/"));
    if (resource_parts.size() > 1 && resource_parts.front().empty())
      resource_parts.erase(resource_parts.begin());

    auto invalid_query_type = parseEDRQuery(state, config, resource);

    EDRMetaData emd = state.getProducerMetaData(itsEDRQuery.collection_id);

    if (itsEDRQuery.query_id == EDRQueryId::SpecifiedCollectionItems)
    {
      parseItemsQuery(emd, config);
      return;
    }

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
      throw EDRNotFound("Invalid query type '" + requested_query_type +
                         "'. The following queries are supported for collection " +
                         itsEDRQuery.collection_id + ": " + data_query_list);
    }

    // If meta data query return from here
    if (itsEDRQuery.query_id != EDRQueryId::DataQuery)
    {
      if (itsEDRQuery.query_type == EDRQueryType::Locations)
        parseLimitAndOffset();

      // BRAINSTORM-3267; if mime_type is left empty, plugin sets the type usíng output format
      //
      if (itsEDRQuery.query_type == EDRQueryType::Locations)
        mime_type = "application/geo+json";
      else
        mime_type = "application/json";

      return;
    }

    // From here on EDR data query
    // Querydata and obervation producers do not have dots (.) in id, but grid
    // producers always do have
    bool grid_producer = (itsEDRQuery.collection_id.find('.') != std::string::npos);
    if (!grid_producer)
      req.addParameter("producer", itsEDRQuery.collection_id);

    // OGC API-EDR: crs must be one of the CRSs listed in the collection metadata
    auto crs = Spine::optional_string(req.getParameter("crs"), "OGC:CRS84");
    if (!is_crs84(crs))
    {
      itsEDRQuery.crs = config.getCRSRegistry().find(crs);
      if (itsEDRQuery.crs)
      {
        // MetOcean profile: the output referencing system id is the requested crs value
        auto info = std::make_shared<CRSInfo>(*itsEDRQuery.crs);
        info->request_name = crs;
        itsEDRQuery.crs = info;
      }
      if (!itsEDRQuery.crs)
      {
        std::string supported = "OGC:CRS84";
        for (const auto& info : config.getCRSRegistry().all())
          supported += ", " + info->id;
        throw EDRException("Invalid crs: " + crs + ". Supported values are " + supported);
      }
      // Output coordinates of projected CRSs are compared with the requested ones in metres
      if (!itsEDRQuery.crs->geographic)
        itsCoordinateFilter.setTolerance(1.0);
    }

    // If query_type is position, radius, trajectory -> coords is required
    auto coords = Spine::optional_string(req.getParameter("coords"), "");
    if (!coords.empty())
    {
      parseCoords(emd, coords);
    }
    else if (itsEDRQuery.query_type == EDRQueryType::Locations)
    {
      parseLocations(emd, coords);
    }
    else if (itsEDRQuery.query_type == EDRQueryType::Cube)
    {
      parseCube(emd);
    }
    else if ((itsEDRQuery.query_type == EDRQueryType::Position) ||
             (itsEDRQuery.query_type == EDRQueryType::Radius) ||
             (itsEDRQuery.query_type == EDRQueryType::Area) ||
             (itsEDRQuery.query_type == EDRQueryType::Trajectory) ||
             (itsEDRQuery.query_type == EDRQueryType::Corridor))
      throw EDRException("Missing coords option!");

    // resolution-z for cube and corridor queries
    parseResolutionZ(emd, config);

    // resolution-x and resolution-y for area, cube and corridor queries
    parseResolution(emd, config);

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

    const auto& supportedOutputFormats =
        config.getSupportedOutputFormats(itsEDRQuery.collection_id);

    // The f option overrides content negotiation, the format names are case insensitive
    auto f_option = req.getParameter("f");
    if (f_option)
    {
      output_format = *f_option;
      for (const auto& format : supportedOutputFormats)
        if (boost::algorithm::iequals(format, output_format))
          output_format = format;
    }
    else
      output_format = negotiate_output_format(
          req.getHeader("Accept"), supportedOutputFormats, emd.default_output_format);

    if (supportedOutputFormats.find(output_format) == supportedOutputFormats.end())
    {
      auto output_format_list = boost::algorithm::join(supportedOutputFormats, ",");
      throw EDRException("Invalid output format '" + output_format +
                         "'. The following output formats are supported for collection " +
                         itsEDRQuery.collection_id + ": " + output_format_list);
    }

    // RFC 7946: GeoJSON coordinates are always WGS84 longitude/latitude. A geographic CRS
    // such as EPSG:4326 can be used for the input coordinates, the output stays CRS84.
    if (itsEDRQuery.crs && output_format == GEO_JSON_FORMAT && !itsEDRQuery.crs->geographic)
      throw EDRException("GeoJSON output is available in geographic coordinates only, crs '" +
                         itsEDRQuery.crs->id + "' can be used with CoverageJSON");

    // Request URLs in each output format for the links of the response
    {
      const auto& base_url = config.defaultUrl();
      const auto pos = resource.find(base_url);
      const auto path =
          (pos == std::string::npos ? resource : resource.substr(pos + base_url.size()));
      for (const auto& format : supportedOutputFormats)
      {
        auto format_request = request;
        format_request.setParameter("f", format);
        // Credentials are never echoed as parameters, resolve_host adds the apikey to the
        // host part of the URL when the client allows it
        format_request.removeParameter("fmi-apikey");
        format_request.removeParameter("access-token");
        itsEDRQuery.format_urls[format] = itsEDRQuery.host + path + format_request.getQueryString();
      }
    }

    if (output_format == COVERAGE_JSON_FORMAT)
      mime_type = COVERAGE_JSON_MIME_TYPE;
    else if (output_format == GEO_JSON_FORMAT)
      mime_type = "application/geo+json";

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

      if (resource_parts.size() > 6 && !resource_parts.at(6).empty())
        itsEDRQuery.location_id = resource_parts.at(6);
      else
        // claude: BRAINSTORM-3499:
        //
        // Alternative to the 'locations/{locationId}' path format: locations?locationId=id
        //
        itsEDRQuery.location_id = Spine::optional_string(req.getParameter("locationId"), "");
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
    // MetOcean profile: instance ids are RFC 3339 times (2008-08-05T06:31:00Z). Other forms
    // of the same time such as the earlier 20080805T063100 are accepted as aliases.
    itsEDRQuery.instance_id = resource_parts.at(4);
    try
    {
      itsEDRQuery.instance_id =
          Fmi::date_time::to_iso_extended_string(Fmi::TimeParser::parse(itsEDRQuery.instance_id)) +
          "Z";
    }
    catch (...)
    {
    }
    itsEDRQuery.query_id = EDRQueryId::SpecifiedCollectionSpecifiedInstance;

    if (!state.getPlugin().isValidInstance(itsEDRQuery.collection_id, itsEDRQuery.instance_id))
      throw EDRNotFound("Instance id '" + itsEDRQuery.instance_id + "' is not valid");

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
        throw EDRNotFound("Collection '" + resource_parts.at(2) + "' does not have instances!");

      itsEDRQuery.query_id = EDRQueryId::SpecifiedCollectionAllInstances;
      if (resource_parts.size() > 4 && !resource_parts.at(4).empty())
        return parseInstances(state, resource_parts);
    }

    itsEDRQuery.query_type = to_query_type_id(resource_parts.at(3));

    if (itsEDRQuery.query_type == EDRQueryType::InvalidQueryType)
      return resource_parts.at(3);

    if (itsEDRQuery.query_type == EDRQueryType::Items)
    {
      itsEDRQuery.query_id = EDRQueryId::SpecifiedCollectionItems;
      if (resource_parts.size() > 4 && !resource_parts.at(4).empty())
        itsEDRQuery.item_id = resource_parts.at(4);
      return {};
    }

    if (itsEDRQuery.query_type == EDRQueryType::Locations)
    {
      itsEDRQuery.query_id = EDRQueryId::SpecifiedCollectionLocations;

      if (resource_parts.size() > 4 && !resource_parts.at(4).empty())
        itsEDRQuery.location_id = resource_parts.at(4);
      else
        // claude: BRAINSTORM-3499:
        //
        // Alternative to the 'locations/{locationId}' path format: locations?locationId=id
        //
        itsEDRQuery.location_id = Spine::optional_string(req.getParameter("locationId"), "");
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
      throw EDRNotFound(("Collection '" + itsEDRQuery.collection_id + "' not found"));

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
      throw EDRNotFound("URI does not match registration url");

    std::vector<std::string> resource_parts;
    std::string edrResource = "edr" + resource.substr(pos + baseUrl.size());
    boost::algorithm::split(resource_parts, edrResource, boost::algorithm::is_any_of("/"));
    if (resource_parts.size() > 1 && resource_parts.back().empty())
      resource_parts.pop_back();

    if (resource_parts.size() < 2)
      return {};

    // First keyword must be collections
    if (resource_parts.at(1) != "collections")
      throw EDRNotFound("Invalid path '" + resource + "'");

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
        if (coordinate_item.size() >= 2 && coordinate_item.front() == '(' &&
            coordinate_item.back() == ')')
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
    // "LINESTRING Z" is equivalent to "LINESTRINGZ"
    boost::algorithm::erase_all(geometry_name, " ");

    // OGC API EDR: the coords are a LINESTRING or, if supported, a MULTILINESTRING, with
    // optional Z (height) and M (epoch time) values. Other geometries get a 400 error.
    const bool multi = boost::algorithm::starts_with(geometry_name, "MULTILINESTRING");
    const std::string dims =
        geometry_name.substr(std::min(geometry_name.size(), std::size_t(multi ? 15 : 10)));
    if ((geometry_name != (multi ? "MULTILINESTRING" : "LINESTRING") + dims) ||
        (dims != "" && dims != "Z" && dims != "M" && dims != "ZM"))
      throw EDRException(
          "Trajectory and corridor coords must be a LINESTRING or MULTILINESTRING, optionally "
          "with Z, M or ZM values: " +
          coords);

    if (wkt.find('(') == std::string::npos || wkt.rfind(')') == std::string::npos)
      throw EDRException("Invalid " + geometry_name + " definition: " + coords);

    // OGC API-EDR: the server SHALL report an error if the coords already define the
    // levels (Z) or the times (M) and the z or datetime query parameter is given too
    const bool has_z = !Spine::optional_string(req.getParameter("z"), "").empty();
    const bool has_datetime = !Spine::optional_string(req.getParameter("datetime"), "").empty();
    if (has_z && (dims == "Z" || dims == "ZM"))
      throw EDRException(
          "Query parameter 'z' cannot be used when the coords define the levels (" +
          geometry_name + ")");
    if (has_datetime && (dims == "M" || dims == "ZM"))
      throw EDRException(
          "Query parameter 'datetime' cannot be used when the coords define the times (" +
          geometry_name + ")");

    auto len = (wkt.rfind(')') - wkt.find('(') - 1);
    auto geometry_values = wkt.substr(wkt.find('(') + 1, len);
    auto radius = wkt.substr(wkt.rfind(')') + 1);

    // Converts the coordinates of one line into 2D coordinates, storing the levels and times
    // given as Z and M values into the coordinate filter
    auto parse_line = [&](const std::string& line) -> std::string
    {
      if (dims.empty())
        return line;

      const std::size_t expected = (dims == "ZM" ? 4 : 3);
      std::string ret;
      std::vector<std::string> coordinates;
      boost::algorithm::split(coordinates, line, boost::algorithm::is_any_of(","));
      for (auto coordinate : coordinates)
      {
        boost::algorithm::trim(coordinate);
        std::vector<std::string> parts;
        boost::algorithm::split(parts,
                                coordinate,
                                boost::algorithm::is_any_of(" "),
                                boost::algorithm::token_compress_on);
        if (parts.size() != expected)
        {
          if (dims == "ZM")
            throw EDRException("Invalid " + geometry_name +
                               " definition, longitude, latitude, level, epoch time expected: " +
                               coords);
          if (dims == "Z")
            throw EDRException("Invalid " + geometry_name +
                               " definition, longitude, latitude, level expected: " + coords);
          throw EDRException("Invalid " + geometry_name +
                             " definition, longitude, latitude, epoch time expected: " + coords);
        }

        if (!ret.empty())
          ret += ',';
        ret += parts[0] + " " + parts[1];

        const double lon = Fmi::stod(parts[0]);
        const double lat = Fmi::stod(parts[1]);
        if (dims == "ZM")
          itsCoordinateFilter.add(
              lon, lat, Fmi::stod(parts[2]), Fmi::date_time::from_time_t(Fmi::stod(parts[3])));
        else if (dims == "Z")
          itsCoordinateFilter.add(lon, lat, Fmi::stod(parts[2]));
        else
          itsCoordinateFilter.add(lon, lat, Fmi::date_time::from_time_t(Fmi::stod(parts[2])));
      }
      return ret;
    };

    if (!multi)
      wkt = "LINESTRING(" + parse_line(geometry_values) + ")";
    else
    {
      // The values are of the form (x y,...),(x y,...)
      wkt = "MULTILINESTRING(";
      std::size_t pos = 0;
      bool first = true;
      while (true)
      {
        pos = geometry_values.find_first_not_of(" ", pos);
        if (pos == std::string::npos)
          break;
        if (!first)
        {
          if (geometry_values[pos] != ',')
            throw EDRException("Invalid " + geometry_name + " definition: " + coords);
          pos = geometry_values.find_first_not_of(" ", pos + 1);
          if (pos == std::string::npos)
            throw EDRException("Invalid " + geometry_name + " definition: " + coords);
        }
        const auto close = geometry_values.find(')', pos);
        if (geometry_values[pos] != '(' || close == std::string::npos ||
            geometry_values.find('(', pos + 1) < close)
          throw EDRException("Invalid " + geometry_name + " definition: " + coords);

        if (!first)
          wkt += ',';
        wkt += "(" + parse_line(geometry_values.substr(pos + 1, close - pos - 1)) + ")";
        first = false;
        pos = close + 1;
      }
      if (first)
        throw EDRException("Invalid " + geometry_name + " definition: " + coords);
      wkt += ")";
    }

    if (!radius.empty())
      wkt.append(radius);

    return wkt;
  }
  catch (...)
  {
    throw Fmi::Exception::Trace(BCP, "Operation failed!");
  }
}

void EDRQueryParams::parseCoords(const EDRMetaData& emd, const std::string& coordinates)
{
  try
  {
    // Coordinates in a CRS whose axis order is latitude/longitude or northing/easting are
    // handled in the x/y order
    auto coords = (itsEDRQuery.crs ? itsEDRQuery.crs->toXY(coordinates) : coordinates);
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
      const auto* units_name =
          (itsEDRQuery.query_type == EDRQueryType::Corridor ? "width-units" : "within-units");
      auto radius = distance_in_km(Fmi::stod(within), within_units, units_name);
      coords += (":" + Fmi::to_string(radius));
    }

    if (itsEDRQuery.query_type == EDRQueryType::Position && !within.empty())
    {
      // Extension to OGC API EDR: within and within-units limit the distance of the
      // station (or point data location) used for the position (BRAINSTORM-2883)
      if (within_units.empty())
        throw EDRException("Query parameter 'within-units' must be defined with 'within'");

      auto distance = Fmi::stod(within);
      if (!(distance > 0))
        throw EDRException("Query parameter 'within' must be positive");
      distance = distance_in_km(distance, within_units, "within-units");
      req.setParameter("maxdistance", Fmi::to_string(distance) + "km");
    }

    auto wkt = coords;
    if (itsEDRQuery.query_type == EDRQueryType::Area)
    {
      // OGC API EDR: the area is a POLYGON; a MULTIPOLYGON is accepted too (e.g. islands)
      auto geometry_type = boost::algorithm::to_upper_copy(boost::algorithm::trim_copy(coords));
      if (!boost::algorithm::starts_with(geometry_type, "POLYGON") &&
          !boost::algorithm::starts_with(geometry_type, "MULTIPOLYGON"))
        throw EDRException("Area query coords must be a POLYGON or MULTIPOLYGON");
    }
    else if (itsEDRQuery.query_type == EDRQueryType::Position)
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

    if (itsEDRQuery.query_type == EDRQueryType::Corridor)
    {
      // EDR corridor-height + height-units
      auto corridor_height = Spine::optional_string(req.getParameter("corridor-height"), "");
      auto height_units = Spine::optional_string(req.getParameter("height-units"), "");

      if (!corridor_height.empty() || !height_units.empty())
      {
        if (corridor_height.empty() || height_units.empty())
          throw EDRException(
              "Query parameter 'corridor-height' and 'height-units' must both be defined for "
              "Corridor query");

        // AVI collections (METAR/TAC/IWXXM reports) have no vertical extent at all - the
        // engine never filters by level/z - so silently accepting corridor-height here would
        // just discard it without effect. Reject explicitly instead of pretending it did
        // something.
        if (emd.isAviProducer())
          throw EDRException(
              "Query parameter 'corridor-height' is not supported for collection '" +
              itsEDRQuery.collection_id + "': it has no vertical extent");

        auto height = Fmi::stod(corridor_height);

        // Normalize corridor-height into the unit the collection's own vertical levels use.
        // Pressure and hybrid/model levels are known, documented special cases:
        // - Pressure: querydata/observation producers report levels in hPa (newbase
        //   "PressureLevel" convention) while grid-engine producers (collection ids containing
        //   '.') report them in Pa (GRIB "PRESSURE" convention) - see EDRMetaData.cpp's
        //   get_edr_metadata_qd/get_edr_metadata_grid. Convert automatically so hPa/Pa/mbar can
        //   be used interchangeably regardless of which engine backs the collection.
        // - Hybrid/model levels ("HybridLevel"/"HYBRID") are dimensionless model level indices,
        //   not a physical unit - EDR represents this with unit "1", and no conversion applies.
        // Other level types (height, depth, ...) have no known unit convention difference across
        // engines, so metric units are only range-checked, not converted, matching prior
        // behaviour.
        bool grid_producer = (itsEDRQuery.collection_id.find('.') != std::string::npos);
        const auto& level_type = emd.vertical_extent.level_type;
        bool is_pressure_level = boost::algorithm::icontains(level_type, "pressure");
        bool is_hybrid_level = boost::algorithm::icontains(level_type, "hybrid");

        if (height_units == "hPa" || height_units == "mbar")
        {
          if (!is_pressure_level)
            throw EDRException("Query parameter 'height-units' value '" + height_units +
                               "' is not valid for collection '" + itsEDRQuery.collection_id +
                               "': its vertical extent is not a pressure level");
          if (grid_producer)
            height *= 100.0;  // hPa/mbar -> Pa
        }
        else if (height_units == "Pa")
        {
          if (!is_pressure_level)
            throw EDRException("Query parameter 'height-units' value '" + height_units +
                               "' is not valid for collection '" + itsEDRQuery.collection_id +
                               "': its vertical extent is not a pressure level");
          if (!grid_producer)
            height /= 100.0;  // Pa -> hPa
        }
        else if (height_units == "1")
        {
          if (!is_hybrid_level)
            throw EDRException("Query parameter 'height-units' value '1' is not valid for "
                               "collection '" + itsEDRQuery.collection_id +
                               "': its vertical extent is not a hybrid/model level");
          // Dimensionless model level index - used as-is, no conversion.
        }
        else if (height_units == "m" || height_units == "km" || height_units == "mi" ||
                 height_units == "ft")
        {
          if (is_pressure_level)
            throw EDRException("Query parameter 'height-units' value '" + height_units +
                               "' is not valid for collection '" + itsEDRQuery.collection_id +
                               "': its vertical extent is a pressure level, use 'hPa', 'mbar' "
                               "or 'Pa' instead");
          if (is_hybrid_level)
            throw EDRException("Query parameter 'height-units' value '" + height_units +
                               "' is not valid for collection '" + itsEDRQuery.collection_id +
                               "': its vertical extent is a hybrid/model level, use '1' instead");
          if (height_units == "km")
            height *= 1000.0;
          else if (height_units == "mi")
            height *= 1609.34;
          else if (height_units == "ft")
            height *= 0.3048;
        }
        else
          throw EDRException(
              "Invalid height-units option '" + height_units +
              "' used, supported units: 'hPa', 'mbar', 'Pa' for pressure levels, '1' for "
              "hybrid/model levels, 'm', 'km', 'mi', 'ft' for height/altitude/depth levels");

        // The corridor's centre level comes from the path's own Z coordinates
        // ([MULTI]LINESTRINGZ/ZM); if the path is 2D an explicit 'z' level must be given
        // instead to act as the centre of the height band
        auto levels = itsCoordinateFilter.getLevels();
        std::string centre_lo;
        std::string centre_hi;
        if (!levels.empty())
        {
          std::vector<std::string> level_list;
          boost::algorithm::split(level_list, levels, boost::algorithm::is_any_of(","));
          centre_lo = level_list.front();
          centre_hi = level_list.back();
        }
        else
        {
          auto z = Spine::optional_string(req.getParameter("z"), "");
          if (z.empty())
            throw EDRException(
                "Query parameter 'corridor-height' requires 3D coords "
                "([MULTI]LINESTRINGZ/ZM) or an explicit 'z' query parameter to define the "
                "centre level of the corridor");

          // 'z' may be a single value, a list, a range (lo/hi) or a repeating interval
          // (Rn/start/step); take the min/max of whatever it expands to as the centre band's
          // extremes, same as the 3D-path branch above does with the path's own levels.
          bool zIsRange = false;
          if (!UtilityFunctions::parseRangeListValue(z, zIsRange, centre_lo, centre_hi))
            throw EDRException("Invalid 'z' query parameter '" + z + "'");
          if (centre_hi.empty())
            centre_hi = centre_lo;
        }

        auto zLo = Fmi::stod(centre_lo) - height / 2;
        auto zHi = Fmi::stod(centre_hi) + height / 2;
        req.removeParameter("z");
        req.addParameter("z", Fmi::to_string(zLo) + "/" + Fmi::to_string(zHi));
      }
    }


    /*
          // Maybe later: support other CRSs than WGS84
    if(!crs.empty() && crs != "EPSG:4326")
          {
            // Have to convert coordinates into WGS84
            auto transformed_wkt = transform_coordinates(wkt, crs);
            wkt = transformed_wkt;
          }
    */
    // Coordinates in another CRS are transformed to WGS84, the coordinate filter keeps the
    // requested coordinates since the output coordinates are transformed back
    if (itsEDRQuery.crs)
    {
      itsRequestXYWkt = wkt;
      wkt = itsEDRQuery.crs->toWGS84(wkt);
    }

    // The segments of a MULTILINESTRING trajectory are queried as separate locations so that
    // each segment becomes a trajectory of its own in the output. AVI queries read the
    // geometry as a single parameter, and their results are per station anyway.
    if (itsEDRQuery.query_type == EDRQueryType::Trajectory && !emd.isAviProducer() &&
        boost::algorithm::starts_with(wkt, "MULTILINESTRING("))
    {
      // parseTrajectoryAndCorridor has normalized the value to MULTILINESTRING((...),(...))
      std::size_t pos = wkt.find('(');  // the outer parenthesis
      while ((pos = wkt.find('(', pos + 1)) != std::string::npos)
      {
        const auto close = wkt.find(')', pos);
        req.addParameter("wkt", "LINESTRING" + wkt.substr(pos, close - pos + 1));
        pos = close;
      }
    }
    else
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
      throw EDRNotFound("Location query is not supported by collection '" +
                         itsEDRQuery.collection_id + "'");

    auto location_id = itsEDRQuery.location_id;

    if (location_id == "all")
    {
      // BRAINSTORM-3288
      //
      // claude: BRAINSTORM-3499:
      //
      // 'locations/all' means "every location this collection's own /locations listing
      // reports" - NOT literally every native grid cell (for a grid-engine-backed collection
      // that can be hundreds of thousands of points, and building/serializing a response that
      // size is impractical - confirmed the hard way). Every location_info (regardless of
      // whether its type is fmisid/ICAO/geoid/qdstation) already carries real lon/lat
      // (LocationInfo.h), so build one combined MULTIPOINT from all of them and reuse the
      // existing multi-point Position query path as-is (same as the qdstation branch below,
      // just for every location at once) - the per-type fmisid/geoid/icao request parameters
      // were tried first and don't work here: each resolves into its own separate
      // Spine::TaggedLocation, and CoverageJson's output formatter only keeps the first when
      // locations arrive as many separate single-point results instead of one combined
      // multi-point query (confirmed against the already-correct 'pal_skandinavia_multi_point'
      // test, which goes through this exact combined-query path).
      //
      if (emd.locations->empty())
        throw EDRException("No locations found for collection '" + itsEDRQuery.collection_id +
                           "'");

      if (emd.isAviProducer())
      {
        // claude: BRAINSTORM-3499:
        //
        // AVI engine's Position handler (checkAviEnginePositionQuery) requires a literal
        // POINT geometry and rejects MULTIPOINT outright - unlike grid/querydata, it doesn't
        // need the MULTIPOINT/Position trick anyway: its Locations query already natively
        // accepts many comma-separated icaos in one request (parseICAOCodesAndAviProducer ->
        // query.icaos -> checkAviEngineLocationQuery's loop), the exact same mechanism the
        // single-icao branch below already relies on. So just list every icao and keep
        // query_type as Locations.
        //
        std::string icaoList;
        for (const auto& item : *emd.locations)
          icaoList += item.first + ",";
        icaoList.pop_back();
        req.addParameter("icao", icaoList);
        return;
      }

      itsEDRQuery.query_type = EDRQueryType::Position;

      std::string points;
      double lon_min = std::numeric_limits<double>::max();
      double lon_max = std::numeric_limits<double>::lowest();
      double lat_min = std::numeric_limits<double>::max();
      double lat_max = std::numeric_limits<double>::lowest();
      bool has_qdstation = false;
      for (const auto& item : *emd.locations)
      {
        const auto& info = item.second;
        points += Fmi::to_string(info.longitude) + " " + Fmi::to_string(info.latitude) + ",";
        lon_min = std::min(lon_min, info.longitude);
        lon_max = std::max(lon_max, info.longitude);
        lat_min = std::min(lat_min, info.latitude);
        lat_max = std::max(lat_max, info.latitude);
        has_qdstation = (has_qdstation || info.type == "qdstation");
      }
      points.pop_back();
      coords = "MULTIPOINT(" + points + ")";

      // claude: BRAINSTORM-3499:
      //
      // Point ('qdstation') querydata is only ever queried at its own exact station
      // coordinates, never interpolated, so the producer for a combined MULTIPOINT query is
      // chosen by checking whether a single representative point - the envelope centre of the
      // whole MULTIPOINT geometry, WktGeometry::locationFromGeometry - falls within
      // 'maxdistance' of an actual station (Repository::contains ->
      // NFmiFastQueryInfo::IsInside). For a collection whose stations ring a coastline (e.g.
      // sealevel) that centre falls inland, far from every station, so producer selection fails
      // with "No data available for 'MULTIPOINT(...)'" before any individual point is even
      // looked at - even though every point in the MULTIPOINT is itself an exact, valid station
      // coordinate. Grid/gridded-querydata collections never hit this because any point inside
      // their bbox is valid data (see df393cf, "Allow MULTIPOINT queries against point
      // querydata producers", which fixed the equivalent single-collection case and explicitly
      // left this centre-point limitation for later). Give the centre enough slack to reach the
      // farthest of this collection's own stations so producer selection succeeds regardless of
      // where the centre falls; each individual point still resolves to its own exact station
      // (distance 0) once a producer is chosen, so this cannot pick the wrong station.
      //
      // 'maxdistance' is not part of the EDR spec, but CommonQuery::commonInit reads it
      // unconditionally on any EDR request, so a caller could pass one here too. There is no
      // legitimate use for that in an "all" query though - it can only accidentally reintroduce
      // this exact bug (e.g. a caller-supplied maxdistance smaller than what the centre needs) -
      // so force the computed value rather than merely defaulting it, using setParameter (erase
      // + insert) rather than addParameter: Spine::HTTP::Request parameters are a multimap and
      // addParameter would leave both values in place, and commonInit's own
      // req.getParameter("maxdistance") throws outright if a parameter has more than one value.
      //
      if (has_qdstation)
      {
        std::pair<double, double> centre((lon_min + lon_max) / 2, (lat_min + lat_max) / 2);
        double max_km = 0;
        for (const auto& item : *emd.locations)
        {
          const auto& info = item.second;
          max_km = std::max(
              max_km, distance_in_kilometers(centre, {info.longitude, info.latitude}));
        }
        req.setParameter("maxdistance", Fmi::to_string(max_km + 1.0));
      }

      parseCoords(emd, coords);

      return;
    }

    if (emd.locations->find(location_id) == emd.locations->end())
    {
      throw EDRNotFound("Invalid locationId '" + location_id + "' for collection '" +
                         itsEDRQuery.collection_id +
                         "'! Please check metadata for valid locationIds!");
    }

    const auto& location_info = emd.locations->at(location_id);

    // Currently locationId is fmisid, geoid, ICAO code, or a station id embedded in
    // nongrid querydata (qdstation)
    if (location_info.type == "ICAO")
      req.addParameter("icao", location_id);
    else if (location_info.type == "fmisid")
      req.addParameter("fmisid", location_id);
    else if (location_info.type == "qdstation")
    {
      // Convert to a Position query at the station's own cached coordinates (byte-identical
      // to the querydata source), reusing the existing nongrid nearest-station pointQuery()
      // path in QEngineQuery.cpp - same trick as the location_id == "all" -> Area conversion
      // above.
      itsEDRQuery.query_type = EDRQueryType::Position;
      coords = "POINT(" + Fmi::to_string(location_info.longitude) + " " +
              Fmi::to_string(location_info.latitude) + ")";
      parseCoords(emd, coords);
    }
    else
      req.addParameter("geoid", location_id);
  }
  catch (...)
  {
    throw Fmi::Exception::Trace(BCP, "Operation failed!");
  }
}

void EDRQueryParams::parseCube(const EDRMetaData& emd)
{
  try
  {
    auto bbox = Spine::optional_string(req.getParameter("bbox"), "");
    if (bbox.empty())
      throw EDRException("Query parameter 'bbox' or 'coords' must be defined for Cube");

    // z values are not converted, height-units may only be the unit of the levels
    auto height_units = Spine::optional_string(req.getParameter("height-units"), "");
    if (!height_units.empty() && !emd.vertical_extent.levels.empty())
    {
      auto native_unit = VerticalReference::native_height_unit(emd.vertical_extent.level_type);
      if (height_units != native_unit && !(native_unit == "hPa" && height_units == "mbar"))
        throw EDRException("Invalid height-units option '" + height_units +
                           "' for cube query, the levels of collection '" +
                           itsEDRQuery.collection_id + "' are in unit '" + native_unit + "'");
    }

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
    if (itsEDRQuery.crs)
    {
      itsRequestXYWkt = itsEDRQuery.crs->toXY(wkt);
      wkt = itsEDRQuery.crs->toWGS84(itsRequestXYWkt);
    }
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
    const bool requested_datetime = !datetime.empty();

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

    // OGC API-EDR: date-times are RFC 3339 values, interpret ones without a UTC offset as UTC
    // instead of the server's local time
    if (!req.getParameter("tz"))
      req.addParameter("tz", "UTC");

    datetime = Fmi::trim_copy(datetime);

    // A list of date-times or a repeating interval Rn/start/duration: query the covered time
    // interval, the output is then limited to the listed times
    if (datetime.find(',') != std::string::npos ||
        (!datetime.empty() && std::toupper(datetime.front()) == 'R'))
    {
      parseDateTimeList(datetime);
      if (requested_datetime)
        for (const auto& t : itsDateTimeList)
          checkModelTimeStep(emd, t);
      req.addParameter("starttime", Fmi::to_iso_string(*itsDateTimeList.begin()) + "Z");
      req.addParameter("endtime", Fmi::to_iso_string(*itsDateTimeList.rbegin()) + "Z");
      return;
    }

    if (datetime.find('/') != std::string::npos)
    {
      std::vector<std::string> datetime_parts;
      boost::algorithm::split(datetime_parts, datetime, boost::algorithm::is_any_of("/"));
      if (datetime_parts.size() != 2)
        throw EDRException("Invalid 'datetime' parameter value '" + datetime + "'");

      const auto starttime = Fmi::trim_copy(datetime_parts.at(0));
      const auto endtime = Fmi::trim_copy(datetime_parts.at(1));

      // Durations are not allowed
      if (starttime.empty() || std::toupper(starttime[0]) == 'P')
        throw EDRException("Invalid 'datetime' interval start '" + starttime + "'");
      if (endtime.empty() || std::toupper(endtime[0]) == 'P')
        throw EDRException("Invalid 'datetime' interval end '" + endtime + "'");

      // Open ends (..) extend to the start or the end of the data
      req.addParameter("starttime", starttime == ".." ? "data" : starttime);
      req.addParameter("endtime", endtime == ".." ? "data" : endtime);
      return;
    }

    if (!datetime.empty() && std::toupper(datetime[0]) == 'P')
      throw EDRException("Invalid 'datetime' parameter value '" + datetime + "'");

    if (requested_datetime)
    {
      std::optional<Fmi::DateTime> t;
      try
      {
        t = Fmi::TimeParser::parse(datetime);
      }
      catch (...)
      {
      }
      if (t)
        checkModelTimeStep(emd, *t);
    }

    req.addParameter("starttime", datetime);
    req.addParameter("endtime", datetime);
  }
  catch (...)
  {
    throw Fmi::Exception::Trace(BCP, "Operation failed!");
  }
}

void EDRQueryParams::parseResolutionZ(const EDRMetaData& emd, const Config& config)
{
  try
  {
    auto opt = req.getParameter("resolution-z");
    if (!opt)
      return;

    if (itsEDRQuery.query_type != EDRQueryType::Cube &&
        itsEDRQuery.query_type != EDRQueryType::Corridor)
      throw EDRException(
          "Query parameter 'resolution-z' is supported only for cube and corridor queries");

    // OGC API-EDR: the number of levels including the minimum and the maximum, 0 means the
    // levels of the data
    // At most 1000 levels unless the request limits of the configuration say otherwise
    const auto& limits = config.requestLimits();
    const long max_levels = (limits.maxlevels > 0 ? long(limits.maxlevels) : 1000);
    long nz = -1;
    try
    {
      std::size_t pos = 0;
      nz = std::stol(*opt, &pos);
      if (pos != opt->size())
        nz = -1;
    }
    catch (...)
    {
    }
    if (nz != 0 && (nz < 2 || nz > max_levels))
      throw EDRException("Invalid 'resolution-z' value '" + *opt +
                         "', valid values are 0 for the levels of the data and 2-" +
                         Fmi::to_string(max_levels) +
                         " for the number of levels including the minimum and maximum");
    if (nz == 0)
      return;

    const auto& extent = emd.vertical_extent;
    if (extent.levels.empty())
      throw EDRException("Query parameter 'resolution-z' cannot be used, collection '" +
                         itsEDRQuery.collection_id + "' has no vertical extent");

    // Vertical interpolation is available for pressure and height levels
    using VerticalReference::LevelKind;
    const auto kind = VerticalReference::kind(extent.level_type);
    const bool pressure = (kind == LevelKind::PressureHPa || kind == LevelKind::PressurePa);
    const bool height =
        (kind == LevelKind::HeightAboveGround || kind == LevelKind::AltitudeAboveMSL);
    // The grid engine does not interpolate to other than the levels of the data
    if (emd.isGridProducer())
      throw EDRException("Query parameter 'resolution-z' is not supported for collection '" +
                         itsEDRQuery.collection_id + "', its data is not interpolated vertically");

    if ((!pressure && !height) || emd.isObsProducer())
      throw EDRException(
          "Query parameter 'resolution-z' cannot be used, the levels of collection '" +
          itsEDRQuery.collection_id + "' cannot be interpolated");

    double extent_min = std::numeric_limits<double>::max();
    double extent_max = std::numeric_limits<double>::lowest();
    for (const auto& level : extent.levels)
    {
      const auto value = Fmi::stod(level);
      extent_min = std::min(extent_min, value);
      extent_max = std::max(extent_max, value);
    }

    // The range of the z parameter (for corridors the corridor-height band), or all levels
    double zmin = extent_min;
    double zmax = extent_max;
    auto z = Spine::optional_string(req.getParameter("z"), "");
    if (!z.empty())
    {
      bool is_range = false;
      std::string lo;
      std::string hi;
      if (!UtilityFunctions::parseRangeListValue(z, is_range, lo, hi))
        throw EDRException("Invalid 'z' query parameter '" + z + "'");
      zmin = Fmi::stod(lo);
      zmax = (hi.empty() ? zmin : Fmi::stod(hi));
      if (zmin > zmax)
        std::swap(zmin, zmax);
      zmin = std::max(zmin, extent_min);
      zmax = std::min(zmax, extent_max);
      if (zmin > zmax)
        throw EDRException(
            "Query parameter 'z' does not overlap the vertical extent of collection '" +
            itsEDRQuery.collection_id + "'");
    }

    std::string values;
    for (long k = 0; k < nz; k++)
    {
      const double level = zmin + (zmax - zmin) * double(k) / double(nz - 1);
      itsInterpolatedLevels.push_back(level);
      if (!values.empty())
        values += ',';
      values += fmt::format("{}", level);
    }

    req.removeParameter("z");
    req.removeParameter(pressure ? "pressures" : "heights");
    req.addParameter(pressure ? "pressures" : "heights", values);
  }
  catch (...)
  {
    throw Fmi::Exception::Trace(BCP, "Operation failed!");
  }
}

namespace
{
// Spherical geometry for sampling corridors
const double earth_radius = 6371008.8;  // metres

struct LonLatPoint
{
  double lon;
  double lat;
};

double to_rad(double deg)
{
  return deg * M_PI / 180;
}

double to_deg(double rad)
{
  return rad * 180 / M_PI;
}

double distance(const LonLatPoint& a, const LonLatPoint& b)
{
  const double dlat = to_rad(b.lat - a.lat);
  const double dlon = to_rad(b.lon - a.lon);
  const double h = std::sin(dlat / 2) * std::sin(dlat / 2) +
                   std::cos(to_rad(a.lat)) * std::cos(to_rad(b.lat)) * std::sin(dlon / 2) *
                       std::sin(dlon / 2);
  return 2 * earth_radius * std::asin(std::min(1.0, std::sqrt(h)));
}

// Initial bearing from a to b in degrees
double bearing(const LonLatPoint& a, const LonLatPoint& b)
{
  const double lat1 = to_rad(a.lat);
  const double lat2 = to_rad(b.lat);
  const double dlon = to_rad(b.lon - a.lon);
  const double y = std::sin(dlon) * std::cos(lat2);
  const double x =
      std::cos(lat1) * std::sin(lat2) - std::sin(lat1) * std::cos(lat2) * std::cos(dlon);
  return to_deg(std::atan2(y, x));
}

// The point at the given distance (metres) and bearing (degrees) from p
LonLatPoint destination(const LonLatPoint& p, double brng, double dist)
{
  const double delta = dist / earth_radius;
  const double theta = to_rad(brng);
  const double lat1 = to_rad(p.lat);
  const double lon1 = to_rad(p.lon);
  const double lat2 = std::asin(std::sin(lat1) * std::cos(delta) +
                                std::cos(lat1) * std::sin(delta) * std::cos(theta));
  const double lon2 =
      lon1 + std::atan2(std::sin(theta) * std::sin(delta) * std::cos(lat1),
                        std::cos(delta) - std::sin(lat1) * std::sin(lat2));
  return {std::remainder(to_deg(lon2), 360.0), to_deg(lat2)};
}

}  // namespace

void EDRQueryParams::parseCorridorResolution(std::size_t nx, std::size_t ny)
{
  try
  {
    // The path in WGS84 with the corridor width in kilometres appended as ":width"
    auto wkt = Spine::optional_string(req.getParameter("wkt"), "");
    auto colon = wkt.rfind(':');
    if (colon == std::string::npos || wkt.rfind(')') > colon)
      throw EDRException("Corridor width is missing");
    const double width = Fmi::stod(wkt.substr(colon + 1)) * 1000;
    wkt.resize(colon);

    if (!itsCoordinateFilter.getDatetime().empty())
      throw EDRException(
          "Query parameters 'resolution-x' and 'resolution-y' cannot be used with "
          "LINESTRINGM or LINESTRINGZM coordinates");

    OGRGeometry* geom = nullptr;
    if (OGRGeometryFactory::createFromWkt(wkt.c_str(), nullptr, &geom) != OGRERR_NONE || !geom)
      throw EDRException("Failed to parse the corridor path: " + wkt);
    std::unique_ptr<OGRGeometry> geometry(geom);
    if (wkbFlatten(geometry->getGeometryType()) != wkbLineString)
      throw EDRException(
          "Query parameters 'resolution-x' and 'resolution-y' require a LINESTRING corridor");

    std::vector<LonLatPoint> path;
    const auto* line = geometry->toLineString();
    for (int k = 0; k < line->getNumPoints(); k++)
      path.push_back({line->getX(k), line->getY(k)});

    std::vector<double> cumulative{0};
    for (std::size_t k = 1; k < path.size(); k++)
      cumulative.push_back(cumulative.back() + distance(path[k - 1], path[k]));
    const double length = cumulative.back();
    if (path.size() < 2 || length <= 0)
      throw EDRException("The corridor path must have a non-zero length");

    // Positions along the path at equal distances, and across the corridor at equal offsets
    // perpendicular to the path. The positions are ordered by the offset first.
    std::vector<double> xs(nx * ny);
    std::vector<double> ys(nx * ny);
    std::size_t segment = 1;
    for (std::size_t i = 0; i < nx; i++)
    {
      const double s = length * double(i) / double(nx - 1);
      while (segment + 1 < path.size() && cumulative[segment] < s)
        segment++;
      const auto& a = path[segment - 1];
      const auto& b = path[segment];
      const double seglen = cumulative[segment] - cumulative[segment - 1];
      const double fraction =
          (seglen > 0 ? std::clamp((s - cumulative[segment - 1]) / seglen, 0.0, 1.0) : 0.0);

      // Linear interpolation within a segment and the initial bearing of the segment are
      // accurate enough for corridor sized segments
      const LonLatPoint p{a.lon + fraction * (b.lon - a.lon), a.lat + fraction * (b.lat - a.lat)};
      const double brng = bearing(a, b);

      for (std::size_t j = 0; j < ny; j++)
      {
        const double offset = (ny == 1 ? 0.0 : -width / 2 + width * double(j) / double(ny - 1));
        const auto q = (offset == 0 ? p : destination(p, brng + 90, offset));
        xs[j * nx + i] = q.lon;
        ys[j * nx + i] = q.lat;
      }
    }

    std::string points;
    for (std::size_t k = 0; k < xs.size(); k++)
    {
      if (!points.empty())
        points += ',';
      points += fmt::format("({} {})", xs[k], ys[k]);
    }
    req.removeParameter("wkt");
    req.addParameter("wkt", "MULTIPOINT(" + points + ")");

    // The output coordinates are in the requested CRS
    if (itsEDRQuery.crs)
      itsEDRQuery.crs->fromWGS84(xs, ys);

    EDRCorridorGrid corridor_grid;
    corridor_grid.nx = nx;
    corridor_grid.ny = ny;
    corridor_grid.width = width;
    corridor_grid.x = std::move(xs);
    corridor_grid.y = std::move(ys);
    itsEDRQuery.corridor_grid = std::move(corridor_grid);

    // The positions are queried as interpolated positions
    itsEDRQuery.query_type = EDRQueryType::Position;
  }
  catch (...)
  {
    throw Fmi::Exception::Trace(BCP, "Operation failed!");
  }
}

void EDRQueryParams::parseResolution(const EDRMetaData& emd, const Config& config)
{
  try
  {
    auto opt_x = req.getParameter("resolution-x");
    auto opt_y = req.getParameter("resolution-y");
    if (!opt_x && !opt_y)
      return;

    const bool corridor = (itsEDRQuery.query_type == EDRQueryType::Corridor);
    if (itsEDRQuery.query_type != EDRQueryType::Area &&
        itsEDRQuery.query_type != EDRQueryType::Cube && !corridor)
      throw EDRException(
          "Query parameters 'resolution-x' and 'resolution-y' are supported only for area, cube "
          "and corridor queries");

    // OGC API-EDR: the number of positions along the axis including the minimum and the
    // maximum, 0 means the native resolution of the data. For corridors x is along the path
    // and y across the corridor, where 1 means the centre line only.
    const std::size_t max_positions = 1000;
    auto parse = [max_positions](const std::optional<std::string>& value,
                                 const char* name,
                                 long min_positions)
    {
      if (!value)
        throw EDRException(std::string("Query parameter '") + name +
                           "' is required with resolution-x/resolution-y");
      try
      {
        std::size_t pos = 0;
        auto n = std::stol(*value, &pos);
        if (pos == value->size() &&
            (n == 0 || (n >= min_positions && n <= long(max_positions))))
          return static_cast<std::size_t>(n);
      }
      catch (...)
      {
      }
      throw EDRException(std::string("Invalid '") + name + "' value '" + *value +
                         "', valid values are 0 for the native resolution and " +
                         Fmi::to_string(min_positions) + "-" + Fmi::to_string(max_positions) +
                         " for the number of positions including the minimum and maximum");
    };

    auto nx = parse(opt_x, "resolution-x", 2);
    auto ny = parse(opt_y, "resolution-y", corridor ? 1 : 2);

    if (nx == 0 && ny == 0)
      return;
    if (nx == 0 || ny == 0)
      throw EDRException(
          "Query parameters 'resolution-x' and 'resolution-y' must both be 0 (native "
          "resolution) or both positive");

    // Interpolation to a regular grid is possible for gridded data only
    bool point_data = (emd.isObsProducer() || emd.isAviProducer());
    if (emd.locations)
      for (const auto& item : *emd.locations)
        point_data = (point_data || item.second.type == "qdstation");
    if (point_data)
      throw EDRException("Collection '" + itsEDRQuery.collection_id +
                         "' contains point data, resolution-x and resolution-y can be used "
                         "with gridded data only");

    const auto& limits = config.requestLimits();
    // Limits of the interpolated positions and positions times levels unless the request limits
    // of the configuration say otherwise
    const std::size_t max_points = (limits.maxlocations > 0 ? limits.maxlocations : 250000);
    if (nx * ny > max_points)
      throw EDRTooLarge("Too many grid positions " + Fmi::to_string(nx * ny) +
                        " requested with resolution-x and resolution-y, the maximum is " +
                        Fmi::to_string(max_points));
    const std::size_t max_elements = (limits.maxelements > 0 ? limits.maxelements : 1000000);
    const std::size_t nz = std::max<std::size_t>(1, itsInterpolatedLevels.size());
    if (nx * ny * nz > max_elements)
      throw EDRTooLarge("Too many grid positions and levels " + Fmi::to_string(nx * ny * nz) +
                        " requested with resolution-x/y/z, the maximum is " +
                        Fmi::to_string(max_elements));

    if (corridor)
    {
      parseCorridorResolution(nx, ny);
      return;
    }

    // The grid is regular in the requested CRS
    auto wkt = (itsEDRQuery.crs ? itsRequestXYWkt
                                : Spine::optional_string(req.getParameter("wkt"), ""));
    OGRGeometry* geom = nullptr;
    if (OGRGeometryFactory::createFromWkt(wkt.c_str(), nullptr, &geom) != OGRERR_NONE || !geom)
      throw EDRException("Failed to parse the area of the query: " + wkt);
    std::unique_ptr<OGRGeometry> geometry(geom);

    OGREnvelope envelope;
    geometry->getEnvelope(&envelope);

    EDRGrid grid;
    grid.nx = nx;
    grid.ny = ny;
    grid.x0 = envelope.MinX;
    grid.x1 = envelope.MaxX;
    grid.y0 = envelope.MinY;
    grid.y1 = envelope.MaxY;

    // Positions inside the area (cube positions are always inside the bounding box). A
    // prepared geometry makes the test fast also for complex polygons.
    std::unique_ptr<OGRPreparedGeometry, void (*)(OGRPreparedGeometry*)> prepared(
        (itsEDRQuery.query_type == EDRQueryType::Area
             ? OGRCreatePreparedGeometry(OGRGeometry::ToHandle(geometry.get()))
             : nullptr),
        [](OGRPreparedGeometry* g)
        {
          if (g)
            OGRDestroyPreparedGeometry(g);
        });
    std::vector<double> xs;
    std::vector<double> ys;
    for (std::size_t j = 0; j < ny; j++)
      for (std::size_t i = 0; i < nx; i++)
      {
        const auto x = grid.x(i);
        const auto y = grid.y(j);
        if (itsEDRQuery.query_type == EDRQueryType::Area)
        {
          OGRPoint point(x, y);
          const bool inside =
              (prepared
                   ? OGRPreparedGeometryIntersects(prepared.get(), OGRGeometry::ToHandle(&point))
                   : geometry->Intersects(&point));
          if (!inside)
            continue;
        }
        xs.push_back(x);
        ys.push_back(y);
      }

    if (itsEDRQuery.crs)
      itsEDRQuery.crs->toWGS84(xs, ys);

    std::string points;
    for (std::size_t k = 0; k < xs.size(); k++)
    {
      if (!points.empty())
        points += ',';
      points += fmt::format("({} {})", xs[k], ys[k]);
    }

    if (points.empty())
      throw EDRException("No positions of the requested grid are inside the area");

    req.removeParameter("wkt");
    req.addParameter("wkt", "MULTIPOINT(" + points + ")");

    // The grid is queried as interpolated positions
    itsEDRQuery.grid = grid;
    itsEDRQuery.query_type = EDRQueryType::Position;
  }
  catch (...)
  {
    throw Fmi::Exception::Trace(BCP, "Operation failed!");
  }
}

void EDRQueryParams::parseItemsQuery(const EDRMetaData& emd, const Config& config)
{
  try
  {
    // OGC API-EDR items: the messages of aviation collections are the items
    const auto& queries = config.getSupportedDataQueries(itsEDRQuery.collection_id);
    if (!emd.isAviProducer() || queries.find("items") == queries.end())
      throw EDRNotFound("Collection '" + itsEDRQuery.collection_id + "' has no items");

    itsEDRQuery.bbox = Spine::optional_string(req.getParameter("bbox"), "");
    itsEDRQuery.datetime = Spine::optional_string(req.getParameter("datetime"), "");

    auto crs = Spine::optional_string(req.getParameter("crs"), "");
    if (!is_crs84(crs))
      throw EDRException("Invalid crs: " + crs + ". Items are available in OGC:CRS84 only");

    auto f = Spine::optional_string(req.getParameter("f"), GEO_JSON_FORMAT);
    if (!boost::algorithm::iequals(f, GEO_JSON_FORMAT))
      throw EDRException("Invalid output format '" + f + "', items are available as GeoJSON");

    // OGC API Features: 10 items by default
    parseLimitAndOffset();
    if (!itsEDRQuery.limit)
      itsEDRQuery.limit = 10;

    mime_type = "application/geo+json";
  }
  catch (...)
  {
    throw Fmi::Exception::Trace(BCP, "Operation failed!");
  }
}

void EDRQueryParams::parseLimitAndOffset()
{
  // OGC API-EDR /req/edr/rc-limit-definition: 1..10000, larger values are not an error
  // but are reduced to the maximum
  const std::size_t max_limit = 10000;

  auto parse = [this](const char* name, long min_value) -> std::optional<long>
  {
    auto value = req.getParameter(name);
    if (!value)
      return {};
    try
    {
      std::size_t pos = 0;
      auto n = std::stol(*value, &pos);
      if (pos == value->size() && n >= min_value)
        return n;
    }
    catch (...)
    {
    }
    throw EDRException("Invalid '" + std::string(name) + "' parameter value '" + *value +
                       "', an integer >= " + Fmi::to_string(min_value) + " is required");
  };

  if (auto limit = parse("limit", 1))
    itsEDRQuery.limit = std::min(static_cast<std::size_t>(*limit), max_limit);
  if (auto offset = parse("offset", 0))
    itsEDRQuery.offset = static_cast<std::size_t>(*offset);
}

void EDRQueryParams::checkModelTimeStep(const EDRMetaData& emd, const Fmi::DateTime& t) const
{
  // MetOcean profile /req/nwp/collection_data_queries C: a time between the time steps of a
  // model is an error. Observations and aviation messages have no fixed time steps. The
  // extent of an instance may differ from the latest one, instance queries are not checked.
  if (emd.isObsProducer() || emd.isAviProducer() || !itsEDRQuery.instance_id.empty())
    return;

  const auto& extent = emd.temporal_extent;
  if (extent.time_periods.empty())
    return;

  // Times outside the extent have no data at all
  const auto& first = extent.time_periods.front().start_time;
  const auto& last_period = extent.time_periods.back();
  const auto& last =
      (last_period.end_time.is_not_a_date_time() ? last_period.start_time : last_period.end_time);
  if (t < first || t > last)
    return;

  for (const auto& period : extent.single_time_periods)
    if (period.start_time == t)
      return;

  for (const auto& period : extent.time_periods)
  {
    if (period.end_time.is_not_a_date_time())
    {
      if (period.start_time == t)
        return;
    }
    else if (t >= period.start_time && t <= period.end_time)
    {
      // Unknown time step, any time inside the period is accepted
      if (period.timestep <= 0)
        return;
      const auto minutes = (t - period.start_time).total_seconds() / 60.0;
      if (std::fmod(minutes, period.timestep) == 0)
        return;
    }
  }

  throw EDRException("Time " + Fmi::date_time::to_iso_extended_string(t) +
                     "Z is between the time steps of collection '" + itsEDRQuery.collection_id +
                     "'");
}

void EDRQueryParams::parseDateTimeList(const std::string& datetime)
{
  try
  {
    const std::size_t max_times = 10000;

    if (std::toupper(datetime.front()) == 'R')
    {
      // Repeating interval Rn/start/duration, n times starting from the given time
      std::vector<std::string> parts;
      boost::algorithm::split(parts, datetime, boost::algorithm::is_any_of("/"));
      if (parts.size() != 3)
        throw EDRException("Invalid 'datetime' repeating interval '" + datetime +
                           "', Rn/start/duration expected");

      const auto count = Fmi::stoi(Fmi::trim_copy(parts[0].substr(1)));
      if (count <= 0 || static_cast<std::size_t>(count) > max_times)
        throw EDRException("Invalid number of repetitions in 'datetime' value '" + datetime +
                           "'");

      auto t = Fmi::TimeParser::parse(Fmi::trim_copy(parts[1]));
      const auto step = Fmi::TimeParser::parse_iso_duration(Fmi::trim_copy(parts[2]));
      if (step.total_seconds() <= 0)
        throw EDRException("Invalid duration in 'datetime' value '" + datetime + "'");

      for (int i = 0; i < count; i++, t += step)
        itsDateTimeList.insert(t);
      return;
    }

    std::vector<std::string> parts;
    boost::algorithm::split(parts, datetime, boost::algorithm::is_any_of(","));
    if (parts.size() > max_times)
      throw EDRException("Too many times in the 'datetime' list");

    for (auto part : parts)
    {
      boost::algorithm::trim(part);
      if (part.empty() || part.find('/') != std::string::npos ||
          std::toupper(part.front()) == 'P')
        throw EDRException("Invalid 'datetime' list element '" + part +
                           "', only date-times are allowed in a list");
      itsDateTimeList.insert(Fmi::TimeParser::parse(part));
    }
  }
  catch (...)
  {
    throw Fmi::Exception::Trace(BCP, "Invalid 'datetime' parameter value '" + datetime + "'");
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
      boost::algorithm::trim_copy(Spine::optional_string(req.getParameter("custom_level"), ""));

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
          levels.push_back(Fmi::stod(s));
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

    // OGC API-EDR: the values must be from the parameter_names of the collection metadata
    std::string unknown_names;
    for (auto p : param_names)
    {
      boost::algorithm::trim(p);
      boost::algorithm::to_lower(p);
      if (p.find(':') != std::string::npos)
        p = p.substr(0, p.find(':'));
      if (emd.parameters.find(p) == emd.parameters.end())
        unknown_names += (unknown_names.empty() ? "'" : ", '") + p + "'";
    }
    if (!unknown_names.empty())
      throw EDRException("Unknown parameter-name value(s) " + unknown_names +
                         " for collection '" + itsEDRQuery.collection_id +
                         "', see the parameter_names of the collection metadata");

    for (auto p : param_names)
    {
      boost::algorithm::trim(p);
      boost::algorithm::to_lower(p);
      if (p.find(':') != std::string::npos)
        p = p.substr(0, p.find(':'));

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
    // Levels interpolated with resolution-z replace the levels of the data
    if (!itsInterpolatedLevels.empty())
      return parseParameterNames(state, emd, grid_producer, noReqParams, customDimensions, "");

    auto z = Spine::optional_string(req.getParameter("z"), "");
    double min_level = std::numeric_limits<double>::min();
    double max_level = std::numeric_limits<double>::max();
    bool hasZParam = !z.empty();
    bool range = false;

    std::string zLo;
    std::string zHi;
    bool zWasRange = false;
    if (UtilityFunctions::parseRangeListValue(z, range, zLo, zHi))
    {
      if (range)
      {
        min_level = Fmi::stod(zLo);
        max_level = Fmi::stod(zHi);
        z.clear();
        zWasRange = true;
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

    if (zWasRange && z.empty())
      throw EDRException(
          "Query parameter 'z' (range " + Fmi::to_string(min_level) + "-" +
          Fmi::to_string(max_level) + ") did not match any vertical level of collection '" +
          itsEDRQuery.collection_id +
          "'. Check that 'z' (and for Corridor queries, 'corridor-height'/'height-units') use "
          "the same vertical unit as the collection's vertical extent.");

    if (!z.empty())
      req.addParameter(zParameter, z);
    req.addParameter("levelrange", range ? "1" : "");

    return parseParameterNames(state, emd, grid_producer, noReqParams, customDimensions, z);
  }
  catch (...)
  {
    throw Fmi::Exception::Trace(BCP, "Operation failed!");
  }
}

std::string EDRQueryParams::parseParameterNames(const State& state,
                                                const EDRMetaData& emd,
                                                bool grid_producer,
                                                bool& noReqParams,
                                                const CustomDimensions& customDimensions,
                                                const std::string& z)
{
  try
  {
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
