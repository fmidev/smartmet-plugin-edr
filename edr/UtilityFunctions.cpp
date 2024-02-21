#include "UtilityFunctions.h"
#include "State.h"
#include <boost/date_time/gregorian/gregorian.hpp>
#include <boost/date_time/posix_time/posix_time.hpp>
#include <engines/observation/ExternalAndMobileProducerId.h>
#include <engines/observation/Keywords.h>
#include <timeseries/ParameterKeywords.h>
#include <timeseries/ParameterTools.h>

namespace SmartMet
{
namespace Plugin
{
namespace EDR
{
namespace UtilityFunctions
{

bool is_special_parameter(const std::string& paramname)
{
  try
  {
    if (paramname == ORIGINTIME_PARAM || paramname == LEVEL_PARAM)
      return false;

    return (TS::is_time_parameter(paramname) || TS::is_location_parameter(paramname));
  }
  catch (...)
  {
    throw Fmi::Exception(BCP, "Operation failed!", nullptr);
  }
}

TS::Value get_location_parameter_value(const Spine::LocationPtr& loc,
                                       const std::string& paramname,
                                       const Query& query,
                                       int precision)
{
  try
  {
    if (paramname == "longitude")
      return loc->longitude;
    else if (paramname == "latitude")
      return loc->latitude;

    return TS::location_parameter(
        loc, paramname, query.valueformatter, query.timezone, precision, query.crs);
  }
  catch (...)
  {
    throw Fmi::Exception(BCP, "Operation failed!", nullptr);
  }
}

void get_special_parameter_values(const std::string& paramname,
                                  int precision,
                                  const TS::TimeSeriesGenerator::LocalTimeList& tlist,
                                  const Spine::LocationPtr& loc,
                                  const Query& query,
                                  const State& state,
                                  const Fmi::TimeZones& timezones,
                                  TS::TimeSeriesPtr& result)
{
  try
  {
    bool is_time_parameter = TS::is_time_parameter(paramname);
    bool is_location_parameter = TS::is_location_parameter(paramname);

    for (const auto& timestep : tlist)
    {
      if (is_time_parameter)
      {
        TS::Value value = TS::time_parameter(paramname,
                                             timestep,
                                             state.getTime(),
                                             *loc,
                                             query.timezone,
                                             timezones,
                                             query.outlocale,
                                             *query.timeformatter,
                                             query.timestring);
        result->emplace_back(TS::TimedValue(timestep, value));
      }
      if (is_location_parameter)
      {
        TS::Value value = get_location_parameter_value(loc, paramname, query, precision);
        result->emplace_back(TS::TimedValue(timestep, value));
      }
    }
  }
  catch (...)
  {
    throw Fmi::Exception(BCP, "Operation failed!", nullptr);
  }
}

void get_special_parameter_values(const std::string& paramname,
                                  int precision,
                                  const TS::TimeSeriesGenerator::LocalTimeList& tlist,
                                  const Spine::LocationList& llist,
                                  const Query& query,
                                  const State& state,
                                  const Fmi::TimeZones& timezones,
                                  TS::TimeSeriesGroupPtr& result)
{
  try
  {
    bool is_time_parameter = TS::is_time_parameter(paramname);
    bool is_location_parameter = TS::is_location_parameter(paramname);
    for (const auto& loc : llist)
    {
      auto timeseries = TS::TimeSeries(state.getLocalTimePool());
      for (const auto& timestep : tlist)
      {
        if (is_time_parameter)
        {
          TS::Value value = TS::time_parameter(paramname,
                                               timestep,
                                               state.getTime(),
                                               *loc,
                                               query.timezone,
                                               timezones,
                                               query.outlocale,
                                               *query.timeformatter,
                                               query.timestring);
          timeseries.emplace_back(TS::TimedValue(timestep, value));
        }
        if (is_location_parameter)
        {
          TS::Value value = get_location_parameter_value(loc, paramname, query, precision);
          timeseries.emplace_back(TS::TimedValue(timestep, value));
        }
      }
      if (!timeseries.empty())
        result->push_back(
            TS::LonLatTimeSeries(Spine::LonLat(loc->longitude, loc->latitude), timeseries));
    }
  }
  catch (...)
  {
    throw Fmi::Exception(BCP, "Operation failed!", nullptr);
  }
}

bool is_mobile_producer(const std::string& producer)
{
  try
  {
    return (producer == Engine::Observation::ROADCLOUD_PRODUCER ||
            producer == Engine::Observation::TECONER_PRODUCER ||
            producer == Engine::Observation::FMI_IOT_PRODUCER ||
            producer == Engine::Observation::NETATMO_PRODUCER);
  }
  catch (...)
  {
    throw Fmi::Exception(BCP, "Operation failed!", nullptr);
  }
}

bool is_flash_producer(const std::string& producer)
{
  try
  {
    return (producer == FLASH_PRODUCER);
  }
  catch (...)
  {
    throw Fmi::Exception(BCP, "Operation failed!", nullptr);
  }
}

bool is_icebuoy_or_copernicus_producer(const std::string& producer)
{
  try
  {
    return (producer == ICEBUOY_PRODUCER || producer == COPERNICUS_PRODUCER);
  }
  catch (...)
  {
    throw Fmi::Exception(BCP, "Operation failed!", nullptr);
  }
}

bool is_flash_or_mobile_producer(const std::string& producer)
{
  try
  {
    return (is_flash_producer(producer) || is_mobile_producer(producer) ||
            is_icebuoy_or_copernicus_producer(producer));
  }
  catch (...)
  {
    throw Fmi::Exception(BCP, "Operation failed!", nullptr);
  }
}

double get_double(const TS::Value& val, double default_value)
{
  try
  {
    double ret = default_value;

    if (boost::get<int>(&val) != nullptr)
      ret = *(boost::get<int>(&val));
    else if (boost::get<double>(&val) != nullptr)
      ret = *(boost::get<double>(&val));
    else if (boost::get<std::string>(&val) != nullptr)
    {
      std::string value = *(boost::get<std::string>(&val));

      try
      {
        ret = Fmi::stod(value);
      }
      catch (...)
      {
        ret = default_value;
      }
    }

    return ret;
  }
  catch (...)
  {
    throw Fmi::Exception::Trace(BCP, "Operation failed!");
  }
}

int get_int(const TS::Value& val, int default_value = kFloatMissing)
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

std::string get_string(const TS::Value& val, const std::string& default_value = "")
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

Json::Value json_value(const TS::Value& val, int precision)
{
  try
  {
    auto double_value = get_double(val, kFloatMissing);

    if (double_value != kFloatMissing)
      return {double_value, static_cast<unsigned int>(precision)};

    auto int_value = get_int(val);

    if (int_value != static_cast<int>(kFloatMissing))
      return int_value;

    // If value is of type string empty string is returned
    auto string_value = get_string(val);

    return string_value;
  }
  catch (...)
  {
    throw Fmi::Exception(BCP, "Operation failed!", nullptr);
  }
}

}  // namespace UtilityFunctions
}  // namespace EDR
}  // namespace Plugin
}  // namespace SmartMet
