// ======================================================================
/*!
 * \brief Query parameters common to both EDR and TimeSeries request styles
 */
// ======================================================================

#include "CommonQuery.h"
#include "Config.h"
#include <engines/observation/ExternalAndMobileProducerId.h>
#include <engines/querydata/Engine.h>
#include <gis/CoordinateTransformation.h>
#include <macgyver/AnsiEscapeCodes.h>
#include <macgyver/DistanceParser.h>
#include <spine/Convenience.h>

using namespace std;

namespace SmartMet
{
namespace Plugin
{
namespace EDR
{
namespace
{

const char* default_timezone = "localtime";

void parse_ids(const std::optional<std::string>& string_param, std::vector<std::string>& id_vector)
{
  try
  {
    if (string_param)
      boost::algorithm::split(id_vector, *string_param, boost::algorithm::is_any_of(","));
  }
  catch (...)
  {
    throw Fmi::Exception::Trace(BCP, "Operation failed!");
  }
}

void parse_ids(const std::optional<std::string>& string_param, std::vector<int>& id_vector)
{
  try
  {
    if (string_param)
    {
      std::vector<string> parts;
      boost::algorithm::split(parts, *string_param, boost::algorithm::is_any_of(","));
      for (const string& id_string : parts)
        id_vector.push_back(Fmi::stoi(id_string));
    }
  }
  catch (...)
  {
    throw Fmi::Exception::Trace(BCP, "Operation failed!");
  }
}

void set_max_agg_interval_behind(TS::DataFunction& func, unsigned int& max_interval)
{
  try
  {
    if (func.type() == TS::FunctionType::TimeFunction)
      max_interval = std::max(max_interval, func.getAggregationIntervalBehind());
  }
  catch (...)
  {
    throw Fmi::Exception::Trace(BCP, "Operation failed!");
  }
}

void set_max_agg_interval_ahead(TS::DataFunction& func, unsigned int& max_interval)
{
  try
  {
    if (func.type() == TS::FunctionType::TimeFunction)
      max_interval = std::max(max_interval, func.getAggregationIntervalAhead());
  }
  catch (...)
  {
    throw Fmi::Exception::Trace(BCP, "Operation failed!");
  }
}

void set_agg_interval_behind(TS::DataFunction& func, unsigned int interval)
{
  try
  {
    if (func.getAggregationIntervalBehind() == std::numeric_limits<unsigned int>::max())
      func.setAggregationIntervalBehind(interval);
  }
  catch (...)
  {
    throw Fmi::Exception::Trace(BCP, "Operation failed!");
  }
}

void set_agg_interval_ahead(TS::DataFunction& func, unsigned int interval)
{
  try
  {
    if (func.getAggregationIntervalAhead() == std::numeric_limits<unsigned int>::max())
      func.setAggregationIntervalAhead(interval);
  }
  catch (...)
  {
    throw Fmi::Exception::Trace(BCP, "Operation failed!");
  }
}

[[maybe_unused]] void add_sql_data_filter(const Spine::HTTP::Request& req,
                                          const std::string& param_name,
                                          TS::DataFilter& dataFilter)
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

void report_unsupported_option(const std::string& name, const std::optional<std::string>& value)
{
  if (value)
    std::cerr << (Spine::log_time_str() + ANSI_FG_RED + " [timeseries] Deprecated option '" +
                  name + ANSI_FG_DEFAULT)
              << '\n';
}

Fmi::ValueFormatterParam valueformatter_params(const Spine::HTTP::Request& req)
{
  Fmi::ValueFormatterParam opt;
  opt.missingText = Spine::optional_string(req.getParameter("missingtext"), "nan");
  opt.floatField = Spine::optional_string(req.getParameter("floatfield"), "fixed");
  if (Spine::optional_string(req.getParameter("format"), "") == "WXML")
    opt.missingText = "NaN";
  return opt;
}

}  // namespace

// ----------------------------------------------------------------------
/*!
 * \brief Thin constructor: sets up ObsQueryParams and alias collection.
 *        Derived classes must call commonInit() from their constructor body.
 */
// ----------------------------------------------------------------------

CommonQuery::CommonQuery(const State& state, const Spine::HTTP::Request& req, Config& config)
    : ObsQueryParams(req),
      valueformatter(valueformatter_params(req))
{
  try
  {
    time_t tt = time(nullptr);
    if ((config.itsLastAliasCheck + 10) < tt)
    {
      config.itsAliasFileCollection.checkUpdates(false);
      config.itsLastAliasCheck = tt;
    }
    itsAliasFileCollectionPtr = &config.itsAliasFileCollection;
    maxdistance = config.defaultMaxDistance();
  }
  catch (...)
  {
    throw Fmi::Exception::Trace(BCP, "Operation failed!");
  }
}

// ----------------------------------------------------------------------
/*!
 * \brief Parse all common query parameters.
 *
 * Called explicitly from derived class constructors so that virtual
 * dispatch works correctly for parse_producers().
 *
 * @param stepDefault  Default value for the 'step' parameter (0.0 for EDR, 1.0 for timeseries).
 * @param setDataTimesMode  If true, override toptions mode when no timestep is given (EDR style).
 * @param extraProducerCheck  Optional additional producer validator (e.g. AVI check for EDR).
 */
// ----------------------------------------------------------------------

void CommonQuery::commonInit(const State& state,
                             const Spine::HTTP::Request& req,
                             Config& config,
                             double stepDefault,
                             bool setDataTimesMode,
                             const std::function<bool(const std::string&)>& extraProducerCheck)
{
  try
  {
    is_timeseries_query = req.getResource() == config.timeSeriesUrl();

    // FMISIDs
    auto name = req.getParameter("fmisid");
    parse_ids(name, fmisids);
    // sid is an alias for fmisid
    name = req.getParameter("sid");
    parse_ids(name, fmisids);
    // WMOs
    name = req.getParameter("wmo");
    parse_ids(name, wmos);
    // LPNNs
    name = req.getParameter("lpnn");
    parse_ids(name, lpnns);
    // RWSIDs
    name = req.getParameter("rwsid");
    parse_ids(name, rwsids);
    // WIGOS Station identifiers
    parse_ids(req.getParameter("wsi"), wsis);

    report_unsupported_option("adjustfield", req.getParameter("adjustfield"));
    report_unsupported_option("width", req.getParameter("width"));
    report_unsupported_option("fill", req.getParameter("fill"));
    report_unsupported_option("showpos", req.getParameter("showpos"));
    report_unsupported_option("uppercase", req.getParameter("uppercase"));

    language = Spine::optional_string(req.getParameter("lang"), config.defaultLanguage());

    forecastSource = Spine::optional_string(req.getParameter("source"), "");

    parse_attr(req);

    if (!parse_grib_loptions(state))
    {
      loptions = std::make_shared<Engine::Geonames::LocationOptions>(
          state.getGeoEngine().parseLocations(req));

      auto lon_coord = Spine::optional_double(req.getParameter("x"), kFloatMissing);
      auto lat_coord = Spine::optional_double(req.getParameter("y"), kFloatMissing);
      auto source_crs = Spine::optional_string(req.getParameter("crs"), "");

      if (lon_coord != kFloatMissing && lat_coord != kFloatMissing && !source_crs.empty())
      {
        if (source_crs != "ESPG:4326")
        {
          Fmi::CoordinateTransformation transformation(source_crs, "WGS84");
          transformation.transform(lon_coord, lat_coord);
        }
        auto tmpReq = req;
        tmpReq.addParameter("lonlat",
                            (Fmi::to_string(lon_coord) + "," + Fmi::to_string(lat_coord)));
        loptions = std::make_shared<Engine::Geonames::LocationOptions>(
            state.getGeoEngine().parseLocations(tmpReq));
      }
    }

    wktGeometries = state.getGeoEngine().getWktGeometries(*loptions, language);

    toptions = TS::parseTimes(req);

    if (setDataTimesMode && ((!toptions.timeStep) || (*toptions.timeStep == 0)))
      toptions.mode = TS::TimeSeriesGeneratorOptions::DataTimes;

#ifdef MYDEBUG
    std::cout << "Time options:\n" << toptions << '\n';
#endif

    latestTimestep = toptions.startTime;

    starttimeOptionGiven = (!!req.getParameter("starttime") || !!req.getParameter("now"));
    std::string endtime = Spine::optional_string(req.getParameter("endtime"), "");
    endtimeOptionGiven = (endtime != "now");
    useCurrentTime = (!!req.getParameter("usecurrenttime"));

    debug = Spine::optional_bool(req.getParameter("debug"), false);

    timezone = Spine::optional_string(req.getParameter("tz"), default_timezone);

    step = Spine::optional_double(req.getParameter("step"), stepDefault);
    leveltype = Spine::optional_string(req.getParameter("leveltype"), "");
    areasource = Spine::optional_string(req.getParameter("areasource"), "");
    crs = Spine::optional_string(req.getParameter("crs"), "");

    auto opt_locale = req.getParameter("locale");
    if (!opt_locale)
      outlocale = config.defaultLocale();
    else
      outlocale = locale(opt_locale->c_str());

    timeformat = Spine::optional_string(req.getParameter("timeformat"), config.defaultTimeFormat());

    maxdistanceOptionGiven = !!req.getParameter("maxdistance");
    maxdistance =
        Spine::optional_string(req.getParameter("maxdistance"), config.defaultMaxDistance());

    keyword = Spine::optional_string(req.getParameter("keyword"), "");

    parse_inkeyword_locations(req, state);

    findnearestvalidpoint = Spine::optional_bool(req.getParameter("findnearestvalid"), false);

    parse_origintime(req);
    parse_producers(req, state, extraProducerCheck);
    parse_parameters(req);

    parse_levels(req);

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

    parse_precision(req, config);

    timeformatter.reset(Fmi::TimeFormatter::create(timeformat));

    if (!!req.getParameter("weekday"))
    {
      const string sweekdays = *req.getParameter("weekday");
      vector<string> parts;
      boost::algorithm::split(parts, sweekdays, boost::algorithm::is_any_of(","));
      for (const string& wday : parts)
        this->weekdays.push_back(Fmi::stoi(wday));
    }

    startrow = Spine::optional_size(req.getParameter("startrow"), 0);
    maxresults = Spine::optional_size(req.getParameter("maxresults"), 0);
    groupareas = Spine::optional_bool(req.getParameter("groupareas"), true);
  }
  catch (...)
  {
    throw Fmi::Exception::Trace(BCP, "Plugin failed to parse query string options!")
        .disableLogging();
  }
}

// ----------------------------------------------------------------------

void CommonQuery::parse_producers(const Spine::HTTP::Request& theReq,
                                  const State& theState,
                                  const std::function<bool(const std::string&)>& extraCheck)
{
  try
  {
    string opt = Spine::optional_string(theReq.getParameter("model"), "");
    string opt2 = Spine::optional_string(theReq.getParameter("producer"), "");

    if (opt == "default" || opt2 == "default")
      return;

    if (opt.empty() && opt2.empty())
      opt2 = Spine::optional_string(theReq.getParameter("stationtype"), "");

    std::list<std::string> resultProducers;

    if (!opt.empty())
      boost::algorithm::split(resultProducers, opt, boost::algorithm::is_any_of(";"));
    else if (!opt2.empty())
      boost::algorithm::split(resultProducers, opt2, boost::algorithm::is_any_of(";"));

    for (auto& p : resultProducers)
    {
      boost::algorithm::to_lower(p);
      if (p == "itmf")
      {
        p = Engine::Observation::FMI_IOT_PRODUCER;
        iot_producer_specifier = "itmf";
      }
    }

    const auto obsProducers = getObsProducers(theState);

    for (const auto& p : resultProducers)
    {
      const auto& qEngine = theState.getQEngine();
      bool ok = qEngine.hasProducer(p);
      if (!obsProducers.empty() && !ok)
        ok = (obsProducers.find(p) != obsProducers.end());

      const auto* gridEngine = theState.getGridEngine();
      if (!ok && gridEngine)
        ok = gridEngine->isGridProducer(p);

      if (!ok && extraCheck)
        ok = extraCheck(p);

      if (!ok)
        throw Fmi::Exception(BCP, "Unknown producer name '" + p + "'");
    }

    for (const auto& tproducers : resultProducers)
    {
      AreaProducers producers;
      boost::algorithm::split(producers, tproducers, boost::algorithm::is_any_of(","));
      timeproducers.push_back(producers);
    }
  }
  catch (...)
  {
    throw Fmi::Exception::Trace(BCP, "Operation failed!");
  }
}

void CommonQuery::parse_levels(const Spine::HTTP::Request& theReq)
{
  try
  {
    string opt = Spine::optional_string(theReq.getParameter("level"), "");
    if (!opt.empty())
      levels.insert(Fmi::stoi(opt));

    opt = Spine::optional_string(theReq.getParameter("levels"), "");
    if (!opt.empty())
    {
      vector<string> parts;
      boost::algorithm::split(parts, opt, boost::algorithm::is_any_of(","));

      levelRange = (!Spine::optional_string(theReq.getParameter("levelrange"), "").empty());
      if (levelRange && (parts.size() != 2))
        throw Fmi::Exception(BCP, "Internal error: 2 levels expected for level range query");

      for (const string& tmp : parts)
        levels.insert(Fmi::stoi(tmp));
    }

    opt = Spine::optional_string(theReq.getParameter("pressure"), "");
    if (!opt.empty())
      pressures.insert(Fmi::stod(opt));

    opt = Spine::optional_string(theReq.getParameter("pressures"), "");
    if (!opt.empty())
    {
      vector<string> parts;
      boost::algorithm::split(parts, opt, boost::algorithm::is_any_of(","));
      for (const string& tmp : parts)
        pressures.insert(Fmi::stod(tmp));
    }

    opt = Spine::optional_string(theReq.getParameter("height"), "");
    if (!opt.empty())
      heights.insert(Fmi::stod(opt));

    opt = Spine::optional_string(theReq.getParameter("heights"), "");
    if (!opt.empty())
    {
      vector<string> parts;
      boost::algorithm::split(parts, opt, boost::algorithm::is_any_of(","));
      for (const string& tmp : parts)
        heights.insert(Fmi::stod(tmp));
    }
  }
  catch (...)
  {
    throw Fmi::Exception::Trace(BCP, "Operation failed!");
  }
}

void CommonQuery::parse_precision(const Spine::HTTP::Request& req, const Config& config)
{
  try
  {
    string precname =
        Spine::optional_string(req.getParameter("precision"), config.defaultPrecision());

    const Precision& prec = config.getPrecision(precname);

    precisions.reserve(poptions.size());

    for (const TS::OptionParsers::ParameterList::value_type& p : poptions.parameters())
    {
      const std::string param_name(p.name());
      precisions.push_back(prec.get_precision(param_name, is_timeseries_query));
    }
  }
  catch (...)
  {
    throw Fmi::Exception::Trace(BCP, "Operation failed!");
  }
}

void CommonQuery::parse_aggregation_intervals(const Spine::HTTP::Request& theReq)
{
  try
  {
    std::string aggregationIntervalStringBehind =
        Spine::optional_string(theReq.getParameter("interval"), "0m");
    std::string aggregationIntervalStringAhead = ("0m");

    auto agg_pos = aggregationIntervalStringBehind.find(':');
    if (agg_pos != string::npos)
    {
      aggregationIntervalStringAhead = aggregationIntervalStringBehind.substr(agg_pos + 1);
      aggregationIntervalStringBehind.resize(agg_pos);
    }

    int agg_interval_behind = Spine::duration_string_to_minutes(aggregationIntervalStringBehind);
    int agg_interval_ahead = Spine::duration_string_to_minutes(aggregationIntervalStringAhead);

    if (agg_interval_behind < 0 || agg_interval_ahead < 0)
      throw Fmi::Exception(BCP, "The 'interval' option must be positive!");

    for (const TS::ParameterAndFunctions& paramfuncs : poptions.parameterFunctions())
    {
      set_agg_interval_behind(const_cast<TS::DataFunction&>(paramfuncs.functions.innerFunction),
                              agg_interval_behind);
      set_agg_interval_ahead(const_cast<TS::DataFunction&>(paramfuncs.functions.innerFunction),
                             agg_interval_ahead);
      set_agg_interval_behind(const_cast<TS::DataFunction&>(paramfuncs.functions.outerFunction),
                              agg_interval_behind);
      set_agg_interval_ahead(const_cast<TS::DataFunction&>(paramfuncs.functions.outerFunction),
                             agg_interval_ahead);
    }

    for (const TS::ParameterAndFunctions& paramfuncs : poptions.parameterFunctions())
    {
      std::string paramname(paramfuncs.parameter.name());

      if (maxAggregationIntervals.find(paramname) == maxAggregationIntervals.end())
        maxAggregationIntervals.insert(make_pair(paramname, AggregationInterval(0, 0)));

      set_max_agg_interval_behind(const_cast<TS::DataFunction&>(paramfuncs.functions.innerFunction),
                                  maxAggregationIntervals[paramname].behind);
      set_max_agg_interval_ahead(const_cast<TS::DataFunction&>(paramfuncs.functions.innerFunction),
                                 maxAggregationIntervals[paramname].ahead);
      set_max_agg_interval_behind(const_cast<TS::DataFunction&>(paramfuncs.functions.outerFunction),
                                  maxAggregationIntervals[paramname].behind);
      set_max_agg_interval_ahead(const_cast<TS::DataFunction&>(paramfuncs.functions.outerFunction),
                                 maxAggregationIntervals[paramname].ahead);

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

void CommonQuery::parse_parameters(const Spine::HTTP::Request& theReq)
{
  try
  {
    string opt =
        Spine::required_string(theReq.getParameter("param"), "The 'param' option is required!");

    if (opt.empty())
      throw Fmi::Exception(BCP, "The 'param' option is empty!");

    using Names = list<string>;
    Names tmpNames;
    boost::algorithm::split(tmpNames, opt, boost::algorithm::is_any_of(","));

    remove_duplicate_parameters(tmpNames);

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

      for (const auto& tmpName : tmpNames)
      {
        std::string alias;
        if (itsAliasFileCollectionPtr->getAlias(tmpName, alias))
        {
          Names tmp;
          boost::algorithm::split(tmp, alias, boost::algorithm::is_any_of(","));
          for (const auto& tt : tmp)
            names.push_back(tt);
          ind = true;
        }
        else if (itsAliasFileCollectionPtr->replaceAlias(tmpName, alias))
        {
          Names tmp;
          boost::algorithm::split(tmp, alias, boost::algorithm::is_any_of(","));
          for (const auto& tt : tmp)
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

    for (const string& paramname : names)
    {
      try
      {
        TS::ParameterAndFunctions paramfuncs =
            TS::ParameterFactory::instance().parseNameAndFunctions(paramname, true);
        poptions.add(paramfuncs.parameter, paramfuncs.functions);
      }
      catch (...)
      {
        throw Fmi::Exception(BCP, "Parameter parsing failed for '" + paramname + "'!", nullptr);
      }
    }
    poptions.expandParameter("data_source");

    parse_aggregation_intervals(theReq);
  }
  catch (...)
  {
    throw Fmi::Exception::Trace(BCP, "Operation failed!");
  }
}

void CommonQuery::parse_attr(const Spine::HTTP::Request& theReq)
{
  try
  {
    string attr = Spine::optional_string(theReq.getParameter("attr"), "");
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
      for (const auto& part : partList)
      {
        std::vector<std::string> list;
        splitString(part, ':', list);
        if (list.size() == 2)
          attributeList.addAttribute(list[0], list[1]);
      }
    }
  }
  catch (...)
  {
    throw Fmi::Exception::Trace(BCP, "Operation failed!");
  }
}

bool CommonQuery::parse_grib_loptions(const State& state)
{
  try
  {
    T::Attribute* v1 = attributeList.getAttributeByNameEnd("Grib1.IndicatorSection.EditionNumber");
    T::Attribute* v2 = attributeList.getAttributeByNameEnd("Grib2.IndicatorSection.EditionNumber");
    T::Attribute* lat = attributeList.getAttributeByNameEnd("LatitudeOfFirstGridPoint");
    T::Attribute* lon = attributeList.getAttributeByNameEnd("LongitudeOfFirstGridPoint");

    if (v1 != nullptr && lat != nullptr && lon != nullptr)
    {
      double latitude = toDouble(lat->mValue.c_str()) / 1000;
      double longitude = toDouble(lon->mValue.c_str()) / 1000;
      std::string val = std::to_string(latitude) + "," + std::to_string(longitude);
      Spine::HTTP::Request tmpReq;
      tmpReq.addParameter("latlon", val);
      loptions = std::make_shared<Engine::Geonames::LocationOptions>(
          state.getGeoEngine().parseLocations(tmpReq));
      return true;
    }

    if (v2 != nullptr && lat != nullptr && lon != nullptr)
    {
      double latitude = toDouble(lat->mValue.c_str()) / 1000000;
      double longitude = toDouble(lon->mValue.c_str()) / 1000000;
      std::string val = std::to_string(latitude) + "," + std::to_string(longitude);
      Spine::HTTP::Request tmpReq;
      tmpReq.addParameter("latlon", val);
      loptions = std::make_shared<Engine::Geonames::LocationOptions>(
          state.getGeoEngine().parseLocations(tmpReq));
      return true;
    }

    return false;
  }
  catch (...)
  {
    throw Fmi::Exception::Trace(BCP, "Operation failed!");
  }
}

void CommonQuery::parse_inkeyword_locations(const Spine::HTTP::Request& theReq,
                                            const State& state)
{
  try
  {
    auto searchName = theReq.getParameterList("inkeyword");
    if (!searchName.empty())
    {
      for (const std::string& key : searchName)
      {
        Locus::QueryOptions opts;
        opts.SetLanguage(language);
        Spine::LocationList places = state.getGeoEngine().keywordSearch(opts, key);
        if (places.empty())
          throw Fmi::Exception(BCP, "No locations for keyword " + std::string(key) + " found");
        inKeywordLocations.insert(inKeywordLocations.end(), places.begin(), places.end());
      }
    }
  }
  catch (...)
  {
    throw Fmi::Exception::Trace(BCP, "Operation failed!");
  }
}

void CommonQuery::parse_origintime(const Spine::HTTP::Request& theReq)
{
  try
  {
    std::optional<std::string> tmp = theReq.getParameter("origintime");
    if (tmp)
    {
      if (*tmp == "latest" || *tmp == "newest")
        origintime = Fmi::DateTime(Fmi::DateTime::POS_INFINITY);
      else if (*tmp == "oldest")
        origintime = Fmi::DateTime(Fmi::DateTime::NEG_INFINITY);
      else
        origintime = Fmi::TimeParser::parse(*tmp);
    }
  }
  catch (...)
  {
    throw Fmi::Exception::Trace(BCP, "Operation failed!");
  }
}

double CommonQuery::maxdistance_kilometers() const
{
  return Fmi::DistanceParser::parse_kilometer(maxdistance);
}

double CommonQuery::maxdistance_meters() const
{
  return Fmi::DistanceParser::parse_meter(maxdistance);
}

void CommonQuery::remove_duplicate_parameters(std::list<std::string>&) const
{
  // FIXME: leave empty for base class for now. EDR did not implement this, but timeseries did
}

}  // namespace EDR
}  // namespace Plugin
}  // namespace SmartMet

// ======================================================================
