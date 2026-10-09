#include "AviEngineQuery.h"
#include "LocationTools.h"
#include "EDRException.h"
#include "UtilityFunctions.h"
#include <macgyver/TimeParser.h>
#include <spine/HTTP.h>
#include <tuple>

using namespace boost::algorithm;

namespace SmartMet
{
namespace Plugin
{
namespace EDR
{

namespace
{

void storeAviData(const State & /* state */,
                  SmartMet::Engine::Avi::StationQueryData &aviData,
                  bool fetchIcao,
                  TS::OutputData &outputData)
{
  try
  {
    TS::TimeSeriesVectorPtr messageData(new TS::TimeSeriesVector());
    outputData.emplace_back("data", std::vector<TS::TimeSeriesData>());
    std::vector<TS::TimeSeriesData> &odata = (--outputData.end())->second;

    for (const auto &column : aviData.itsColumns)
    {
      // station id and icao code are automatically returned by aviengine.
      // messagetime is used when storing other column values
      //
      // BRAINSTORM-3305: icao code is fetched too for naming zipped IWXXM files

      if ((column.itsName == "stationid") || (column.itsName == "messagetime") ||
          ((column.itsName == "icao") && (!fetchIcao)))
        continue;

      TS::TimeSeriesGroupPtr messageData = std::make_shared<TS::TimeSeriesGroup>();

      for (auto stationId : aviData.itsStationIds)
      {
        auto timeIter = aviData.itsValues[stationId]["messagetime"].cbegin();
        auto longitude = std::get<double>(*(aviData.itsValues[stationId]["longitude"].cbegin()));
        auto latitude = std::get<double>(*(aviData.itsValues[stationId]["latitude"].cbegin()));
        TS::TimeSeries ts;
        for (auto &value : aviData.itsValues[stationId][column.itsName])
        {
          auto dt = std::get<Fmi::LocalDateTime>(*timeIter);
          TS::TimedValue tv(dt, value);
          ts.push_back(tv);
          timeIter++;
        }
        Spine::LonLat lonlat(longitude, latitude);
        TS::LonLatTimeSeries ll_ts(lonlat, ts);
        messageData->push_back(ll_ts);
      }

      if (!messageData->empty())
        odata.emplace_back(messageData);
    }
  }
  catch (...)
  {
    throw Fmi::Exception::Trace(BCP, "Operation failed!");
  }
}

void checkAviEngineLocationQuery(const Query &query,
                                 const EDRMetaData &edrMetaData,
                                 bool locationCheck,
                                 SmartMet::Engine::Avi::QueryOptions &queryOptions)
{
  try
  {
    if (query.icaos.empty())
      throw Fmi::Exception(BCP, "No location(s) to query", nullptr);

    for (auto const &icao : query.icaos)
    {
      if (locationCheck && (edrMetaData.locations->find(icao) == edrMetaData.locations->end()))
        throw Fmi::Exception(BCP, "Location is not listed in metadata", nullptr);

      queryOptions.itsLocationOptions.itsIcaos.push_back(icao);
    }
  }
  catch (...)
  {
    throw Fmi::Exception::Trace(BCP, "Operation failed!");
  }
}

void checkAviEnginePositionQuery(const Query &query,
                                 const EDRQuery &edrQuery,
                                 SmartMet::Engine::Avi::QueryOptions &queryOptions)
{
  try
  {
    // For position query set maxdistance 1 km
    auto wkt = query.requestWKT;
    int radius = 1.0;
    if (edrQuery.query_type == EDRQueryType::Radius)
    {
      std::string::size_type n = wkt.find(':');
      if (n == std::string::npos)
        throw Fmi::Exception(BCP, "Error! Radius query must contain radius!", nullptr);

      radius = Fmi::stoi(wkt.substr(n + 1));
      wkt = wkt.substr(0, n);
    }

    // Find out the point
    auto geom = get_ogr_geometry(wkt);
    if (geom->getGeometryType() != wkbPoint)
      throw Fmi::Exception(
          BCP,
          "Error! POINT geometry must be defined in " +
              std::string(edrQuery.query_type == EDRQueryType::Position ? "Position query!"
                                                                        : "Radius query!"));

    auto *point = geom->toPoint();
    auto lon = point->getX();
    auto lat = point->getY();

    // Lets set longitude, latitude and maxdistance
    queryOptions.itsLocationOptions.itsLonLats.emplace_back(lon, lat);
    queryOptions.itsLocationOptions.itsMaxDistance = (radius * 1000);
    queryOptions.itsLocationOptions.itsNumberOfNearestStations = 1;
  }
  catch (...)
  {
    throw Fmi::Exception::Trace(BCP, "Operation failed!");
  }
}

void checkAviEngineAreaQuery(const Query &query, SmartMet::Engine::Avi::QueryOptions &queryOptions)
{
  try
  {
    if (query.requestWKT.empty())
      throw Fmi::Exception(BCP, "No area to query", nullptr);

    auto wkt = query.requestWKT;
    //	if(edrQuery.query_type == EDRQueryType::Corridor)
    {
      queryOptions.itsLocationOptions.itsMaxDistance = 0;
      std::string::size_type n = wkt.find(':');
      if (n != std::string::npos)
      {
        queryOptions.itsLocationOptions.itsMaxDistance = (Fmi::stoi(wkt.substr(n + 1)) * 1000);
        queryOptions.itsLocationOptions.itsNumberOfNearestStations = 1;
        wkt = wkt.substr(0, n);
      }
    }
    queryOptions.itsLocationOptions.itsWKTs.itsWKTs.push_back(wkt);
  }
  catch (...)
  {
    throw Fmi::Exception::Trace(BCP, "Operation failed!");
  }
}

void checkAviEngineQuery(const Query &query,
                         const std::vector<EDRMetaData> &edrMetaDataVector,
                         bool locationCheck,
                         SmartMet::Engine::Avi::QueryOptions &queryOptions)
{
  try
  {
    const auto &edrQuery = query.edrQuery();
    // In AVI engine there is only one metadata for each producer/collection (e.g. querydata has
    // several instances)
    const auto &edrMetaData = edrMetaDataVector.front();

    if (edrQuery.query_type == EDRQueryType::Locations)
      checkAviEngineLocationQuery(query, edrMetaData, locationCheck, queryOptions);
    else if (edrQuery.query_type == EDRQueryType::Position ||
             edrQuery.query_type == EDRQueryType::Radius)
      checkAviEnginePositionQuery(query, edrQuery, queryOptions);
    else
      checkAviEngineAreaQuery(query, queryOptions);
  }
  catch (...)
  {
    throw Fmi::Exception::Trace(BCP, "Operation failed!");
  }
}

}  // namespace

AviEngineQuery::AviEngineQuery(const PluginImpl &thePlugin) : itsPlugin(thePlugin) {}

#ifndef WITHOUT_AVI
bool AviEngineQuery::isAviProducer(const std::string &producer) const
{
  try
  {
    auto producer_name = trim_copy(to_lower_copy(producer));
    auto avi_meta_data = itsPlugin.getAviMetaData();
    return (avi_meta_data.find(producer_name) != avi_meta_data.end());
  }
  catch (...)
  {
    throw Fmi::Exception::Trace(BCP, "Operation failed!");
  }
}

void AviEngineQuery::processAviEngineQuery(const Config &config,
                                           const State &state,
                                           const Query &query,
                                           const std::string &prod,
                                           TS::OutputData &outputData) const
{
  try
  {
    const auto producer = trim_copy(to_lower_copy(prod));
    const auto &edrProducerMetaData = state.getAviMetaData();
    const auto edrMetaData = edrProducerMetaData.find(producer);
    const auto &aviCollection = get_avi_collection(producer, config.getAviCollections());

    if ((edrMetaData == edrProducerMetaData.end()) || edrMetaData->second.empty() ||
        aviCollection.getName().empty())
      throw Fmi::Exception(BCP, "Internal error: no metadata for producer " + producer, nullptr);

    SmartMet::Engine::Avi::QueryOptions queryOptions;
    bool locationCheck = false;

    checkAviEngineQuery(query, edrMetaData->second, locationCheck, queryOptions);

    // Column order must be dataparam, lon, lat for outputData handling later in the code.
    // messagetime is needed for output.
    //
    // station id and icao code are automatically returned by aviengine
    //
    // BRAINSTORM-3305: icao code is fetched too for naming zipped IWXXM files
    //
    bool fetchIcao = (query.output_format == IWXXMZIP_FORMAT);
    if (fetchIcao)
      queryOptions.itsParameters.emplace_back("icao");

    queryOptions.itsParameters.emplace_back("messagetime");
    queryOptions.itsParameters.emplace_back("message");
    queryOptions.itsParameters.emplace_back("longitude");
    queryOptions.itsParameters.emplace_back("latitude");

    queryOptions.itsValidity = SmartMet::Engine::Avi::Validity::Accepted;
    queryOptions.itsMessageTypes.push_back(to_upper_copy(producer));
    auto message_format =
        (query.output_format == COVERAGE_JSON_FORMAT || query.output_format == GEO_JSON_FORMAT
             ? TAC_FORMAT
             : query.output_format);
    queryOptions.itsMessageFormat = message_format;

    // Time range or observation time to fetch messages
    //
    // TODO: Handle localtime vs UTC, message query times must be in UTC
    //
    std::string startTime;
    std::string endTime;
    bool hasStartTime = (!query.toptions.startTime.is_special());
    bool hasEndTime = (!query.toptions.endTime.is_special());

    if (hasStartTime)
      startTime = Fmi::date_time::to_iso_string(query.toptions.startTime);
    if (hasEndTime)
      endTime = Fmi::date_time::to_iso_string(query.toptions.endTime);

    if (hasStartTime && hasEndTime && (startTime != endTime))
    {
      queryOptions.itsTimeOptions.itsStartTime = "timestamptz '" + startTime + "Z'";
      queryOptions.itsTimeOptions.itsEndTime = "timestamptz '" + endTime + "Z'";
    }
    else if (hasStartTime)
      queryOptions.itsTimeOptions.itsObservationTime = "timestamptz '" + startTime + "Z'";
    else if ((!hasStartTime) && (!hasEndTime))
      queryOptions.itsTimeOptions.itsObservationTime = "current_timestamp";
    else
      throw Fmi::Exception(BCP, "Only query end time is given");

    queryOptions.itsTimeOptions.itsTimeFormat = "iso";

    // Filter off message duplicates (same message received via multiple routes)
    //
    queryOptions.itsDistinctMessages = true;

    // Allowed max number of stations and messages is controlled by engine configuration
    //
    queryOptions.itsMaxMessageStations = -1;
    queryOptions.itsMaxMessageRows = -1;

    // Query is based on message validity, not by creation time
    //
    queryOptions.itsTimeOptions.itsQueryValidRangeMessages = true;

    // Finnish TAC METAR filtering (ignore messages not starting with 'METAR')
    //
    queryOptions.itsFilterMETARs = (queryOptions.itsMessageFormat == TAC_FORMAT);

    // Finnish SPECIs are ignored (https://jira.fmi.fi/browse/BRAINSTORM-2472)
    //
    // BRAINSTORM-3284: now configurable, defaults to true
    //
    queryOptions.itsExcludeSPECIs = config.excludeAviSPECI();

    if ((!queryOptions.itsExcludeSPECIs) && (producer == METAR))
      queryOptions.itsMessageTypes.emplace_back("SPECI");

    // BRAINSTORM-3300
    //
    // Do not check/filter if messages were created after the given messagetime and
    // do not apply message query time restrictions (used for TAFs)
    //
    // itsUseCurrentTime is set when fetching latest messages; itsObservationTime was set
    // whether the user provided the time or not (in which case the current time was used) and
    // SIGMETs are fetched differently depending on if the time is given by the user or not
    //
    // When fetching SIGMETs with time range message validity is not taken ínto account
    //
    queryOptions.itsTimeOptions.itsMessageTimeChecks = false;
    queryOptions.itsTimeOptions.itsUseCurrentTime = query.useCurrentTime;

    if ((producer == SIGMET) && hasStartTime && hasEndTime)
    {
      queryOptions.itsTimeOptions.itsQueryValidRangeMessages = false;
      queryOptions.itsTimeOptions.itsClosedTimeRange = true;
    }

    // BRAINSTORM-3288
    //
    // Set additional query filters
    //
    // Note: currently aviengine applies the filters in 'countries=...' (collection metadata)
    //       and non route wkt (e.g. wkt=POLYGON(...)) station queries only, including stations
    //       by country codes and includíng/excluding by icao codes
    //
    if (!aviCollection.getCountries().empty())
      queryOptions.itsLocationOptions.itsIncludeCountryFilters.insert(
          queryOptions.itsLocationOptions.itsIncludeCountryFilters.begin(),
          aviCollection.getCountries().begin(),
          aviCollection.getCountries().end());

    if (!aviCollection.getIcaos().empty())
      queryOptions.itsLocationOptions.itsIncludeIcaoFilters.insert(
          queryOptions.itsLocationOptions.itsIncludeIcaoFilters.begin(),
          aviCollection.getIcaos().begin(),
          aviCollection.getIcaos().end());

    if (!aviCollection.getExcludeIcaoFilters().empty())
      queryOptions.itsLocationOptions.itsExcludeIcaoFilters.insert(
          queryOptions.itsLocationOptions.itsExcludeIcaoFilters.begin(),
          aviCollection.getExcludeIcaoFilters().begin(),
          aviCollection.getExcludeIcaoFilters().end());

    auto aviData = itsPlugin.getEngines().aviEngine->queryStationsAndMessages(queryOptions);

    storeAviData(state, aviData, fetchIcao, outputData);
  }
  catch (...)
  {
    throw Fmi::Exception::Trace(BCP, "Operation failed!");
  }
}

namespace
{
struct AviItem
{
  Fmi::DateTime time;
  std::string icao;
  std::string id;
  std::string message;
  double lon = 0;
  double lat = 0;
};

std::string avi_time(const Fmi::DateTime &t)
{
  return "timestamptz '" + Fmi::date_time::to_iso_string(t) + "Z'";
}

std::string item_id(const std::string &icao, const Fmi::DateTime &t, long messageid)
{
  return icao + "_" + Fmi::date_time::to_iso_string(t) + "Z_" + Fmi::to_string(messageid);
}

// The time range of an items query, the temporal extent of the collection by default
std::pair<Fmi::DateTime, Fmi::DateTime> items_time_range(const EDRQuery &edr_query,
                                                        const EDRMetaData &emd)
{
  const auto now = Fmi::SecondClock::universal_time();
  Fmi::DateTime start = now - Fmi::Hours(24);
  Fmi::DateTime end = now;
  const auto &periods = emd.temporal_extent.time_periods;
  if (!periods.empty())
    start = periods.front().start_time;

  auto parse = [](const std::string &str)
  {
    try
    {
      return Fmi::TimeParser::parse(str);
    }
    catch (...)
    {
      throw EDRException("Invalid 'datetime' value '" + str + "'");
    }
  };

  const auto &datetime = edr_query.datetime;
  if (datetime.empty())
    return {start, end};

  auto pos = datetime.find('/');
  if (pos == std::string::npos)
  {
    auto t = parse(datetime);
    return {t, t};
  }
  auto first = datetime.substr(0, pos);
  auto second = datetime.substr(pos + 1);
  if (first != "..")
    start = parse(first);
  if (second != "..")
    end = parse(second);
  if (end < start)
    throw EDRException("Invalid 'datetime' value '" + datetime + "', the end is before the start");
  return {start, end};
}

Json::Value item_feature(const AviItem &item,
                         const EDRMetaData &emd,
                         const std::string &collection_url)
{
  auto feature = Json::Value(Json::ValueType::objectValue);
  feature["type"] = Json::Value("Feature");
  feature["id"] = Json::Value(item.id);

  auto geometry = Json::Value(Json::ValueType::objectValue);
  if (!emd.getGeometry(item.icao, geometry))
  {
    geometry["type"] = Json::Value("Point");
    auto coordinates = Json::Value(Json::ValueType::arrayValue);
    coordinates[0] = Json::Value(item.lon, emd.getPrecision("longitude"));
    coordinates[1] = Json::Value(item.lat, emd.getPrecision("latitude"));
    geometry["coordinates"] = coordinates;
  }
  feature["geometry"] = geometry;

  auto properties = Json::Value(Json::ValueType::objectValue);
  const auto time = Fmi::date_time::to_iso_extended_string(item.time) + "Z";
  properties["datetime"] = Json::Value(time);
  properties["icao"] = Json::Value(item.icao);
  properties["message"] = Json::Value(item.message);
  feature["properties"] = properties;

  auto links = Json::Value(Json::ValueType::arrayValue);
  auto link = Json::Value(Json::ValueType::objectValue);
  link["href"] = Json::Value(collection_url + "/items/" + item.id);
  link["rel"] = Json::Value("self");
  link["type"] = Json::Value("application/geo+json");
  links.append(link);
  feature["links"] = links;
  return feature;
}

}  // namespace

Json::Value AviEngineQuery::processItemsQuery(const Config &config,
                                              const State &state,
                                              const EDRQuery &edr_query,
                                              Links *response_links) const
{
  try
  {
    const auto producer = trim_copy(to_lower_copy(edr_query.collection_id));
    const auto &edrProducerMetaData = state.getAviMetaData();
    const auto edrMetaData = edrProducerMetaData.find(producer);
    const auto &aviCollection = get_avi_collection(producer, config.getAviCollections());

    if ((edrMetaData == edrProducerMetaData.end()) || edrMetaData->second.empty() ||
        aviCollection.getName().empty())
      throw EDRNotFound("Collection '" + producer + "' has no items");
    const auto &emd = edrMetaData->second.front();

    SmartMet::Engine::Avi::QueryOptions queryOptions;

    // A single item is identified by ICAO_messagetime_messageid
    std::optional<long> wanted_messageid;
    Fmi::DateTime start;
    Fmi::DateTime end;
    if (!edr_query.item_id.empty())
    {
      std::vector<std::string> parts;
      boost::algorithm::split(parts, edr_query.item_id, boost::algorithm::is_any_of("_"));
      try
      {
        if (parts.size() != 3 || parts[0].empty())
          throw std::runtime_error("");
        start = end = Fmi::TimeParser::parse(parts[1]);
        wanted_messageid = std::stol(parts[2]);
      }
      catch (...)
      {
        throw EDRNotFound("Item '" + edr_query.item_id + "' not found");
      }
      queryOptions.itsLocationOptions.itsIcaos.push_back(parts[0]);
    }
    else
    {
      std::tie(start, end) = items_time_range(edr_query, emd);

      if (!edr_query.bbox.empty())
      {
        auto parts = UtilityFunctions::parseBBox(edr_query.bbox, true);
        queryOptions.itsLocationOptions.itsBBoxes.emplace_back(
            Fmi::stod(parts[0]), Fmi::stod(parts[2]), Fmi::stod(parts[1]), Fmi::stod(parts[3]));
      }
      else if (emd.locations)
      {
        for (const auto &item : *emd.locations)
          queryOptions.itsLocationOptions.itsIcaos.push_back(item.first);
      }
    }

    queryOptions.itsParameters.emplace_back("icao");
    queryOptions.itsParameters.emplace_back("messageid");
    queryOptions.itsParameters.emplace_back("messagetime");
    queryOptions.itsParameters.emplace_back("message");
    queryOptions.itsParameters.emplace_back("longitude");
    queryOptions.itsParameters.emplace_back("latitude");

    queryOptions.itsValidity = SmartMet::Engine::Avi::Validity::Accepted;
    queryOptions.itsMessageTypes.push_back(to_upper_copy(producer));
    queryOptions.itsMessageFormat = TAC_FORMAT;

    // The items are selected by their message time
    queryOptions.itsTimeOptions.itsQueryValidRangeMessages = false;
    queryOptions.itsTimeOptions.itsClosedTimeRange = true;
    queryOptions.itsTimeOptions.itsMessageTimeChecks = false;
    queryOptions.itsTimeOptions.itsStartTime = avi_time(start);
    queryOptions.itsTimeOptions.itsEndTime = avi_time(end);
    queryOptions.itsTimeOptions.itsTimeFormat = "iso";
    queryOptions.itsDistinctMessages = true;
    queryOptions.itsMaxMessageStations = -1;
    queryOptions.itsMaxMessageRows = -1;
    queryOptions.itsFilterMETARs = true;
    queryOptions.itsExcludeSPECIs = config.excludeAviSPECI();
    if ((!queryOptions.itsExcludeSPECIs) && (producer == METAR))
      queryOptions.itsMessageTypes.emplace_back("SPECI");

    if (!aviCollection.getCountries().empty())
      queryOptions.itsLocationOptions.itsIncludeCountryFilters.assign(
          aviCollection.getCountries().begin(), aviCollection.getCountries().end());
    if (!aviCollection.getIcaos().empty())
      queryOptions.itsLocationOptions.itsIncludeIcaoFilters.assign(
          aviCollection.getIcaos().begin(), aviCollection.getIcaos().end());
    if (!aviCollection.getExcludeIcaoFilters().empty())
      queryOptions.itsLocationOptions.itsExcludeIcaoFilters.assign(
          aviCollection.getExcludeIcaoFilters().begin(),
          aviCollection.getExcludeIcaoFilters().end());

    std::vector<AviItem> items;
    if (!queryOptions.itsLocationOptions.itsIcaos.empty() ||
        !queryOptions.itsLocationOptions.itsBBoxes.empty())
    {
      auto aviData = itsPlugin.getEngines().aviEngine->queryStationsAndMessages(queryOptions);

      for (auto stationId : aviData.itsStationIds)
      {
        auto &values = aviData.itsValues[stationId];
        const auto &times = values["messagetime"];
        const auto &ids = values["messageid"];
        const auto &messages = values["message"];
        const auto &icaos = values["icao"];
        const auto &lons = values["longitude"];
        const auto &lats = values["latitude"];
        for (std::size_t i = 0; i < times.size(); i++)
        {
          const auto *t = std::get_if<Fmi::LocalDateTime>(&times[i]);
          if (!t || i >= ids.size() || i >= messages.size())
            continue;
          AviItem item;
          item.time = t->utc_time();
          item.icao = (i < icaos.size() && std::holds_alternative<std::string>(icaos[i])
                           ? std::get<std::string>(icaos[i])
                           : std::string());
          long messageid = 0;
          if (const auto *n = std::get_if<int>(&ids[i]))
            messageid = *n;
          else if (const auto *d = std::get_if<double>(&ids[i]))
            messageid = static_cast<long>(*d);
          if (wanted_messageid && messageid != *wanted_messageid)
            continue;
          item.id = item_id(item.icao, item.time, messageid);
          if (const auto *m = std::get_if<std::string>(&messages[i]))
            item.message = *m;
          if (!lons.empty() && std::holds_alternative<double>(lons.front()))
            item.lon = std::get<double>(lons.front());
          if (!lats.empty() && std::holds_alternative<double>(lats.front()))
            item.lat = std::get<double>(lats.front());
          items.push_back(std::move(item));
        }
      }
    }

    std::sort(items.begin(),
              items.end(),
              [](const AviItem &a, const AviItem &b)
              { return std::tie(a.time, a.icao, a.id) < std::tie(b.time, b.icao, b.id); });

    const auto collection_url = edr_query.host + "/collections/" + edr_query.collection_id;

    if (!edr_query.item_id.empty())
    {
      if (items.empty())
        throw EDRNotFound("Item '" + edr_query.item_id + "' not found");
      auto feature = item_feature(items.front(), emd, collection_url);
      Links links{{collection_url + "/items/" + items.front().id, "self", "application/geo+json"},
                  {collection_url, "collection", "application/json"}};
      feature["links"] = links_json(links);
      if (response_links)
        *response_links = links;
      return feature;
    }

    // Paging
    const auto matched = items.size();
    const auto first = std::min(edr_query.offset, matched);
    const auto count = std::min(edr_query.limit.value_or(matched), matched - first);

    auto features = Json::Value(Json::ValueType::arrayValue);
    for (std::size_t i = first; i < first + count; i++)
      features.append(item_feature(items[i], emd, collection_url));

    auto href = [&](std::size_t offset)
    {
      std::string url = collection_url + "/items?limit=" + Fmi::to_string(*edr_query.limit);
      if (offset > 0)
        url += "&offset=" + Fmi::to_string(offset);
      if (!edr_query.bbox.empty())
        url += "&bbox=" + Spine::HTTP::urlencode(edr_query.bbox);
      if (!edr_query.datetime.empty())
        url += "&datetime=" + Spine::HTTP::urlencode(edr_query.datetime);
      return url;
    };

    Links links;
    links.push_back({href(first), "self", "application/geo+json"});
    if (first + count < matched)
      links.push_back({href(first + count), "next", "application/geo+json"});
    if (first > 0)
      links.push_back({href(first > *edr_query.limit ? first - *edr_query.limit : 0),
                       "prev",
                       "application/geo+json"});
    links.push_back({collection_url, "collection", "application/json"});
    if (response_links)
      *response_links = links;

    auto result = Json::Value(Json::ValueType::objectValue);
    result["type"] = Json::Value("FeatureCollection");
    result["features"] = features;
    result["numberMatched"] = Json::Value(matched);
    result["numberReturned"] = Json::Value(count);
    result["links"] = links_json(links);
    return result;
  }
  catch (...)
  {
    throw Fmi::Exception::Trace(BCP, "Operation failed!");
  }
}

#endif

}  // namespace EDR
}  // namespace Plugin
}  // namespace SmartMet

// ======================================================================
