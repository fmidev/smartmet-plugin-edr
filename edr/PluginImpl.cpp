// ======================================================================
/*!
 * \brief Smartmet EDR plugin implementation (reloadable portion)
 */
// ======================================================================

#include "PluginImpl.h"
#include "ApiDefinition.h"
#include "EDRException.h"
#include "ProducerCatalog.h"
#include "QueryProcessingHub.h"
#include "State.h"
#include "TimeSeriesQuery.h"
#include "UtilityFunctions.h"
#include <engines/gis/Engine.h>
#include <fmt/format.h>
#include <macgyver/Hash.h>
#include <macgyver/StringConversion.h>
#include <boost/algorithm/string.hpp>
#include <macgyver/TimeParser.h>
#include <spine/Convenience.h>
#include <spine/FmiApiKey.h>
#include <spine/HostInfo.h>
#include <spine/ImageFormatter.h>
#include <spine/Reactor.h>
#include <spine/TableFormatterFactory.h>
#include <timeseries/ParameterKeywords.h>
#include <array>
#include <iostream>

// #define MYDEBUG ON

namespace SmartMet
{
namespace Plugin
{
namespace EDR
{
namespace
{
// Maximum size of the Link header
const std::size_t max_link_header_size = 4096;

// Escape a string for embedding into a JSON string value
const char* const PROBLEM_DETAIL_JSON = R"JSON({
  "title" : "An error occurred",
  "status" : __STATUS__,
  "detail" : "__DETAIL__"
}
)JSON";

std::string json_escape(const std::string& str)
{
  std::string ret;
  ret.reserve(str.size());
  for (unsigned char c : str)
  {
    switch (c)
    {
      case '"':
        ret += "\\\"";
        break;
      case '\\':
        ret += "\\\\";
        break;
      case '\n':
        ret += "\\n";
        break;
      case '\r':
        ret += "\\r";
        break;
      case '\t':
        ret += "\\t";
        break;
      default:
        if (c < 0x20)
          ret += fmt::format("\\u{:04x}", static_cast<unsigned int>(c));
        else
          ret += static_cast<char>(c);
    }
  }
  return ret;
}


std::set<std::string> get_metadata_parameters(const std::shared_ptr<EngineMetaData>& metadata)
{
  std::set<std::string> parameter_names;

  // Iterate metadata of engines
  for (const auto& item : metadata->getMetaData())
  {
    // Iterate metadata of producers
    for (const auto& item2 : item.second)
    {
      // Iterate metadata of single producer
      for (const auto& md : item2.second)
      {
        // Iterate parameter names
        for (const auto& pname : md.parameter_names)
          parameter_names.insert(pname);
      }
    }
  }
  return parameter_names;
}

Spine::TableFormatter* get_formatter_and_qstreamer(const CommonQuery& q,
                                                   QueryServer::QueryStreamer_sptr& queryStreamer)
{
  try
  {
    if (strcasecmp(q.format.c_str(), "IMAGE") == 0)
      return new Spine::ImageFormatter();

    if (strcasecmp(q.format.c_str(), "FILE") == 0)
    {
      auto* qStreamer = new QueryServer::QueryStreamer();
      queryStreamer.reset(qStreamer);
      return new Spine::ImageFormatter();
    }

    if (strcasecmp(q.format.c_str(), "INFO") == 0)
      return Spine::TableFormatterFactory::create("debug");

    return Spine::TableFormatterFactory::create(q.format);
  }
  catch (...)
  {
    throw Fmi::Exception::Trace(BCP, "Operation failed!");
  }
}

Spine::TableFormatter::Names get_headers(const std::vector<Spine::Parameter>& parameters)
{
  try
  {
    // The names of the columns
    Spine::TableFormatter::Names headers;

    for (const Spine::Parameter& p : parameters)
    {
      std::string header_name = p.alias();
      std::vector<std::string> partList;
      splitString(header_name, ':', partList);
      // There was a merge conflict in here at one time. GRIB branch processed
      // these two special cases, while master added the sensor part below. This
      // may be incorrect.
      if (partList.size() > 2 && (partList[0] == "ISOBANDS" || partList[0] == "ISOLINES"))
      {
        const char* param_header =
            header_name.c_str() + partList[0].size() + partList[1].size() + 2;
        headers.emplace_back(param_header);
      }
      else
      {
        const std::optional<int>& sensor_no = p.getSensorNumber();
        if (sensor_no)
        {
          header_name += ("_#" + Fmi::to_string(*sensor_no));
        }
        if (!p.getSensorParameter().empty())
          header_name += ("_" + p.getSensorParameter());
        headers.push_back(header_name);
      }
    }

    return headers;
  }
  catch (...)
  {
    throw Fmi::Exception::Trace(BCP, "Operation failed!");
  }
}

std::string get_wxml_type(const std::string& producer_option,
                          const std::set<std::string>& obs_station_types)
{
  try
  {
    // If query is fast, we do not not have observation producers
    // This means we put 'forecast' into wxml-tag
    std::string wxml_type = "forecast";

    for (const auto& obsProducer : obs_station_types)
    {
      if (boost::algorithm::contains(producer_option, obsProducer))
      {
        // Observation mentioned, use 'observation' wxml type
        wxml_type = "observation";
        break;
      }
    }

    return wxml_type;
  }
  catch (...)
  {
    throw Fmi::Exception::Trace(BCP, "Operation failed!");
  }
}

// Quick check on request limits. The counting limits have TS::RequestLimitMember
// counterparts and are enforced by the engine query classes, but maxradius has none,
// so it can only be checked here.

void check_limits(const Spine::TaggedLocationList& locations, const TS::RequestLimits& limits)
{
  if (limits.maxradius <= 0)
    return;

  double maxradius = 0;
  for (const auto& tloc : locations)
    if (tloc.loc)
      maxradius = std::max(maxradius, tloc.loc->radius);

  if (maxradius > limits.maxradius)
    throw EDRTooLarge("Too large radius requested")
        .addParameter("Radius", Fmi::to_string(maxradius))
        .addParameter("Limit", Fmi::to_string(limits.maxradius));
}

// The tag suffix distinguishes the endpoint the entity was produced by. The /timeseries
// endpoint uses "timeseries" so that the ETags stay identical to the ones the standalone
// timeseries plugin produced for the same request.

bool etag_only(const Spine::HTTP::Request& request,
               Spine::HTTP::Response& response,
               std::size_t product_hash,
               const std::string& tag_suffix = "edr")
{
  try
  {
    if (product_hash != Fmi::bad_hash)
    {
      auto etag = fmt::format("\"{:x}-{}\"", product_hash, tag_suffix);
      response.setHeader("ETag", etag);

      // If the product is cacheable and etag was requested, respond with etag only

      if (request.getHeader("X-Request-ETag"))
      {
        response.setStatus(Spine::HTTP::Status::no_content);
        return true;
      }

      // Standalone conditional handling (RFC 7232): return 304 Not Modified when
      // the client already has this version (If-None-Match), or 412 Precondition
      // Failed when an If-Match precondition fails, with no body.
      if (auto status = Spine::HTTP::conditionalResponseStatus(request, etag))
      {
        response.setStatus(*status);
        return true;
      }
    }
    return false;
  }
  catch (...)
  {
    throw Fmi::Exception::Trace(BCP, "Operation failed!");
  }
}

void parse_lonlats(const std::optional<std::string>& lonlats,
                   Spine::HTTP::Request& request,
                   std::string& wkt_multipoint)
{
  try
  {
    if (!lonlats)
      return;

    request.removeParameter("lonlat");
    request.removeParameter("lonlats");
    std::vector<std::string> parts;
    boost::algorithm::split(parts, *lonlats, boost::algorithm::is_any_of(","));
    if (parts.size() % 2 != 0)
      throw Fmi::Exception(BCP, "Invalid lonlats list: " + *lonlats);

    for (unsigned int j = 0; j < parts.size(); j += 2)
    {
      if (wkt_multipoint != "MULTIPOINT(")
        wkt_multipoint += ",";
      wkt_multipoint += "(" + parts[j] + " " + parts[j + 1] + ")";
    }
  }
  catch (...)
  {
    throw Fmi::Exception::Trace(BCP, "Operation failed!");
  }
}

void parse_latlons(const std::optional<std::string>& latlons,
                   Spine::HTTP::Request& request,
                   std::string& wkt_multipoint)
{
  try
  {
    if (!latlons)
      return;

    request.removeParameter("latlon");
    request.removeParameter("latlons");
    std::vector<std::string> parts;
    boost::algorithm::split(parts, *latlons, boost::algorithm::is_any_of(","));
    if (parts.size() % 2 != 0)
      throw Fmi::Exception(BCP, "Invalid latlons list: " + *latlons);

    for (unsigned int j = 0; j < parts.size(); j += 2)
    {
      if (wkt_multipoint != "MULTIPOINT(")
        wkt_multipoint += ",";
      wkt_multipoint += "(" + parts[j + 1] + " " + parts[j] + ")";
    }
  }
  catch (...)
  {
    throw Fmi::Exception::Trace(BCP, "Operation failed!");
  }
}

void parse_places(const std::optional<std::string>& places,
                  Spine::HTTP::Request& request,
                  std::string& wkt_multipoint,
                  const Engine::Geonames::Engine* geoEngine)
{
  try
  {
    if (!places)
      return;

    request.removeParameter("places");
    std::vector<std::string> parts;
    boost::algorithm::split(parts, *places, boost::algorithm::is_any_of(","));
    for (const auto& place : parts)
    {
      Spine::LocationPtr loc = geoEngine->nameSearch(place, "fi");
      if (loc)
      {
        if (wkt_multipoint != "MULTIPOINT(")
          wkt_multipoint += ",";
        wkt_multipoint +=
            "(" + Fmi::to_string(loc->longitude) + " " + Fmi::to_string(loc->latitude) + ")";
      }
    }
  }
  catch (...)
  {
    throw Fmi::Exception::Trace(BCP, "Operation failed!");
  }
}

void parse_fmisids(const std::optional<std::string>& fmisid,
                   Spine::HTTP::Request& request,
                   std::vector<int>& fmisids)
{
  try
  {
    if (!fmisid)
      return;

    request.removeParameter("fmisid");
    std::vector<std::string> parts;
    boost::algorithm::split(parts, *fmisid, boost::algorithm::is_any_of(","));
    for (const auto& id : parts)
      fmisids.push_back(Fmi::stoi(id));
  }
  catch (...)
  {
    throw Fmi::Exception::Trace(BCP, "Operation failed!");
  }
}

void parse_lpnns(const std::optional<std::string>& lpnn,
                 Spine::HTTP::Request& request,
                 std::vector<int>& lpnns)
{
  try
  {
    if (!lpnn)
      return;

    request.removeParameter("lpnn");
    std::vector<std::string> parts;
    boost::algorithm::split(parts, *lpnn, boost::algorithm::is_any_of(","));
    for (const auto& id : parts)
      lpnns.push_back(Fmi::stoi(id));
  }
  catch (...)
  {
    throw Fmi::Exception::Trace(BCP, "Operation failed!");
  }
}

void parse_wmos(const std::optional<std::string>& wmo,
                Spine::HTTP::Request& request,
                std::vector<int>& wmos)
{
  try
  {
    if (!wmo)
      return;

    request.removeParameter("wmo");
    std::vector<std::string> parts;
    boost::algorithm::split(parts, *wmo, boost::algorithm::is_any_of(","));
    for (const auto& id : parts)
      wmos.push_back(Fmi::stoi(id));
  }
  catch (...)
  {
    throw Fmi::Exception::Trace(BCP, "Operation failed!");
  }
}

}  // namespace

// ----------------------------------------------------------------------
/*!
 * \brief Perform a EDR query
 */
// ----------------------------------------------------------------------

void PluginImpl::query(const State& state,
                       const Spine::HTTP::Request& request,
                       Spine::HTTP::Response& response)
{
  try
  {
    using std::chrono::duration_cast;
    using std::chrono::high_resolution_clock;
    using std::chrono::microseconds;

    Spine::Table data;

    // Options
    high_resolution_clock::time_point t1 = high_resolution_clock::now();

    QueryProcessingHub qph(*this);

    Query q(state, request, itsConfig);

    std::shared_ptr<std::string> obj;
    std::string timeheader;
    std::string producer_option;
    high_resolution_clock::time_point t3;
    std::shared_ptr<Spine::TableFormatter> formatter;
    QueryServer::QueryStreamer_sptr queryStreamer;

    if (!q.isEDRMetaDataQuery())
    {
      // Resolve locations for FMISDs,WMOs,LPNNs (https://jira.fmi.fi/browse/BRAINSTORM-1848)
      Engine::Geonames::LocationOptions lopt =
          itsEngines.geoEngine->parseLocations(q.fmisids, q.lpnns, q.wmos, q.language);

      const Spine::TaggedLocationList& locations = lopt.locations();
      Spine::TaggedLocationList tagged_ll = q.loptions->locations();
      tagged_ll.insert(tagged_ll.end(), locations.begin(), locations.end());
      q.loptions->setLocations(tagged_ll);

      // Radius arrives via the EDR 'within' parameter, encoded into the coords string
      check_limits(q.loptions->locations(), itsConfig.requestLimits());
    }

    high_resolution_clock::time_point t2 = high_resolution_clock::now();

    data.setPaging(0, 1);

    producer_option =
        Spine::optional_string(request.getParameter(PRODUCER_PARAM),
                               Spine::optional_string(request.getParameter(STATIONTYPE_PARAM), ""));
    boost::algorithm::to_lower(producer_option);

    // The formatter knows which mimetype to send
    Spine::TableFormatter* fmt = get_formatter_and_qstreamer(q, queryStreamer);

    bool gridEnabled = false;
    if (strcasecmp(q.forecastSource.c_str(), "grid") == 0 ||
        (q.forecastSource.empty() &&
         strcasecmp(itsConfig.primaryForecastSource().c_str(), "grid") == 0))
      gridEnabled = true;

    formatter.reset(fmt);
    std::string mime(q.mimeType());

    if (mime.empty())
    {
      if (q.output_format == IWXXM_FORMAT)
        mime = "text/xml";
      else if (q.output_format == IWXXMZIP_FORMAT)
        mime = "application/zip";
      else if (q.output_format == TAC_FORMAT)
        mime = "text/plain";
      else
        mime = "application/json";
    }
    response.setHeader("Content-Type", mime);

    // The output format of a data query is negotiated with the Accept header unless the f
    // option is given
    if (!q.isEDRMetaDataQuery() && !request.getParameter("f"))
      response.setHeader("Vary", "Accept");

    // Calculate the hash value for the product.

    std::size_t product_hash = Fmi::bad_hash;

    try
    {
      product_hash = qph.hash_value(state, request, q);
    }
    catch (...)
    {
      if (!gridEnabled)
        throw Fmi::Exception::Trace(BCP, "Operation failed!");
    }

    t3 = high_resolution_clock::now();

    timeheader = Fmi::to_string(duration_cast<microseconds>(t2 - t1).count()) + '+' +
                 Fmi::to_string(duration_cast<microseconds>(t3 - t2).count());

    // if (etag_only(request, response, product_hash))
    //   return;

    // If obj is not nullptr it is from cache
    obj = qph.processQuery(state, data, q, queryStreamer, product_hash);

    // OGC API-EDR recommendation 11: the links of the response also as Link headers. Data
    // responses link to the request in each output format and to the collection, paged
    // listings to the other pages. CoverageJSON, TAC and IWXXM have no links in the body.
    const Links links = (q.isEDRMetaDataQuery()
                             ? q.responseLinks
                             : QueryProcessingHub::dataQueryLinks(q.edrQuery(), q.output_format));
    // Recommendation 11 B: the links are left out of the header when there are too many of
    // them or they are long (e.g. the coords of a long trajectory), proxies limit header sizes
    if (!links.empty())
    {
      const auto header = link_header(links);
      if (header.size() <= max_link_header_size)
        response.setHeader("Link", header);
    }

    if (obj)
    {
      product_hash = Fmi::hash_value(*obj);
      if (etag_only(request, response, product_hash))
        return;

      response.setHeader("X-Duration", timeheader);
      response.setHeader("X-EDR-Cache", "yes");
      response.setContent(obj);
      return;
    }

    // Must generate the result from scratch
    response.setHeader("X-EDR-Cache", "no");

    high_resolution_clock::time_point t4 = high_resolution_clock::now();
    timeheader.append("+").append(
        Fmi::to_string(std::chrono::duration_cast<std::chrono::microseconds>(t4 - t3).count()));

    data.setMissingText(q.valueformatter.missing());

    // The names of the columns
    Spine::TableFormatter::Names headers = get_headers(q.poptions.parameters());

    // Format product
    // Deduce WXML-tag from used producer. (What happens when we combine forecasts and
    // observations??).

    std::string wxml_type = get_wxml_type(producer_option, itsObsEngineStationTypes);

    auto formatter_options = itsConfig.formatterOptions();
    formatter_options.setFormatType(wxml_type);

    auto out = formatter->format(data, headers, request, formatter_options);

    high_resolution_clock::time_point t5 = high_resolution_clock::now();
    timeheader.append("+").append(
        Fmi::to_string(std::chrono::duration_cast<std::chrono::microseconds>(t5 - t4).count()));

    // TODO: Should use std::move when it has become available
    std::shared_ptr<std::string> result(new std::string());
    std::swap(out, *result);

    // Too many flash data requests with empty output filling the logs...
#if 0
    if (result->empty())
    {
      std::cerr << "Warning: Empty output for request " << request.getQueryString() << " from "
                << request.getClientIP() << std::endl;
    }
#endif

#ifdef MYDEBUG
    std::cout << "Output:" << std::endl << *result << std::endl;
#endif

    response.setHeader("X-Duration", timeheader);

    if (strcasecmp(q.format.c_str(), "FILE") == 0)
    {
      std::string filename =
          "attachement; filename=timeseries_" + std::to_string(getTime()) + ".grib";
      response.setHeader("Content-type", "application/octet-stream");
      response.setHeader("Content-Disposition", filename);
      response.setContent(queryStreamer);
    }
    else
    {
      // No data is answered with 204 No Content. Such a response must not carry an
      // ETag: the frontend would cache the empty entity under it and serve it later
      // as a 200 response. Without an ETag the frontend passes the 204 through also
      // when answering its ETag probe.

      bool no_data = (q.output_format == IWXXMZIP_FORMAT
                          ? qph.getZipFileName().empty()
                          : (*result == (q.valueformatter.missing() + "\n")));

      if (no_data)
      {
        result->clear();
        response.setStatus(Spine::HTTP::Status::no_content);
        response.removeHeader("Content-type");
      }
      else
      {
        product_hash = Fmi::hash_value(*result);
        if (etag_only(request, response, product_hash))
          return;

        if (q.output_format == IWXXMZIP_FORMAT)
          response.setHeader("Content-Disposition",
                             std::string("attachement; filename=") + qph.getZipFileName());
      }

      response.setContent(*result);
    }
  }
  catch (...)
  {
    throw Fmi::Exception::Trace(BCP, "Operation failed!");
  }
}

void PluginImpl::grouplocations(Spine::HTTP::Request& request) const
{
  try
  {
    auto lonlats = request.getParameter("lonlats");
    if (!lonlats)
      lonlats = request.getParameter("lonlat");
    auto latlons = request.getParameter("latlons");
    if (!latlons)
      latlons = request.getParameter("latlon");
    auto places = request.getParameter("places");
    if (!places)
      places = request.getParameter("place");
    auto keyword = request.getParameter("keyword");
    auto fmisid = request.getParameter("fmisid");
    auto lpnn = request.getParameter("lpnn");
    auto wmo = request.getParameter("wmo");

    std::string wkt_multipoint = "MULTIPOINT(";
    parse_lonlats(lonlats, request, wkt_multipoint);
    parse_latlons(latlons, request, wkt_multipoint);
    if (places)
      request.removeParameter("place");
    parse_places(places, request, wkt_multipoint, itsEngines.geoEngine.get());

    // The locations of a keyword
    if (keyword)
    {
      request.removeParameter("keyword");
      Locus::QueryOptions opts;
      opts.SetLanguage(
          Spine::optional_string(request.getParameter("lang"), itsConfig.defaultLanguage()));
      for (const auto& loc : itsEngines.geoEngine->keywordSearch(opts, *keyword))
      {
        if (wkt_multipoint != "MULTIPOINT(")
          wkt_multipoint += ",";
        wkt_multipoint +=
            "(" + Fmi::to_string(loc->longitude) + " " + Fmi::to_string(loc->latitude) + ")";
      }
    }

    std::vector<int> fmisids;
    std::vector<int> lpnns;
    std::vector<int> wmos;

    parse_fmisids(fmisid, request, fmisids);
    parse_lpnns(lpnn, request, lpnns);
    parse_wmos(wmo, request, wmos);

    Engine::Geonames::LocationOptions lopts =
        itsEngines.geoEngine->parseLocations(fmisids, lpnns, wmos, "fi");
    for (const auto& lopt : lopts.locations())
    {
      if (lopt.loc)
      {
        if (wkt_multipoint != "MULTIPOINT(")
          wkt_multipoint += ",";
        wkt_multipoint += "(" + Fmi::to_string(lopt.loc->longitude) + " " +
                          Fmi::to_string(lopt.loc->latitude) + ")";
      }
    }

    if (wkt_multipoint == "MULTIPOINT(")
      throw Fmi::Exception(BCP,
                           "grouplocations needs locations: lonlat(s), latlon(s), place(s), "
                           "keyword, fmisid, lpnn or wmo");

    wkt_multipoint += ")";
    request.addParameter("wkt", wkt_multipoint);
  }
  catch (...)
  {
    throw Fmi::Exception::Trace(BCP, "Operation failed!");
  }
}

// ----------------------------------------------------------------------
/*!
 * \brief Perform a timeseries query
 */
// ----------------------------------------------------------------------

void PluginImpl::timeSeriesQuery(const State& state,
                                 const Spine::HTTP::Request& request,
                                 Spine::HTTP::Response& response)
{
  try
  {
    using std::chrono::duration_cast;
    using std::chrono::high_resolution_clock;
    using std::chrono::microseconds;

    Spine::Table data;

    high_resolution_clock::time_point t1 = high_resolution_clock::now();

    QueryProcessingHub qph(*this);

    TimeSeriesQuery tsq(state, request, itsConfig);

    // Resolve locations for FMISIDs, WMOs, LPNNs
    Engine::Geonames::LocationOptions lopt =
        itsEngines.geoEngine->parseLocations(tsq.fmisids, tsq.lpnns, tsq.wmos, tsq.language);
    const Spine::TaggedLocationList& locations = lopt.locations();
    Spine::TaggedLocationList tagged_ll = tsq.loptions->locations();
    tagged_ll.insert(tagged_ll.end(), locations.begin(), locations.end());
    tsq.loptions->setLocations(tagged_ll);

    check_limits(tsq.loptions->locations(), itsConfig.requestLimits());

    high_resolution_clock::time_point t2 = high_resolution_clock::now();
    data.setPaging(tsq.startrow, tsq.maxresults);

    std::string producer_option =
        Spine::optional_string(request.getParameter(PRODUCER_PARAM),
                               Spine::optional_string(request.getParameter(STATIONTYPE_PARAM), ""));
    boost::algorithm::to_lower(producer_option);

    // At least one of the location specifiers must be set

#ifndef WITHOUT_OBSERVATION
    if (tsq.fmisids.empty() && tsq.lpnns.empty() && tsq.wmos.empty() && tsq.boundingBox.empty() &&
        !UtilityFunctions::is_flash_or_mobile_producer(producer_option) &&
        tsq.loptions->locations().empty())
#else
    if (tsq.loptions->locations().empty())
#endif
      throw Fmi::Exception(BCP, "No location option given!").disableLogging();

    QueryServer::QueryStreamer_sptr queryStreamer;
    std::shared_ptr<Spine::TableFormatter> formatter(
        get_formatter_and_qstreamer(tsq, queryStreamer));

    std::string mime = formatter->mimetype() + "; charset=UTF-8";
    response.setHeader("Content-Type", mime);

    std::size_t product_hash = Fmi::bad_hash;
    high_resolution_clock::time_point t3 = high_resolution_clock::now();
    std::string timeheader =
        Fmi::to_string(duration_cast<microseconds>(t2 - t1).count()) + '+' +
        Fmi::to_string(duration_cast<microseconds>(t3 - t2).count());

    std::shared_ptr<std::string> obj =
        qph.processQuery(state, data, tsq, queryStreamer, product_hash);

    if (obj)
    {
      product_hash = Fmi::hash_value(*obj);
      if (etag_only(request, response, product_hash, "timeseries"))
        return;

      response.setHeader("X-Duration", timeheader);
      response.setHeader("X-EDR-Cache", "yes");
      response.setContent(obj);
      return;
    }

    response.setHeader("X-EDR-Cache", "no");

    high_resolution_clock::time_point t4 = high_resolution_clock::now();
    timeheader.append("+").append(
        Fmi::to_string(duration_cast<microseconds>(t4 - t3).count()));

    data.setMissingText(tsq.valueformatter.missing());
    Spine::TableFormatter::Names headers = get_headers(tsq.poptions.parameters());
    std::string wxml_type = get_wxml_type(producer_option, itsObsEngineStationTypes);
    auto formatter_options = itsConfig.formatterOptions();
    formatter_options.setFormatType(wxml_type);
    auto out = formatter->format(data, headers, request, formatter_options);

    high_resolution_clock::time_point t5 = high_resolution_clock::now();
    timeheader.append("+").append(
        Fmi::to_string(duration_cast<microseconds>(t5 - t4).count()));

    std::shared_ptr<std::string> result(new std::string());
    std::swap(out, *result);
    response.setHeader("X-Duration", timeheader);

    if (strcasecmp(tsq.format.c_str(), "FILE") == 0)
    {
      std::string filename =
          "attachement; filename=timeseries_" + std::to_string(getTime()) + ".grib";
      response.setHeader("Content-type", "application/octet-stream");
      response.setHeader("Content-Disposition", filename);
      response.setContent(queryStreamer);
    }
    else
    {
      product_hash = Fmi::hash_value(*result);
      if (etag_only(request, response, product_hash, "timeseries"))
        return;

      response.setContent(*result);
    }
  }
  catch (...)
  {
    throw Fmi::Exception::Trace(BCP, "Operation failed!");
  }
}

// ----------------------------------------------------------------------
/*!
 * \brief TimeSeries content handler
 */
// ----------------------------------------------------------------------

void PluginImpl::timeSeriesRequestHandler(Spine::Reactor& /* theReactor */,
                                          const Spine::HTTP::Request& theRequest,
                                          Spine::HTTP::Response& theResponse)
{
  bool isdebug = false;

  try
  {
    // The API definition and its documentation
    if (handleApiRequest(theRequest, theResponse))
      return;

    auto request = theRequest;

    if (Spine::optional_bool(request.getParameter("grouplocations"), false))
      grouplocations(request);

    isdebug = ("debug" == Spine::optional_string(request.getParameter("format"), ""));

    theResponse.setHeader("Access-Control-Allow-Origin", "*");

    auto expires_seconds = itsConfig.expirationTime();
    State state(*this);
    state.setPretty(Spine::optional_bool(request.getParameter("pretty"), itsConfig.getPretty()));

    theResponse.setStatus(Spine::HTTP::Status::ok);

    timeSeriesQuery(state, request, theResponse);

    std::shared_ptr<Fmi::TimeFormatter> tformat(Fmi::TimeFormatter::create("http"));

    if (expires_seconds == 0)
    {
      theResponse.setHeader("Cache-Control", "no-cache, must-revalidate");
      theResponse.setHeader("Pragma", "no-cache");
    }
    else
    {
      std::string cachecontrol = "public, max-age=" + Fmi::to_string(expires_seconds);
      theResponse.setHeader("Cache-Control", cachecontrol);

      Fmi::DateTime t_expires = state.getTime() + Fmi::Seconds(expires_seconds);
      std::string expiration = tformat->format(t_expires);
      theResponse.setHeader("Expires", expiration);
    }

    std::string modification = tformat->format(state.getTime());
    theResponse.setHeader("Last-Modified", modification);
  }
  catch (...)
  {
    Fmi::Exception ex(BCP, "Request processing exception!", nullptr);
    ex.addParameter("URI", theRequest.getURI());
    ex.addParameter("ClientIP", theRequest.getClientIP());
    ex.addParameter("HostName", Spine::HostInfo::getHostName(theRequest.getClientIP()));

    const bool check_token = true;
    auto apikey = Spine::FmiApiKey::getFmiApiKey(theRequest, check_token);
    ex.addParameter("Apikey", (apikey ? *apikey : std::string("-")));

    ex.printError();

    std::string firstMessage = ex.what();
    if (isdebug)
    {
      theResponse.setHeader("Content-Type", "text/html; charset=UTF-8");
      theResponse.setContent(ex.getHtmlStackTrace());
      theResponse.setStatus(Spine::HTTP::Status::ok);
    }
    else
    {
      if (firstMessage.find("timeout") != std::string::npos)
        theResponse.setStatus(Spine::HTTP::Status::request_timeout);
      else
        theResponse.setStatus(Spine::HTTP::Status::bad_request);

      // The message may repeat request parameters: it must never be taken for HTML
      theResponse.setHeader("Content-Type", "text/plain; charset=UTF-8");
      theResponse.setHeader("X-Content-Type-Options", "nosniff");
      theResponse.setContent(firstMessage);
    }

    // The message may contain request parameters, control characters (e.g. CR) must not
    // end up in the header
    std::replace_if(
        firstMessage.begin(),
        firstMessage.end(),
        [](char c) { return static_cast<unsigned char>(c) < 0x20 || c == 0x7f; },
        ' ');
    if (firstMessage.size() > 300)
      firstMessage.resize(300);
    theResponse.setHeader("X-TimeSeriesPlugin-Error", firstMessage);
  }
}

// ----------------------------------------------------------------------
/*!
 * \brief Main content handler
 */
// ----------------------------------------------------------------------

void PluginImpl::requestHandler(Spine::Reactor& /* theReactor */,
                                const Spine::HTTP::Request& theRequest,
                                Spine::HTTP::Response& theResponse)
{
  // We need this in the catch block
  bool isdebug = false;

  try
  {
    // The API definition and its documentation
    if (handleApiRequest(theRequest, theResponse))
      return;

    // The request will be modified when parsing the input
    auto request = theRequest;

    if (Spine::optional_bool(request.getParameter("grouplocations"), false))
      grouplocations(request);

    isdebug = ("debug" == Spine::optional_string(request.getParameter("format"), ""));

    theResponse.setHeader("Access-Control-Allow-Origin", "*");

    auto expires_seconds = itsConfig.expirationTime();
    State state(*this);
    state.setPretty(Spine::optional_bool(request.getParameter("pretty"), itsConfig.getPretty()));

    theResponse.setStatus(Spine::HTTP::Status::ok);

    // BRAINSTORM-3349
    //
    // Store custom dimension request parameter 'level' using other name since it's used
    // internally to store 'z' parameter's values

    auto opt = Spine::optional_string(request.getParameter("level"), "");
    if (! opt.empty())
    {
       request.addParameter("custom_level", opt);
       request.removeParameter("level");
    }

    query(state, request, theResponse);  // may modify the status

    // Adding response headers

    std::shared_ptr<Fmi::TimeFormatter> tformat(Fmi::TimeFormatter::create("http"));

    if (expires_seconds == 0)
    {
      theResponse.setHeader("Cache-Control", "no-cache, must-revalidate");  // HTTP/1.1
      theResponse.setHeader("Pragma", "no-cache");                          // HTTP/1.0
    }
    else
    {
      std::string cachecontrol = "public, max-age=" + Fmi::to_string(expires_seconds);
      theResponse.setHeader("Cache-Control", cachecontrol);

      Fmi::DateTime t_expires = state.getTime() + Fmi::Seconds(expires_seconds);
      std::string expiration = tformat->format(t_expires);
      theResponse.setHeader("Expires", expiration);
    }

    std::string modification = tformat->format(state.getTime());
    theResponse.setHeader("Last-Modified", modification);
  }
  catch (...)
  {
    // Catching all exceptions

    Fmi::Exception ex(BCP, "Request processing exception!", nullptr);
    ex.addParameter("URI", theRequest.getURI());
    ex.addParameter("ClientIP", theRequest.getClientIP());
    ex.addParameter("HostName", Spine::HostInfo::getHostName(theRequest.getClientIP()));

    const bool check_token = true;
    auto apikey = Spine::FmiApiKey::getFmiApiKey(theRequest, check_token);
    ex.addParameter("Apikey", (apikey ? *apikey : std::string("-")));

    ex.printError();

    std::string firstMessage = ex.what();
    if (isdebug)
    {
      // Delivering the exception information as HTTP content
      std::string fullMessage = ex.getHtmlStackTrace();
      theResponse.setHeader("Content-Type", "text/html; charset=UTF-8");
      theResponse.setContent(fullMessage);
      theResponse.setStatus(Spine::HTTP::Status::ok);
    }
    else
    {
      const int status = http_status(ex);
      theResponse.setStatus(static_cast<Spine::HTTP::Status>(status));

      // RFC 7807 problem details
      std::string problem_detail = PROBLEM_DETAIL_JSON;
      boost::algorithm::replace_all(problem_detail, "__STATUS__", Fmi::to_string(status));
      boost::algorithm::replace_all(problem_detail, "__DETAIL__", json_escape(firstMessage));
      theResponse.setContent(problem_detail);
      theResponse.setHeader("Content-Type", "application/problem+json");
    }

    // Adding the first exception information into the response header
    // The message may contain request parameters, control characters (e.g. CR) must not
    // end up in the header
    std::replace_if(
        firstMessage.begin(),
        firstMessage.end(),
        [](char c) { return static_cast<unsigned char>(c) < 0x20 || c == 0x7f; },
        ' ');
    if (firstMessage.size() > 300)
      firstMessage.resize(300);
    theResponse.setHeader("X-EDRPlugin-Error", firstMessage);
  }
}

// ----------------------------------------------------------------------
/*!
 * \brief PluginImpl constructor
 */
// ----------------------------------------------------------------------

PluginImpl::PluginImpl(Spine::Reactor* theReactor, const char* theConfigFile)
    : itsReactor(theReactor),
      itsConfigFile(theConfigFile),
      itsConfig(theConfigFile),
      itsConfigHash(Spine::config_hash(itsConfig.config()))
{
}

// ----------------------------------------------------------------------
/*!
 * \brief PluginImpl destructor - ensures metadata task is stopped
 */
// ----------------------------------------------------------------------

PluginImpl::~PluginImpl()
{
  try
  {
    if (itsMetaDataUpdateTask)
    {
      itsMetaDataUpdateTask->cancel();
      itsMetaDataUpdateTask->wait();
    }
  }
  catch (...)
  {
  }
}

// ----------------------------------------------------------------------
/*!
 * \brief Initialize in a separate thread due to PostGIS slowness
 */
// ----------------------------------------------------------------------

void PluginImpl::init()
{
  try
  {
    // Time series cache
    itsTimeSeriesCache.reset(new TS::TimeSeriesGeneratorCache);
    itsTimeSeriesCache->resize(itsConfig.maxTimeSeriesCacheSize());

    /* GeoEngine */
    itsEngines.geoEngine = itsReactor->getEngine<Engine::Geonames::Engine>("Geonames", nullptr);

    /* GisEngine */
    itsEngines.gisEngine = itsReactor->getEngine<Engine::Gis::Engine>("Gis", nullptr);

    // Read the geometries from PostGIS database
    itsEngines.gisEngine->populateGeometryStorage(itsConfig.getPostGISIdentifiers(),
                                                  itsGeometryStorage);

    /* QEngine */
    itsEngines.qEngine = itsReactor->getEngine<Engine::Querydata::Engine>("Querydata", nullptr);

    /* GridEngine */
    if (!itsConfig.gridEngineDisabled())
    {
      itsEngines.gridEngine = itsReactor->getEngine<Engine::Grid::Engine>("grid", nullptr);
      itsEngines.gridEngine->setDem(itsEngines.geoEngine->dem());
      itsEngines.gridEngine->setLandCover(itsEngines.geoEngine->landCover());
    }

#ifndef WITHOUT_OBSERVATION
    {
      /* ObsEngine - always load if available; observation_disabled only affects EDR routing */
      try
      {
        itsEngines.obsEngine =
            itsReactor->getEngine<Engine::Observation::Engine>("Observation", nullptr);
      }
      catch (...)
      {
        // Observation engine not loaded - continue without it
      }

      // fetch obsengine station types (producers)
      if (itsEngines.obsEngine != nullptr)
        itsObsEngineStationTypes = itsEngines.obsEngine->getValidStationTypes();
    }
#endif

#ifndef WITHOUT_AVI
    if (!itsConfig.aviEngineDisabled())
    {
      /* AviEngine */
      itsEngines.aviEngine = itsReactor->getEngine<Engine::Avi::Engine>("Avi", nullptr);
    }
#endif

    // Get locations
    updateSupportedLocations();
    // Get metadata
    updateMetaData(true);
    // Update metadata parameter info from config
    updateParameterInfo();

    // Generated OpenAPI definitions
    const auto& openapi = itsConfig.openAPISettings();
    if (openapi.enabled)
    {
      const auto& page = itsConfig.landingPage();
      itsEdrApi = std::make_unique<Spine::OpenAPI::Publisher>(openapi);
      itsEdrApi->addToCatalog(itsConfig.defaultUrl(), page.title, page.description, "edr");
      for (const auto& url : itsConfig.timeSeriesUrls())
      {
        auto publisher = std::make_unique<Spine::OpenAPI::Publisher>(openapi);
        publisher->addToCatalog(url,
                                ApiDefinition::TIMESERIES_TITLE,
                                ApiDefinition::TIMESERIES_DESCRIPTION,
                                "timeseries");
        itsTimeSeriesApis.emplace(url, std::move(publisher));
      }
      publishApiDefinitions();
    }

    if (Spine::Reactor::isShuttingDown())
      return;

    // Begin the metadata update loop
    if (!itsConfig.metaDataUpdatesDisabled())
    {
      itsMetaDataUpdateTask.reset(
          new Fmi::AsyncTask("upd-edr-meta", [this]() { metaDataUpdateLoop(); }));
    }

    itsReady = true;
  }
  catch (...)
  {
    throw Fmi::Exception::Trace(BCP, "Operation failed!");
  }
}

// ----------------------------------------------------------------------
/*!
 * \brief Shutdown - stops the metadata update task
 */
// ----------------------------------------------------------------------

void PluginImpl::shutdown()
{
  try
  {
    if (itsMetaDataUpdateTask)
    {
      itsMetaDataUpdateTask->cancel();
      itsMetaDataUpdateTask->wait();
    }
  }
  catch (...)
  {
    throw Fmi::Exception::Trace(BCP, "Operation failed!");
  }
}

// ----------------------------------------------------------------------
/*!
 * \brief Check if config has changed since this impl was created.
 *
 * Re-reads the config file and computes its content hash. This detects
 * changes in @include'd files and ignores whitespace-only edits.
 */
// ----------------------------------------------------------------------

bool PluginImpl::isReloadRequired() const
{
  try
  {
    libconfig::Config cfg;
    cfg.readFile(itsConfigFile);
    return Spine::config_hash(cfg) != itsConfigHash;
  }
  catch (...)
  {
    return false;
  }
}

void PluginImpl::metaDataUpdateLoop()
{
  try
  {
    while (!Spine::Reactor::isShuttingDown())
    {
      try
      {
        // update metadata every N seconds
        boost::this_thread::sleep_for(boost::chrono::seconds(itsConfig.metaDataUpdateInterval()));
        updateQueryDataLocations();
        updateMetaData(false);
      }
      catch (...)
      {
        Fmi::Exception exception(BCP, "Could not update capabilities!", nullptr);
        exception.printError();
      }
    }
  }
  catch (...)
  {
    Fmi::Exception exception(BCP, "Could not update metadata!", nullptr);
    exception.printError();
  }
}

void PluginImpl::updateMetaData(bool initial_phase)
{
  try
  {
    std::shared_ptr<EngineMetaData> engine_meta_data(std::make_shared<EngineMetaData>());

    const auto& producer_licenses = itsConfig.allProducerLicenses();
    const auto& default_language = itsConfig.defaultLanguage();
    const auto* parameter_info = &itsConfigParameterInfo;
    const auto& data_queries = itsConfig.allSupportedDataQueries();
    const auto& output_formats = itsConfig.allSupportedOutputFormats();
    const auto& default_formats = itsConfig.allDefaultOutputFormats();
    const auto& collection_info_container = itsConfig.getCollectionInfo();
    auto observation_period = itsConfig.getObservationPeriod();

    auto qengine_metadata = get_edr_metadata_qd(*itsEngines.qEngine,
                                                producer_licenses,
                                                default_language,
                                                parameter_info,
                                                collection_info_container,
                                                data_queries,
                                                output_formats,
                                                default_formats,
                                                itsSupportedLocations);
    engine_meta_data->addMetaData(SourceEngine::Querydata, qengine_metadata);
    if (!itsConfig.gridEngineDisabled())
    {
      auto grid_engine_metadata = get_edr_metadata_grid(*itsEngines.gridEngine,
                                                        producer_licenses,
                                                        default_language,
                                                        parameter_info,
                                                        collection_info_container,
                                                        data_queries,
                                                        output_formats,
                                                        default_formats,
                                                        itsSupportedLocations);

      engine_meta_data->addMetaData(SourceEngine::Grid, grid_engine_metadata);
    }
#ifndef WITHOUT_OBSERVATION
    if (!itsConfig.obsEngineDisabled())
    {
      if (initial_phase)
      {
        // std::shared_ptr<std::vector<ObservableProperty>>
        std::map<std::string, const Engine::Observation::ObservableProperty*> properties;
        std::vector<std::string> params;
        try
        {
          itsObservableProperties =
              itsEngines.obsEngine->observablePropertyQuery(params, default_language);
        }
        catch (...)
        {
          // The properties only describe the parameters, the collections work without them.
          // For example the SpatiaLite drivers have no measurand table.
          Fmi::Exception::Trace(BCP, "Failed to read the observable properties")
              .disableStackTrace()
              .printError();
          itsObservableProperties.reset();
        }
        // Some database drivers (e.g. SpatiaLite) do not implement observablePropertyQuery
        // and return a null pointer instead of an empty vector.
        if (itsObservableProperties)
          for (const auto& prop : *itsObservableProperties)
            itsObservablePropertiesMap[prop.gmlId] = &prop;
      }

      auto obs_engine_metadata = get_edr_metadata_obs(*itsEngines.obsEngine,
                                                      producer_licenses,
                                                      default_language,
                                                      parameter_info,
                                                      itsObservablePropertiesMap,
                                                      collection_info_container,
                                                      data_queries,
                                                      output_formats,
                                                      default_formats,
                                                      itsSupportedLocations,
                                                      itsConfig.getProducerParameters(),
                                                      observation_period);

      engine_meta_data->addMetaData(SourceEngine::Observation, obs_engine_metadata);
    }
#endif
#ifndef WITHOUT_AVI
    if (!itsConfig.aviEngineDisabled())
    {
      auto avi_engine_metadata = get_edr_metadata_avi(*itsEngines.aviEngine,
                                                      itsConfig,
                                                      producer_licenses,
                                                      default_language,
                                                      parameter_info,
                                                      collection_info_container,
                                                      data_queries,
                                                      output_formats,
                                                      default_formats,
                                                      itsSupportedLocations);
      engine_meta_data->addMetaData(SourceEngine::Avi, avi_engine_metadata);
    }
#endif
    engine_meta_data->removeDuplicates(initial_phase);

    //	checkNewDataAndNotify(engine_meta_data);

    itsMetaData.store(engine_meta_data);

    // The API definitions list the collections, parameters and instances. The initial
    // definitions are published by init() once the parameter info is available.
    if (!initial_phase)
      publishApiDefinitions();
  }
  catch (...)
  {
    throw Fmi::Exception::Trace(BCP, "Operation failed!");
  }
}

// ----------------------------------------------------------------------
/*!
 * \brief Regenerate the OpenAPI definitions from the current metadata
 *
 * A failure is logged and the previous definitions stay in use.
 */
// ----------------------------------------------------------------------

void PluginImpl::publishApiDefinitions() const
{
  try
  {
    auto metadata = itsMetaData.load();
    if (!metadata)
      return;

    if (itsEdrApi)
      itsEdrApi->publish(ApiDefinition::edr(itsConfig, *metadata));

    for (const auto& item : itsTimeSeriesApis)
      item.second->publish(ApiDefinition::timeseries(itsConfig, *metadata, item.first));

    if (!itsTimeSeriesApis.empty())
    {
      ::Json::StreamWriterBuilder builder;
      builder["indentation"] = "";
      const auto store = [&builder](ProducerDocument& doc, const ::Json::Value& json)
      {
        doc.json = ::Json::writeString(builder, json);
        doc.etag = fmt::format("\"{:x}-producers\"", Fmi::hash_value(doc.json));
      };
      auto docs = std::make_shared<ProducerDocuments>();
      const auto catalog = ProducerCatalog::build(*metadata);
      store((*docs)[""], catalog);
      (*docs)[""].catalog = catalog;
      for (const auto& source : ProducerCatalog::sources())
        store((*docs)[source], ProducerCatalog::filter(catalog, source));
      itsProducerDocuments.store(docs);
    }
  }
  catch (...)
  {
    Fmi::Exception::Trace(BCP, "Failed to generate the OpenAPI definitions").printError();
  }
}

// ----------------------------------------------------------------------
/*!
 * \brief Answer <root>/api, <root>/api/docs and the documentation files
 *
 * Returns false if the request is not for an API document.
 */
// ----------------------------------------------------------------------

bool PluginImpl::handleApiRequest(const Spine::HTTP::Request& theRequest,
                                  Spine::HTTP::Response& theResponse) const
{
  try
  {
    const auto& resource = theRequest.getResource();
    const auto& root = itsConfig.defaultUrl();

    if (itsEdrApi && Spine::OpenAPI::Publisher::isApiResource(resource, root))
      return itsEdrApi->handle(
          theRequest, theResponse, root, Spine::OpenAPI::serverUrl(theRequest, root));

    for (const auto& item : itsTimeSeriesApis)
    {
      const auto producers = item.first + "/api/producers";
      if (resource == producers)
      {
        handleProducers(theRequest, theResponse);
        return true;
      }
      if (resource.size() > producers.size() + 1 &&
          resource.compare(0, producers.size() + 1, producers + "/") == 0)
      {
        handleProducer(theRequest, theResponse, resource.substr(producers.size() + 1));
        return true;
      }
      if (Spine::OpenAPI::Publisher::isApiResource(resource, item.first))
      {
        // The paths of the TimeSeries definition are absolute
        auto server = Spine::OpenAPI::serverUrl(theRequest, "");
        if (server.empty())
          server = "/";
        return item.second->handle(theRequest, theResponse, item.first, server);
      }
    }
    return false;
  }
  catch (...)
  {
    throw Fmi::Exception::Trace(BCP, "Operation failed!");
  }
}

// ----------------------------------------------------------------------
/*!
 * \brief Answer <timeseries_url>/api/producers
 */
// ----------------------------------------------------------------------

void PluginImpl::handleProducers(const Spine::HTTP::Request& theRequest,
                                 Spine::HTTP::Response& theResponse) const
{
  try
  {
    auto docs = itsProducerDocuments.load();
    theResponse.setHeader("Access-Control-Allow-Origin", "*");

    const auto source = Spine::optional_string(theRequest.getParameter("source"), "");
    const ProducerDocument* doc = nullptr;
    if (docs)
    {
      auto pos = docs->find(source);
      if (pos == docs->end())
      {
        theResponse.setStatus(Spine::HTTP::Status::bad_request);
        theResponse.setHeader("Content-Type", "application/problem+json");
        theResponse.setContent(
            R"({"title":"Bad Request","status":400,"detail":"source must be querydata, grid or observation"})");
        return;
      }
      doc = &pos->second;
    }
    if (doc == nullptr)
    {
      theResponse.setStatus(Spine::HTTP::Status::service_unavailable);
      theResponse.setHeader("Content-Type", "application/problem+json");
      theResponse.setContent(
          R"({"title":"Service Unavailable","status":503,"detail":"The metadata is not available yet"})");
      return;
    }

    theResponse.setHeader("Cache-Control", "public, max-age=60");
    theResponse.setHeader("ETag", doc->etag);
    if (theRequest.getHeader("X-Request-ETag"))
    {
      theResponse.setStatus(Spine::HTTP::Status::no_content);
      return;
    }
    if (auto status = Spine::HTTP::conditionalResponseStatus(theRequest, doc->etag))
    {
      theResponse.setStatus(*status);
      return;
    }
    theResponse.setStatus(Spine::HTTP::Status::ok);
    theResponse.setHeader("Content-Type", "application/json");
    theResponse.setContent(doc->json);
  }
  catch (...)
  {
    throw Fmi::Exception::Trace(BCP, "Operation failed!");
  }
}

// ----------------------------------------------------------------------
/*!
 * \brief Answer <timeseries_url>/api/producers/{name}
 *
 * The producer as listed, and for grid producers the model runs and what
 * the parameter keys of the chosen run may contain.
 */
// ----------------------------------------------------------------------

void PluginImpl::handleProducer(const Spine::HTTP::Request& theRequest,
                                Spine::HTTP::Response& theResponse,
                                const std::string& name) const
{
  const auto problem = [&theResponse](Spine::HTTP::Status status, const std::string& detail)
  {
    ::Json::Value json;
    json["title"] = (status == Spine::HTTP::Status::not_found ? "Not Found" : "Bad Request");
    json["status"] = static_cast<int>(status);
    json["detail"] = detail;
    ::Json::StreamWriterBuilder builder;
    builder["indentation"] = "";
    theResponse.setStatus(status);
    theResponse.setHeader("Content-Type", "application/problem+json");
    theResponse.setContent(::Json::writeString(builder, json));
  };

  try
  {
    theResponse.setHeader("Access-Control-Allow-Origin", "*");
    auto docs = itsProducerDocuments.load();
    if (!docs || docs->find("") == docs->end())
    {
      theResponse.setStatus(Spine::HTTP::Status::service_unavailable);
      theResponse.setHeader("Content-Type", "application/problem+json");
      theResponse.setContent(
          R"({"title":"Service Unavailable","status":503,"detail":"The metadata is not available yet"})");
      return;
    }

    // Without a source the first one listed, as in a request without one
    const auto source = Spine::optional_string(theRequest.getParameter("source"), "");
    const auto& catalog = docs->at("").catalog;
    const ::Json::Value* entry = nullptr;
    for (const auto& p : catalog["producers"])
      if (p["name"].asString() == Fmi::ascii_tolower_copy(name) &&
          (source.empty() || p["source"].asString() == source))
      {
        entry = &p;
        break;
      }
    if (entry == nullptr)
    {
      problem(Spine::HTTP::Status::not_found,
              "Unknown producer " + name + (source.empty() ? "" : " of source " + source));
      return;
    }

    ::Json::Value result = *entry;
    result["parameters"] = ::Json::Value(::Json::objectValue);
    for (const auto& p : (*entry)["parameters"])
    {
      const auto& info = catalog["parameters"][p.asString()];
      result["parameters"][p.asString()] =
          (info.isObject() ? info : ::Json::Value(::Json::objectValue));
    }

    // The locations of the producer, such as observation stations, from the metadata
    if ((*entry)["source"].asString() != "grid")
    {
      if (auto metadata = itsMetaData.load())
      {
        for (const auto& engine_item : metadata->getMetaData())
        {
          const auto& source = (*entry)["source"].asString();
          const bool same_source =
              (engine_item.first == SourceEngine::Querydata && source == "querydata") ||
              (engine_item.first == SourceEngine::Observation && source == "observation");
          if (!same_source)
            continue;
          auto pos = engine_item.second.find((*entry)["name"].asString());
          if (pos == engine_item.second.end())
            continue;
          for (const auto& md : pos->second)
          {
            if (!md.locations || md.locations->empty())
              continue;
            auto& locations = result["locations"];
            locations = ::Json::Value(::Json::arrayValue);
            for (const auto& item : *md.locations)
            {
              const auto& loc = item.second;
              ::Json::Value l;
              l["id"] = loc.id;
              l["type"] = loc.type;
              l["name"] = loc.name;
              l["lon"] = loc.longitude;
              l["lat"] = loc.latitude;
              if (!loc.start_time.is_not_a_date_time())
                l["start"] = Fmi::date_time::to_iso_extended_string(loc.start_time) + "Z";
              if (!loc.end_time.is_not_a_date_time())
                l["end"] = Fmi::date_time::to_iso_extended_string(loc.end_time) + "Z";
              if (!loc.wsi.empty())
                l["wsi"] = loc.wsi;
              locations.append(l);
            }
            break;
          }
        }
      }
    }

#ifndef WITHOUT_OBSERVATION
    // The sensors of observation stations: which sensors each parameter has at a station, which
    // one is the default, and their periods
    const auto fmisids = Spine::optional_string(theRequest.getParameter("fmisid"), "");
    if ((*entry)["source"].asString() == "observation" && !fmisids.empty() && itsEngines.obsEngine)
    {
      Engine::Observation::SensorOptions options;
      options.producer = (*entry)["name"].asString();
      std::vector<std::string> parts;
      boost::algorithm::split(parts, fmisids, boost::algorithm::is_any_of(","));
      for (const auto& p : parts)
        if (auto id = Fmi::stoi_opt(p))
          options.fmisids.insert(*id);
      const auto params = Spine::optional_string(theRequest.getParameter("param"), "");
      if (!params.empty())
      {
        boost::algorithm::split(parts, params, boost::algorithm::is_any_of(","));
        for (const auto& p : parts)
          if (!p.empty())
            options.parameters.insert(p);
      }
      const auto starttime = Spine::optional_string(theRequest.getParameter("starttime"), "");
      const auto endtime = Spine::optional_string(theRequest.getParameter("endtime"), "");
      try
      {
        if (!starttime.empty())
          options.starttime = Fmi::TimeParser::parse(starttime);
        if (!endtime.empty())
          options.endtime = Fmi::TimeParser::parse(endtime);
        const auto found = itsEngines.obsEngine->getSensors(options);
        const auto iso = [](const Fmi::DateTime& t)
        { return Fmi::date_time::to_iso_extended_string(t) + "Z"; };
        ::Json::Value sensors(::Json::objectValue);
        sensors["available"] = found.available;
        sensors["note"] = found.note;
        sensors["list"] = ::Json::Value(::Json::arrayValue);
        for (const auto& s : found.sensors)
        {
          ::Json::Value item;
          item["fmisid"] = s.fmisid;
          item["measurand_id"] = s.measurand_id;
          if (!s.measurand_code.empty())
            item["code"] = s.measurand_code;
          item["parameters"] = ::Json::Value(::Json::arrayValue);
          for (const auto& name : s.parameters)
            item["parameters"].append(name);
          item["sensor"] = s.sensor_no;
          item["default"] = s.is_default;
          if (!s.configuration_start.is_not_a_date_time())
            item["configuration_start"] = iso(s.configuration_start);
          if (!s.configuration_end.is_not_a_date_time())
            item["configuration_end"] = iso(s.configuration_end);
          if (!s.first_data.is_not_a_date_time())
            item["first"] = iso(s.first_data);
          if (!s.last_data.is_not_a_date_time())
            item["last"] = iso(s.last_data);
          if (s.values > 0)
            item["values"] = static_cast<::Json::UInt64>(s.values);
          sensors["list"].append(item);
        }
        result["sensors"] = sensors;
      }
      catch (...)
      {
        // The first reason, e.g. an unknown parameter of the producer
        Fmi::Exception e = Fmi::Exception::Trace(BCP, "Listing the sensors failed");
        const auto* first = e.getFirstException();
        problem(Spine::HTTP::Status::bad_request, first ? first->getWhat() : e.getWhat());
        return;
      }
    }
#endif

    if ((*entry)["source"].asString() == "grid" && itsEngines.gridEngine)
    {
      const auto origintime = Spine::optional_string(theRequest.getParameter("origintime"), "");
      ::Json::Value grid;
      try
      {
        grid =
            ProducerDetails::grid(*itsEngines.gridEngine, *itsGridDetailsCache, name, origintime);
      }
      catch (...)
      {
        problem(Spine::HTTP::Status::not_found,
                "No model run " + origintime + " of grid producer " + name);
        return;
      }
      result["producer"] = grid["producer"];
      result["runs"] = grid["runs"];
      if (grid.isMember("run"))
      {
        // The descriptions of the listing, what the keys may contain from the run
        for (const auto& key : grid["run"]["parameters"].getMemberNames())
        {
          auto& p = grid["run"]["parameters"][key];
          for (const auto& field : result["parameters"][key].getMemberNames())
            if (!p.isMember(field))
              p[field] = result["parameters"][key][field];
        }
        result["parameters"] = grid["run"]["parameters"];
        grid["run"].removeMember("parameters");
        result["run"] = grid["run"];
      }
    }

    ::Json::StreamWriterBuilder builder;
    builder["indentation"] = "";
    const auto json = ::Json::writeString(builder, result);
    const auto etag = fmt::format("\"{:x}-producer\"", Fmi::hash_value(json));
    theResponse.setHeader("Cache-Control", "public, max-age=60");
    theResponse.setHeader("ETag", etag);
    if (theRequest.getHeader("X-Request-ETag"))
    {
      theResponse.setStatus(Spine::HTTP::Status::no_content);
      return;
    }
    if (auto status = Spine::HTTP::conditionalResponseStatus(theRequest, etag))
    {
      theResponse.setStatus(*status);
      return;
    }
    theResponse.setStatus(Spine::HTTP::Status::ok);
    theResponse.setHeader("Content-Type", "application/json");
    theResponse.setContent(json);
  }
  catch (...)
  {
    throw Fmi::Exception::Trace(BCP, "Operation failed!");
  }
}

std::map<std::string, Fmi::DateTime> PluginImpl::getNotificationTimes(
    SourceEngine source_engine,
    EngineMetaData& new_emd,
    const Fmi::DateTime& now) const
{
  try
  {
    std::map<std::string, Fmi::DateTime> times;

    const auto& new_md = new_emd.getMetaData(source_engine);

    for (const auto& producer_md_item : new_md)
    {
      const auto& producer = producer_md_item.first;
      const auto& producer_new_md = producer_md_item.second;

      auto old_latest_data_update_time = getProducerMetaData(producer).latest_data_update_time;

      if (old_latest_data_update_time.is_not_a_date_time())
      {
        // If old latest data update time does not exist -> set it to now and continue and don't
        // notify
        new_emd.setLatestDataUpdateTime(source_engine, producer, now);
      }
      else
      {
        std::optional<Fmi::DateTime> new_latest_data_update_time;

        switch (source_engine)
        {
          case SourceEngine::Observation:
          {
            // Get the latest update time since old_latest_data_update_time
            new_latest_data_update_time = itsEngines.obsEngine->getLatestDataUpdateTime(
                producer, old_latest_data_update_time);
            break;
          }
          case SourceEngine::Querydata:
          case SourceEngine::Grid:
          case SourceEngine::Avi:
          {
            if (!producer_new_md.empty())
              new_latest_data_update_time = producer_new_md.front().latest_data_update_time;
            break;
          }
          case SourceEngine::Undefined:
            break;
        }

        if (new_latest_data_update_time)
        {
          if (new_latest_data_update_time->is_not_a_date_time())
          {
            // If no timestamp received -> set latest data update time to now and do not notify
            new_latest_data_update_time = now;
          }
          else if (new_latest_data_update_time > old_latest_data_update_time)
          {
            // Set notification time, but it must not be in the future
            times[producer] =
                (new_latest_data_update_time <= now ? *new_latest_data_update_time : now);
          }
          new_emd.setLatestDataUpdateTime(source_engine, producer, *new_latest_data_update_time);
        }
      }
    }
    return times;
  }
  catch (...)
  {
    throw Fmi::Exception::Trace(BCP, "Operation failed!");
  }
}

void PluginImpl::checkNewDataAndNotify(const std::shared_ptr<EngineMetaData>& new_emd) const
{
  try
  {
    /*
      1) querydata: check origintimes
      2) grid-engine: check origintimes (analysisTime)
      3) avi-engine: endtime of avi engine metadata temporal extet is the latest data_time
      4) observation-engine: get latest data_time of each producer, either from cache or from
      database
    */

    std::map<SourceEngine, std::map<std::string, Fmi::DateTime>>
        times_to_notify;  // engine -> producer -> latest update time
    const std::array<SourceEngine, 4> source_engines{
        SourceEngine::Querydata, SourceEngine::Grid, SourceEngine::Observation, SourceEngine::Avi};

    auto now = Fmi::SecondClock::universal_time();

    for (auto source_engine : source_engines)
    {
      auto notification_times = getNotificationTimes(source_engine, *new_emd, now);

      times_to_notify[source_engine] = notification_times;
    }

    /*
    if(times_to_notify.empty())
      std::cout << "Nothing to notify\n";

    for(const auto& item : times_to_notify)
      {
            std::cout << "Notifications of " << get_engine_name(item.first) << std::endl;
            if(item.second.empty())
              {
                    std::cout << "- none" << std::endl;
              }
            else
              {
                    for(const auto& item2 : item.second)
                      std::cout << "- "  << item2.first << " -> " << item2.second << std::endl;
              }
      }
    */
  }
  catch (...)
  {
    throw Fmi::Exception::Trace(BCP, "Operation failed!");
  }
}

void PluginImpl::updateSupportedLocations()
{
  try
  {
    std::set<std::string> obs_producers;
#ifndef WITHOUT_OBSERVATION
    if (!itsConfig.obsEngineDisabled())
    {
      obs_producers = itsEngines.obsEngine->getValidStationTypes();
      for (const auto& producer : obs_producers)
      {
        Engine::Observation::Settings settings;
        settings.stationtype = producer;
        settings.allplaces = true;
        Spine::Stations stations;
        try
        {
          itsEngines.obsEngine->getStations(stations, settings);
        }
        catch (...)
        {
          // A station type without a working database driver must not prevent the
          // locations of the other producers from being listed
          Fmi::Exception::Trace(BCP, "Failed to read the stations of producer " + producer)
              .disableStackTrace()
              .printError();
          continue;
        }
        SupportedLocations sls;
        for (const auto& station : stations)
        {
          if (station.fmisid == 0 || station.geoid == 0)
            continue;
          location_info li(station);
          sls[li.id] = li;
        }
        if (!sls.empty())
          itsSupportedLocations[producer] =
              std::make_shared<const SupportedLocations>(std::move(sls));
      }
    }
#endif

    // Get locations directly embedded in nongrid (point) querydata; these take precedence
    // over the generic keyword-search fallback below for the producers they cover. The
    // metadata update loop refreshes them, see updateQueryDataLocations().
    auto qd_native_location_producers = load_locations_qd(*itsEngines.qEngine, itsSupportedLocations);

    // Get locations using keywords
    auto producer_keywords = itsConfig.getProducerKeywords();
    Locus::QueryOptions opts;
    opts.SetLanguage(itsConfig.defaultLanguage());
    for (const auto& item : producer_keywords)
    {
      auto producer = item.first;
      // Use keywords except for observation producers and producers that already got
      // their real station list from nongrid querydata above.
      if (obs_producers.find(producer) != obs_producers.end())
        continue;
      if (qd_native_location_producers.find(producer) != qd_native_location_producers.end())
        continue;
      Spine::LocationList producer_llist;
      SupportedLocations sls;
      for (const auto& keyword : item.second)
      {
        auto llist = itsEngines.geoEngine->keywordSearch(opts, keyword);
        for (const auto& loc : llist)
        {
          location_info li(loc, keyword);
          sls[li.id] = li;
        }
      }
      if (!sls.empty())
        itsSupportedLocations[producer] =
            std::make_shared<const SupportedLocations>(std::move(sls));
    }

    // For avi producers locations/stations are loaded from avidb
#ifndef WITHOUT_AVI
    if (!itsConfig.aviEngineDisabled())
    {
      const auto& collection_info_container = itsConfig.getCollectionInfo();
      load_locations_avi(*itsEngines.aviEngine,
                         itsConfig.getAviCollections(),
                         itsSupportedLocations,
                         collection_info_container);
    }
#endif
  }
  catch (...)
  {
    throw Fmi::Exception::Trace(BCP, "Operation failed!");
  }
}

// ----------------------------------------------------------------------
/*!
 * \brief Refresh the locations of nongrid (point) querydata producers
 *
 * The station set of point querydata may change while the server is running.
 * Called by the metadata update loop before the metadata is rebuilt. The old
 * location lists stay alive for as long as the published metadata uses them.
 */
// ----------------------------------------------------------------------

void PluginImpl::updateQueryDataLocations()
{
  try
  {
    SupportedProducerLocations qd_locations;
    for (const auto& producer : load_locations_qd(*itsEngines.qEngine, qd_locations))
      itsSupportedLocations[producer] = qd_locations[producer];
  }
  catch (...)
  {
    throw Fmi::Exception::Trace(BCP, "Operation failed!");
  }
}

void PluginImpl::updateParameterInfo()
{
  try
  {
    auto metadata = itsMetaData.load();

    std::set<std::string> parameter_names = get_metadata_parameters(metadata);
    bool requireMetOceanCustomDimReferences = false;

    const auto& lconfig = itsConfig.config();

    // Metadata parameter names are lowercase, but the settings may use any case
    // (e.g. RadiationLw), and libconfig lookups are case sensitive

    std::map<std::string, std::string> config_parameter_names;  // lowercase -> setting name
    if (lconfig.exists("parameter_info"))
    {
      const auto& settings = lconfig.lookup("parameter_info");
      if (!settings.isGroup())
        throw Fmi::Exception(BCP, "Configuration file error. parameter_info must be an object");

      for (int i = 0; i < settings.getLength(); i++)
      {
        std::string name = settings[i].getName();
        if (!config_parameter_names.emplace(Fmi::ascii_tolower_copy(name), name).second)
          throw Fmi::Exception(BCP,
                               "Configuration file error. parameter_info has several settings "
                               "for parameter " +
                                   name);
      }
    }

    for (const auto& pname : parameter_names)
    {
      if (pname.empty())
        continue;

      parameter_info_config pinfo;

      auto config_name = config_parameter_names.find(pname);
      auto pname_key =
          ("parameter_info." +
           (config_name != config_parameter_names.end() ? config_name->second : pname));
      if (lconfig.exists(pname_key))
      {
        // Description
        auto desc_key = (pname_key + ".description");
        if (lconfig.exists(desc_key))
        {
          auto& setting = lconfig.lookup(desc_key);
          int len = setting.getLength();
          for (int i = 0; i < len; i++)
          {
            const auto* name = setting[i].getName();
            std::string name_key = desc_key + "." + name;
            std::string value;
            lconfig.lookupValue(name_key, value);
            if (value.empty())
              throw Fmi::Exception(BCP, name_key + " setting is required");
            pinfo.description[Fmi::ascii_tolower_copy(name)] = value;
          }
        }
        // Label
        std::string label;
        auto label_key = (pname_key + ".label");
        if (lconfig.exists(label_key))
          lconfig.lookupValue(label_key, label);
        if (! label.empty())
          pinfo.label = label;
        // ObservedProperty. Using 'label' as the default english label
        auto obsprop_key = (pname_key + ".observed_property");
        if (lconfig.exists(obsprop_key))
        {
          auto& setting = lconfig.lookup(obsprop_key);
          if (! setting.isGroup())
            throw Fmi::Exception(
                BCP, "Configuration file error. " + obsprop_key + " must be an object");
          auto obsprop_label_key = (obsprop_key + ".label");
          if (lconfig.exists(obsprop_label_key))
          {
            auto& setting = lconfig.lookup(obsprop_label_key);
            int len = setting.getLength();
            for (int i = 0; i < len; i++)
            {
              const auto* name = setting[i].getName();
              std::string name_key = obsprop_label_key + "." + name;
              std::string value;
              lconfig.lookupValue(name_key, value);
              if (value.empty())
                throw Fmi::Exception(BCP, name_key + " setting is required");
              pinfo.observed_property_label[Fmi::ascii_tolower_copy(name)] = value;
            }
          }
        }
        if (
            (! pinfo.label.empty()) &&
            (pinfo.observed_property_label.find("en") == pinfo.observed_property_label.end())
           )
          pinfo.observed_property_label["en"] = pinfo.label;
        // Unit
        auto unit_label_key = (pname_key + ".unit.label");
        if (lconfig.exists(unit_label_key))
        {
          auto& setting = lconfig.lookup(unit_label_key);
          int len = setting.getLength();
          for (int i = 0; i < len; i++)
          {
            const auto* name = setting[i].getName();
            std::string name_key = unit_label_key + "." + name;
            std::string value;
            lconfig.lookupValue(name_key, value);
            if (value.empty())
              throw Fmi::Exception(BCP, name_key + " setting is required");
            pinfo.unit_label[Fmi::ascii_tolower_copy(name)] = value;
          }
        }
        if (pinfo.unit_label.empty())
          throw Fmi::Exception(BCP, unit_label_key + " setting is required");
        lconfig.lookupValue(pname_key + ".unit.symbol.value", pinfo.unit_symbol_value);
        lconfig.lookupValue(pname_key + ".unit.symbol.type", pinfo.unit_symbol_type);
        lconfig.lookupValue(pname_key + ".unit.id", pinfo.unit_id);
        if (pinfo.unit_symbol_value.empty())
          throw Fmi::Exception(BCP, pname_key + " unit.symbol.value setting is required");
        // metocean; standard name
        lconfig.lookupValue(pname_key + ".standard_name", pinfo.metocean.standard_name);
        // metocean; level
        auto level_key = (pname_key + ".level");
        if (lconfig.exists(level_key))
          lconfig.lookupValue(level_key, pinfo.metocean.level);
        else
          level_key.clear();
        // metocean; measurement type
        auto meastype_key = (pname_key + ".measurement_type");
        if (lconfig.exists(meastype_key))
        {
          auto& setting = lconfig.lookup(meastype_key);
          if (! setting.isGroup())
            throw Fmi::Exception(
                BCP, "Configuration file error. " + meastype_key + " must be an object");
          auto method_key = meastype_key + ".method";
          if (! lconfig.exists(method_key))
            throw Fmi::Exception(BCP, method_key + " setting is required");
          lconfig.lookupValue(method_key, pinfo.metocean.method);
          auto duration_key = meastype_key + ".duration";
          if (! lconfig.exists(duration_key))
            throw Fmi::Exception(BCP, duration_key + " setting is required");
          lconfig.lookupValue(duration_key, pinfo.metocean.duration);
        }

        // If standard_name, method, duration or level is given, require all of them
        // for MetOcean profile metadata output

        if (
            (
             pinfo.metocean.standard_name.empty() &&
             (! (pinfo.metocean.method.empty() && pinfo.metocean.duration.empty()))
            ) ||
            (pinfo.metocean.standard_name.empty() != level_key.empty())
           )
          throw Fmi::Exception(BCP, pname_key + " standard_name and level settings are required");
        if ((!pinfo.metocean.standard_name.empty()) &&
            (pinfo.metocean.method.empty() || pinfo.metocean.duration.empty()))
          throw Fmi::Exception(BCP, meastype_key + " method and duration settings are required");
      }

      if (! pinfo.metocean.standard_name.empty())
      {
        boost::algorithm::to_lower(pinfo.metocean.standard_name);
        boost::algorithm::to_lower(pinfo.metocean.method);
        boost::algorithm::to_lower(pinfo.metocean.duration);

        requireMetOceanCustomDimReferences = true;
      }

      itsConfigParameterInfo[pname] = pinfo;
    }

    // MetOcean related custom dimension references are required only if any standard_name
    // is configured for parameters

    for (const auto& dim_ref : itsConfig.getCustomDimReferences())
    {
      auto ref_key = ("custom_dimensions.references." + dim_ref.first);
      bool requireSetting = (requireMetOceanCustomDimReferences && dim_ref.second.empty());

      if (! lconfig.exists(ref_key))
      {
        if (requireSetting)
          throw Fmi::Exception(BCP, "Missing custom dimension reference '" + ref_key + "'");

        continue;
      }

      std::string reference;
      lconfig.lookupValue(ref_key, reference);

      itsConfig.setCustomDimReference(dim_ref.first, reference);
    }
  }
  catch (...)
  {
    throw Fmi::Exception::Trace(BCP, "Operation failed!");
  }
}

EDRMetaData PluginImpl::getProducerMetaData(const std::string& producer) const
{
  auto metadata = itsMetaData.load();
  EDRMetaData empty_emd;

  if (!metadata)
    return empty_emd;

  for (const auto& item : metadata->getMetaData())
  {
    const auto& engine_metadata = item.second;
    auto it = engine_metadata.find(producer);
    if (it != engine_metadata.end() && !it->second.empty())
      return it->second.front();
  }

  return empty_emd;
}

#ifndef WITHOUT_AVI
const EDRProducerMetaData& PluginImpl::getAviMetaData() const
{
  auto metadata = itsMetaData.load();

  return metadata->getMetaData(SourceEngine::Avi);
}
#endif

bool PluginImpl::isValidCollection(const std::string& producer) const
{
  auto metadata = itsMetaData.load();

  if (!metadata)
    return false;

  return metadata->isValidCollection(producer);
}

bool PluginImpl::isValidInstance(const std::string& producer, const std::string& instanceId) const
{
  auto metadata = itsMetaData.load();

  if (!metadata)
    return false;

  for (const auto& item : metadata->getMetaData())
  {
    const auto& engine_metadata = item.second;
    const auto epmd = engine_metadata.find(producer);

    if (epmd != engine_metadata.end())
    {
      for (auto const& md : epmd->second)
        if (Fmi::date_time::to_iso_extended_string(md.temporal_extent.origin_time) + "Z" ==
            instanceId)
          return true;
    }
  }

  return false;
}

const Fmi::TimeZones& PluginImpl::getTimeZones() const
{
  return itsEngines.geoEngine->getTimeZones();
}

Fmi::Cache::CacheStatistics PluginImpl::getCacheStats() const
{
  Fmi::Cache::CacheStatistics ret;

  ret.insert(std::make_pair("Timeseries::timeseries_generator_cache",
                            itsTimeSeriesCache->getCacheStats()));

  return ret;
}

std::unique_ptr<Spine::Table> PluginImpl::getEDRInternalInformation(
  Spine::Reactor& reactor,
  const Spine::HTTP::Request& request) const
{
  std::map<std::string,
           std::pair<std::function<std::unique_ptr<Spine::Table>(
                         Spine::Reactor & reactor, const Spine::HTTP::Request& request)>,
                     std::string>>
      info = {{"geometrystorage",
               {[this](Spine::Reactor& /*reactor*/, const Spine::HTTP::Request& /*request*/)
                { return itsGeometryStorage.dumpContents(); },
                "Contents of the geometry storage"}}};

  const std::string request_param = Fmi::ascii_tolower_copy(
    Spine::optional_string(request.getParameter("request"), "?"));

  const auto ir = info.find(request_param);
  if (ir != info.end())
  {
    try
    {
      return ir->second.first(reactor, request);
    }
    catch (...)
    {
      throw Fmi::Exception::Trace(BCP, "Failed to get internal information for request '" + request_param + "'");
    }
  }

  std::unique_ptr<Spine::Table> result = std::make_unique<Spine::Table>();
  result->setTitle("EDR plugin internal information (request=...)");
  result->setNames({"name", "description"});
  int row = 0;
  for (const auto& item : info)
  {
    result->set(row, 0, item.first);
    result->set(row, 1, item.second.second);
    row++;
  }
  return result;
}

}  // namespace EDR
}  // namespace Plugin
}  // namespace SmartMet
