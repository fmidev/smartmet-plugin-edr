// ======================================================================
/*!
 * \brief Query parameters common to both EDR and TimeSeries request styles
 */
// ======================================================================

#pragma once

#include "AggregationInterval.h"
#include "ObsQueryParams.h"
#include "Producers.h"
#include <engines/geonames/Engine.h>
#include <engines/geonames/WktGeometry.h>
#include <grid-content/queryServer/definition/AliasFileCollection.h>
#include <grid-files/common/AttributeList.h>
#include <timeseries/OptionParsers.h>
#include <timeseries/TimeSeriesInclude.h>
#include <functional>

namespace SmartMet
{
namespace Plugin
{
namespace EDR
{
class Config;

class CommonQuery : public ObsQueryParams
{
 public:
  CommonQuery() = delete;
  CommonQuery(const State& state, const Spine::HTTP::Request& req, Config& config);

  using ParamPrecisions = std::vector<int>;
  using Levels = std::set<int>;
  using Pressures = std::set<double>;
  using Heights = std::set<double>;

  // DO NOT FORGET TO CHANGE hash_value IF YOU ADD ANY NEW PARAMETERS

  double step = 0;

  std::size_t startrow = 0;    // Paging; first (0-) row to return; default 0
  std::size_t maxresults = 0;  // max rows to return (page length); default 0 (all)

  std::string wmo;
  std::string fmisid;
  std::string lpnn;
  std::string language;
  std::string keyword;
  std::string timezone;
  std::string leveltype;
  std::string format;
  std::string timeformat;
  std::string timestring;
  std::string localename;
  std::locale outlocale;
  std::string areasource;
  std::string crs;

  Fmi::DateTime latestTimestep;
  std::optional<Fmi::DateTime> origintime;

  TimeProducers timeproducers;
  std::shared_ptr<Engine::Geonames::LocationOptions> loptions;
  std::shared_ptr<Fmi::TimeFormatter> timeformatter;

  // last coordinate used while forming the output
  mutable NFmiPoint lastpoint;

  std::vector<int> weekdays;

  std::vector<int> wmos;
  std::vector<int> lpnns;
  std::vector<int> fmisids;
  std::vector<int> rwsids;
  std::vector<std::string> wsis;

  ParamPrecisions precisions;
  Fmi::ValueFormatter valueformatter;

  Levels levels;
  Pressures pressures;
  Heights heights;
  bool levelRange = false;  // Set if querying level/pressure/height range

  TS::OptionParsers::ParameterOptions poptions;
  MaxAggregationIntervals maxAggregationIntervals;
  Engine::Geonames::WktGeometries wktGeometries;
  TS::TimeSeriesGeneratorOptions toptions;

  bool maxdistanceOptionGiven = false;
  bool findnearestvalidpoint = false;
  bool debug = false;
  bool starttimeOptionGiven = false;
  bool endtimeOptionGiven = false;
  bool useCurrentTime = false;
  bool timeAggregationRequested = false;
  std::string forecastSource;
  T::AttributeList attributeList;

  Spine::LocationList inKeywordLocations;
  bool groupareas{true};

  double maxdistance_kilometers() const;
  double maxdistance_meters() const;
  // DO NOT FORGET TO CHANGE hash_value IF YOU ADD ANY NEW PARAMETERS

 protected:
  void commonInit(const State& state,
                  const Spine::HTTP::Request& req,
                  Config& config,
                  double stepDefault = 1.0,
                  bool setDataTimesMode = false,
                  const std::function<bool(const std::string&)>& extraProducerCheck = nullptr);

  void parse_levels(const Spine::HTTP::Request& theReq);
  void parse_precision(const Spine::HTTP::Request& theReq, const Config& config);
  void parse_producers(const Spine::HTTP::Request& theReq,
                       const State& theState,
                       const std::function<bool(const std::string&)>& extraCheck = nullptr);
  void parse_parameters(const Spine::HTTP::Request& theReq);
  void parse_aggregation_intervals(const Spine::HTTP::Request& theReq);
  void parse_attr(const Spine::HTTP::Request& theReq);
  bool parse_grib_loptions(const State& state);
  void parse_inkeyword_locations(const Spine::HTTP::Request& theReq, const State& state);
  void parse_origintime(const Spine::HTTP::Request& theReq);

  QueryServer::AliasFileCollection* itsAliasFileCollectionPtr = nullptr;
  std::string maxdistance;
};

}  // namespace EDR
}  // namespace Plugin
}  // namespace SmartMet

// ======================================================================
