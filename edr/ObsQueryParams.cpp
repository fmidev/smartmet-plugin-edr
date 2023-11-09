
#include "ObsQueryParams.h"
#include <boost/algorithm/string/split.hpp>
#include <macgyver/StringConversion.h>
#include <spine/Convenience.h>
#ifndef WITHOUT_OBSERVATION
#include <engines/observation/Engine.h>
#endif

using namespace std;

namespace SmartMet
{
namespace Plugin
{
namespace EDR
{
namespace
{

void add_sql_data_filter(const Spine::HTTP::Request& req,
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

}  // namespace

ObsQueryParams::ObsQueryParams(const Spine::HTTP::Request& req)
{
  try
  {
#ifndef WITHOUT_OBSERVATION
    std::string endtime = Spine::optional_string(req.getParameter("endtime"), "");
    latestObservation = (endtime == "now");
    allplaces = (Spine::optional_string(req.getParameter("places"), "") == "all");
    // observation params
    numberofstations = Spine::optional_int(req.getParameter("numberofstations"), 1);
    std::string opt_stationgroups = Spine::optional_string(req.getParameter("stationgroups"), "");
    if (!opt_stationgroups.empty())
      boost::algorithm::split(stationgroups, opt_stationgroups, boost::algorithm::is_any_of(","));

    if (!!req.getParameter("bbox"))
    {
      string bbox = *req.getParameter("bbox");
      vector<string> parts;
      boost::algorithm::split(parts, bbox, boost::algorithm::is_any_of(","));
      std::string lat2(parts[3]);
      auto radius_pos = lat2.find(':');
      if (radius_pos != string::npos)
        lat2.resize(radius_pos);

      // Bounding box must contain exactly 4 elements
      if (parts.size() != 4)
      {
        throw Fmi::Exception(BCP, "Invalid bounding box '" + bbox + "'!");
      }

      if (!parts[0].empty())
        boundingBox["minx"] = Fmi::stod(parts[0]);

      if (!parts[1].empty())
        boundingBox["miny"] = Fmi::stod(parts[1]);

      if (!parts[2].empty())
        boundingBox["maxx"] = Fmi::stod(parts[2]);

      if (!parts[3].empty())
        boundingBox["maxy"] = Fmi::stod(lat2);
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
  }
  catch (...)
  {
    // Stack traces in journals are useless when the user has made a typo
    throw Fmi::Exception::Trace(BCP, "TimeSeries plugin failed to parse query string options!")
        .disableLogging();
  }
}

std::set<std::string> ObsQueryParams::getObsProducers(const State& state) const
{
  try
  {
    std::set<std::string> ret;
#ifndef WITHOUT_OBSERVATION
    auto* obsengine = state.getObsEngine();
    if (obsengine != nullptr)
      ret = obsengine->getValidStationTypes();
#endif
    return ret;
  }
  catch (...)
  {
    throw Fmi::Exception::Trace(BCP, "Operation failed!");
  }
}

}  // namespace EDR
}  // namespace Plugin
}  // namespace SmartMet

// ======================================================================
