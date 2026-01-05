#include "AviEngineQuery.h"
#include "LocationTools.h"

using namespace boost::algorithm;

namespace SmartMet
{
namespace Plugin
{
namespace EDR
{

namespace
{

void storeAviData(const State &state,
                  SmartMet::Engine::Avi::StationQueryData &aviData,
                  bool fetchIcao,
                  TS::OutputData &outputData)
{
  try
  {
    TS::TimeSeriesVectorPtr messageData(new TS::TimeSeriesVector());
    outputData.push_back(make_pair("data", std::vector<TS::TimeSeriesData>()));
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

      TS::TimeSeriesGroupPtr messageData(new TS::TimeSeriesGroup());

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
    queryOptions.itsLocationOptions.itsLonLats.push_back({lon, lat});
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

AviEngineQuery::AviEngineQuery(const Plugin &thePlugin) : itsPlugin(thePlugin) {}

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

    if ((edrMetaData == edrProducerMetaData.end()) || aviCollection.getName().empty())
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
      queryOptions.itsParameters.push_back("icao");

    queryOptions.itsParameters.push_back("messagetime");
    queryOptions.itsParameters.push_back("message");
    queryOptions.itsParameters.push_back("longitude");
    queryOptions.itsParameters.push_back("latitude");

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
      queryOptions.itsMessageTypes.push_back("SPECI");

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

#endif

}  // namespace EDR
}  // namespace Plugin
}  // namespace SmartMet

// ======================================================================
