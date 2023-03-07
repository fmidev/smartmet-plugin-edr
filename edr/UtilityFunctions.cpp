#include "UtilityFunctions.h"
#include <boost/date_time/gregorian/gregorian.hpp>
#include <boost/date_time/posix_time/posix_time.hpp>

namespace SmartMet
{
namespace Plugin
{
namespace EDR
{
namespace UtilityFunctions
{
// ----------------------------------------------------------------------
/*!
 * \brief
 */
// ----------------------------------------------------------------------

void update_latest_timestep(Query &query, const TS::TimeSeriesVectorPtr &tsv)
{
  try
  {
    if (tsv->empty())
      return;

    // Sometimes some of the timeseries are empty. However, should we iterate
    // backwards until we find a valid one instead of exiting??

    if (tsv->back().empty())
      return;

    const auto &last_time = tsv->back().back().time;

    if (query.toptions.startTimeUTC)
      query.latestTimestep = last_time.utc_time();
    else
      query.latestTimestep = last_time.local_time();
  }
  catch (...)
  {
    throw Fmi::Exception::Trace(BCP, "Operation failed!");
  }
}

// ----------------------------------------------------------------------
/*!
 * \brief
 */
// ----------------------------------------------------------------------

void update_latest_timestep(Query &query, const TS::TimeSeries &ts)
{
  try
  {
    if (ts.empty())
      return;

    const auto &last_time = ts[ts.size() - 1].time;

    if (query.toptions.startTimeUTC)
      query.latestTimestep = last_time.utc_time();
    else
      query.latestTimestep = last_time.local_time();
  }
  catch (...)
  {
    throw Fmi::Exception::Trace(BCP, "Operation failed!");
  }
}

// ----------------------------------------------------------------------
/*!
 * \brief
 */
// ----------------------------------------------------------------------

void update_latest_timestep(Query &query, const TS::TimeSeriesGroup &tsg)
{
  try
  {
    // take first time series and last timestep thereof
    TS::TimeSeries ts = tsg[0].timeseries;

    if (ts.empty())
      return;

    // update the latest timestep, so that next query (is exists) knows from
    // where to continue
    update_latest_timestep(query, ts);
  }
  catch (...)
  {
    throw Fmi::Exception::Trace(BCP, "Operation failed!");
  }
}

// ----------------------------------------------------------------------
/*!
 * \brief
 */
// ----------------------------------------------------------------------

void store_data(TS::TimeSeriesVectorPtr aggregatedData, Query &query, TS::OutputData &outputData)
{
  try
  {
    if (aggregatedData->empty())
      return;

    // insert data to the end
    std::vector<TS::TimeSeriesData> &odata = (--outputData.end())->second;
    odata.emplace_back(TS::TimeSeriesData(aggregatedData));
    update_latest_timestep(query, aggregatedData);
  }
  catch (...)
  {
    throw Fmi::Exception::Trace(BCP, "Operation failed!");
  }
}

// ----------------------------------------------------------------------
/*!
 * \brief
 */
// ----------------------------------------------------------------------

void store_data(std::vector<TS::TimeSeriesData> &aggregatedData,
                Query &query,
                TS::OutputData &outputData)
{
  try
  {
    if (aggregatedData.empty())
      return;

    TS::TimeSeriesData tsdata;
    if (boost::get<TS::TimeSeriesPtr>(&aggregatedData[0]))
    {
      TS::TimeSeriesPtr ts_first = *(boost::get<TS::TimeSeriesPtr>(&aggregatedData[0]));
      TS::TimeSeriesPtr ts_result(new TS::TimeSeries(ts_first->getLocalTimePool()));
      // first merge timeseries of all levels of one parameter
      for (const auto &data : aggregatedData)
      {
        TS::TimeSeriesPtr ts = *(boost::get<TS::TimeSeriesPtr>(&data));
        ts_result->insert(ts_result->end(), ts->begin(), ts->end());
      }
      // update the latest timestep, so that next query (if exists) knows from
      // where to continue
      update_latest_timestep(query, *ts_result);
      tsdata = ts_result;
    }
    else if (boost::get<TS::TimeSeriesGroupPtr>(&aggregatedData[0]))
    {
      TS::TimeSeriesGroupPtr tsg_result(new TS::TimeSeriesGroup);
      // first merge timeseries of all levels of one parameter
      for (const auto &data : aggregatedData)
      {
        TS::TimeSeriesGroupPtr tsg = *(boost::get<TS::TimeSeriesGroupPtr>(&data));
        tsg_result->insert(tsg_result->end(), tsg->begin(), tsg->end());
      }
      // update the latest timestep, so that next query (if exists) knows from
      // where to continue
      update_latest_timestep(query, *tsg_result);
      tsdata = tsg_result;
    }

    // insert data to the end
    std::vector<TS::TimeSeriesData> &odata = (--outputData.end())->second;
    odata.push_back(tsdata);
  }
  catch (...)
  {
    throw Fmi::Exception::Trace(BCP, "Operation failed!");
  }
}

double get_double(const TS::Value &val, double default_value = kFloatMissing)
{
  try
  {
    double ret = default_value;

    if (boost::get<int>(&val) != nullptr)
      ret = *(boost::get<int>(&val));
    else if (boost::get<double>(&val) != nullptr)
      ret = *(boost::get<double>(&val));

    return ret;
  }
  catch (...)
  {
    throw Fmi::Exception::Trace(BCP, "Operation failed!");
  }
}

int get_int(const TS::Value &val, int default_value = kFloatMissing)
{
  try
  {
    int ret = default_value;

    if (boost::get<int>(&val) != nullptr)
      ret = *(boost::get<int>(&val));
    else if (boost::get<double>(&val) != nullptr)
      ret = *(boost::get<double>(&val));

    return ret;
  }
  catch (...)
  {
    throw Fmi::Exception::Trace(BCP, "Operation failed!");
  }
}

std::string get_string(const TS::Value &val, const std::string &default_value = "")
{
  try
  {
    std::string ret = default_value;

    if (boost::get<int>(&val) != nullptr)
      ret = Fmi::to_string(*(boost::get<int>(&val)));
    else if (boost::get<double>(&val) != nullptr)
      ret = Fmi::to_string(*(boost::get<double>(&val)));
    else if (boost::get<std::string>(&val) != nullptr)
      ret = *(boost::get<std::string>(&val));

    return ret;
  }
  catch (...)
  {
    throw Fmi::Exception::Trace(BCP, "Operation failed!");
  }
}

Json::Value json_value(const TS::Value &val, int precision)
{
  auto double_value = get_double(val);
  if (double_value != kFloatMissing)
    return Json::Value(double_value, precision);

  auto int_value = get_int(val);
  if (int_value != static_cast<int>(kFloatMissing))
    return Json::Value(int_value);

  // If value is of type string empty string is returned
  auto string_value = get_string(val);
  return Json::Value(string_value);
}

}  // namespace UtilityFunctions
}  // namespace EDR
}  // namespace Plugin
}  // namespace SmartMet
