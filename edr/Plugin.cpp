// ======================================================================
/*!
 * \brief SmartMet EDR plugin implementation
 */
// ======================================================================

#include "Plugin.h"
#include "QueryProcessingHub.h"
#include "State.h"
#include "UtilityFunctions.h"
#include <engines/gis/Engine.h>
#include <fmt/format.h>
#include <macgyver/Hash.h>
#include <macgyver/TimeParser.h>
#include <spine/Convenience.h>
#include <spine/FmiApiKey.h>
#include <spine/HostInfo.h>
#include <spine/ImageFormatter.h>
#include <spine/TableFormatterFactory.h>
#include <timeseries/ParameterKeywords.h>

// #define MYDEBUG ON

namespace SmartMet
{
namespace Plugin
{
namespace EDR
{
namespace
{

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

Spine::TableFormatter* get_formatter_and_qstreamer(const Query& q,
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
        headers.push_back(param_header);
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

bool etag_only(const Spine::HTTP::Request& request,
               Spine::HTTP::Response& response,
               std::size_t product_hash)
{
  try
  {
    if (product_hash != Fmi::bad_hash)
    {
      response.setHeader("ETag", fmt::format("\"{:x}-edr\"", product_hash));

      // If the product is cacheable and etag was requested, respond with etag only

      if (request.getHeader("X-Request-ETag"))
      {
        response.setStatus(Spine::HTTP::Status::no_content);
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

void Plugin::query(const State& state,
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
        (q.forecastSource.length() == 0 &&
         strcasecmp(itsConfig.primaryForecastSource().c_str(), "grid") == 0))
      gridEnabled = true;

    formatter.reset(fmt);
    std::string mime;
    if (q.output_format == IWXXM_FORMAT)
      mime = "text/xml";
    else if (q.output_format == IWXXMZIP_FORMAT)
      mime = "application/zip";
    else if (q.output_format == TAC_FORMAT)
      mime = "text/plain";
    else
      mime = "application/json; charset=utf-8";
    response.setHeader("Content-Type", mime);

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
      product_hash = Fmi::hash_value(*result);
      if (etag_only(request, response, product_hash))
        return;

      if (q.output_format == IWXXMZIP_FORMAT)
      {
        if (qph.getZipFileName().empty())
        {
          response.setHeader("Content-Type", "application/json; charset=utf-8");
          response.setStatus(204);
        }
        else
          response.setHeader(
              "Content-Disposition",
              (std::string("attachement; filename=") + qph.getZipFileName()).c_str());
      }
      else if (*result == (q.valueformatter.missing() + "\n"))
      {
        result->clear();
        response.setStatus(204);
      }

      response.setContent(*result);
    }
  }
  catch (...)
  {
    throw Fmi::Exception::Trace(BCP, "Operation failed!");
  }
}

void Plugin::grouplocations(Spine::HTTP::Request& request) const
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
    auto fmisid = request.getParameter("fmisid");
    auto lpnn = request.getParameter("lpnn");
    auto wmo = request.getParameter("wmo");

    std::string wkt_multipoint = "MULTIPOINT(";
    parse_lonlats(lonlats, request, wkt_multipoint);
    parse_latlons(latlons, request, wkt_multipoint);
    parse_places(places, request, wkt_multipoint, itsEngines.geoEngine.get());

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
 * \brief Main content handler
 */
// ----------------------------------------------------------------------

void Plugin::requestHandler(Spine::Reactor& /* theReactor */,
                            const Spine::HTTP::Request& theRequest,
                            Spine::HTTP::Response& theResponse)
{
  // We need this in the catch block
  bool isdebug = false;

  try
  {
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
      theResponse.setContent(fullMessage);
      theResponse.setStatus(Spine::HTTP::Status::ok);
    }
    else
    {
      if (firstMessage.find("timeout") != std::string::npos)
        theResponse.setStatus(Spine::HTTP::Status::request_timeout);
      else
        theResponse.setStatus(Spine::HTTP::Status::bad_request);
    }

    // Adding the first exception information into the response header
    boost::algorithm::replace_all(firstMessage, "\n", " ");
    if (firstMessage.size() > 300)
      firstMessage.resize(300);
    theResponse.setHeader("X-EDRPlugin-Error", firstMessage);
  }
}

// ----------------------------------------------------------------------
/*!
 * \brief Plugin constructor
 */
// ----------------------------------------------------------------------

Plugin::Plugin(Spine::Reactor* theReactor, const char* theConfig)
    : itsModuleName("EDR"), itsConfig(theConfig), itsReactor(theReactor)
{
  try
  {
    if (theReactor->getRequiredAPIVersion() != SMARTMET_API_VERSION)
    {
      std::cerr << "*** EDRPlugin and Server SmartMet API version mismatch ***\n";
      return;
    }
  }
  catch (...)
  {
    throw Fmi::Exception::Trace(BCP, "Operation failed!");
  }
}

// ----------------------------------------------------------------------
/*!
 * \brief Initialize in a separate thread due to PostGIS slowness
 */
// ----------------------------------------------------------------------

void Plugin::init()
{
  using namespace boost::placeholders;

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
    if (!itsConfig.obsEngineDisabled())
    {
      /* ObsEngine */
      itsEngines.obsEngine =
          itsReactor->getEngine<Engine::Observation::Engine>("Observation", nullptr);

      // fetch obsebgine station types (producers)
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

    // Initialization done, register services. We are aware that throwing
    // from a separate thread will cause a crash, but these should never
    // fail.

    // Handler for the main content: /edr/...
    if (!itsReactor->addContentHandler(
            this,
            itsConfig.defaultUrl(),
            [this](Spine::Reactor& theReactor,
                   const Spine::HTTP::Request& theRequest,
                   Spine::HTTP::Response& theResponse)
            { callRequestHandler(theReactor, theRequest, theResponse); },
            {},
            true))
      throw Fmi::Exception(BCP, "Failed to register edr content handler");

    // Get locations
    updateSupportedLocations();
    // Get metadata
    updateMetaData(true);
    // Update metadata parameter info from config
    updateParameterInfo();

    if (Spine::Reactor::isShuttingDown())
      return;

    // Begin the update loop

    if (!itsConfig.metaDataUpdatesDisabled())
    {
      itsMetaDataUpdateTask.reset(
          new Fmi::AsyncTask("Plugin: metadata update task", [this]() { metaDataUpdateLoop(); }));
    }

    // DONE
    itsReady = true;
  }
  catch (...)
  {
    throw Fmi::Exception::Trace(BCP, "Operation failed!");
  }
}

void Plugin::metaDataUpdateLoop()
{
  try
  {
    while (!Spine::Reactor::isShuttingDown())
    {
      try
      {
        // update metadata every N seconds
        // FIXME: do we need to put interruption points into methods called
        // below?
        // TODO
        boost::this_thread::sleep_for(boost::chrono::seconds(itsConfig.metaDataUpdateInterval()));
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

void Plugin::updateMetaData(bool initial_phase)
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
        itsObservableProperties =
            itsEngines.obsEngine->observablePropertyQuery(params, default_language);
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
  }
  catch (...)
  {
    throw Fmi::Exception::Trace(BCP, "Operation failed!");
  }
}

std::map<std::string, Fmi::DateTime> Plugin::getNotificationTimes(SourceEngine source_engine,
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

void Plugin::checkNewDataAndNotify(const std::shared_ptr<EngineMetaData>& new_emd) const
{
  try
  {
    //	std::cout << " Plugin::checkNewMetaData\n";

    /*
      1) querydata: check origintimes
      2) grid-engine: check origintimes (analysisTime)
      3) avi-engine: endtime of avi engine metadata temporal extet is the latest data_time
      4) observation-engine: get latest data_time of each producer, either from cache or from
      database
    */

    std::map<SourceEngine, std::map<std::string, Fmi::DateTime>>
        times_to_notify;  // engine -> producer -> latest update time
    const SourceEngine source_engines[] = {
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

void Plugin::updateSupportedLocations()
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
        itsEngines.obsEngine->getStations(stations, settings);
        SupportedLocations sls;
        for (const auto& station : stations)
        {
          if (station.fmisid == 0 || station.geoid == 0)
            continue;
          location_info li(station);
          sls[li.id] = li;
        }
        if (!sls.empty())
          itsSupportedLocations[producer] = sls;
      }
    }
#endif

    // Get locations using keywords
    auto producer_keywords = itsConfig.getProducerKeywords();
    Locus::QueryOptions opts;
    opts.SetLanguage(itsConfig.defaultLanguage());
    for (const auto& item : producer_keywords)
    {
      auto producer = item.first;
      // Use keywords except for observation producers
      if (obs_producers.find(producer) != obs_producers.end())
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
        itsSupportedLocations[producer] = sls;
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

void Plugin::updateParameterInfo()
{
  try
  {
    auto metadata = itsMetaData.load();

    std::set<std::string> parameter_names = get_metadata_parameters(metadata);

    const auto& lconfig = itsConfig.config();

    for (const auto& pname : parameter_names)
    {
      if (pname.empty())
        continue;
      parameter_info_config pinfo;
      // Default values
      pinfo.description["en"] = pname;
      pinfo.unit_label["en"] = pname;

      auto pname_key = ("parameter_info." + pname);
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
            std::string value;
            lconfig.lookupValue(desc_key + "." + name, value);
            pinfo.description[name] = value;
          }
        }
        // Unit
        auto unit_label_key = (pname_key + ".unit.label");
        if (lconfig.exists(unit_label_key))
        {
          auto& setting = lconfig.lookup(unit_label_key);
          int len = setting.getLength();
          for (int i = 0; i < len; i++)
          {
            const auto* name = setting[i].getName();
            std::string value;
            lconfig.lookupValue(unit_label_key + "." + name, value);
            pinfo.unit_label[name] = value;
          }
        }
        lconfig.lookupValue(pname_key + ".unit.symbol.value", pinfo.unit_symbol_value);
        lconfig.lookupValue(pname_key + ".unit.symbol.type", pinfo.unit_symbol_type);
      }
      itsConfigParameterInfo[pname] = pinfo;
    }
  }
  catch (...)
  {
    throw Fmi::Exception::Trace(BCP, "Operation failed!");
  }
}

EDRMetaData Plugin::getProducerMetaData(const std::string& producer) const
{
  auto metadata = itsMetaData.load();
  EDRMetaData empty_emd;

  if (!metadata)
    return empty_emd;

  for (const auto& item : metadata->getMetaData())
  {
    const auto& engine_metadata = item.second;
    if (engine_metadata.find(producer) != engine_metadata.end())
      return engine_metadata.at(producer).front();
  }

  return empty_emd;
}

#ifndef WITHOUT_AVI
const EDRProducerMetaData& Plugin::getAviMetaData() const
{
  auto metadata = itsMetaData.load();

  return metadata->getMetaData(SourceEngine::Avi);
}
#endif

bool Plugin::isValidCollection(const std::string& producer) const
{
  auto metadata = itsMetaData.load();

  if (!metadata)
    return false;

  return metadata->isValidCollection(producer);
}

bool Plugin::isValidInstance(const std::string& producer, const std::string& instanceId) const
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
        if (Fmi::date_time::to_iso_string(md.temporal_extent.origin_time) == instanceId)
          return true;
    }
  }

  return false;
}

// ----------------------------------------------------------------------
/*!
 * \brief Shutdown the plugin
 */
// ----------------------------------------------------------------------

void Plugin::shutdown()
{
  try
  {
    std::cout << "  -- Shutdown requested (timeseries)\n";
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
 * \brief Destructor
 */
// ----------------------------------------------------------------------

Plugin::~Plugin() = default;

// ----------------------------------------------------------------------
/*!
 * \brief Return the plugin name
 */
// ----------------------------------------------------------------------

const std::string& Plugin::getPluginName() const
{
  return itsModuleName;
}
// ----------------------------------------------------------------------
/*!
 * \brief Return the required version
 */
// ----------------------------------------------------------------------

int Plugin::getRequiredAPIVersion() const
{
  return SMARTMET_API_VERSION;
}

const Fmi::TimeZones& Plugin::getTimeZones() const
{
  return itsEngines.geoEngine->getTimeZones();
}

// ----------------------------------------------------------------------
/*!
 * \brief Return true if the plugin has been initialized
 */
// ----------------------------------------------------------------------

bool Plugin::ready() const
{
  return itsReady;
}

Fmi::Cache::CacheStatistics Plugin::getCacheStats() const
{
  Fmi::Cache::CacheStatistics ret;

  ret.insert(std::make_pair("Timeseries::timeseries_generator_cache",
                            itsTimeSeriesCache->getCacheStats()));

  return ret;
}

}  // namespace EDR
}  // namespace Plugin
}  // namespace SmartMet

/*
 * Server knows us through the 'SmartMetPlugin' virtual interface, which
 * the 'Plugin' class implements.
 */

extern "C" SmartMetPlugin* create(SmartMet::Spine::Reactor* them, const char* config)
{
  return new SmartMet::Plugin::EDR::Plugin(them, config);
}

extern "C" void destroy(SmartMetPlugin* us)
{
  // This will call 'Plugin::~Plugin()' since the destructor is virtual
  delete us;
}

// ======================================================================
