#include "UtilityFunctions.h"
#include "State.h"
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
      auto timeseries = TS::TimeSeries();
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

    if (std::get_if<int>(&val) != nullptr)
      ret = *(std::get_if<int>(&val));
    else if (std::get_if<double>(&val) != nullptr)
      ret = *(std::get_if<double>(&val));
    else if (std::get_if<std::string>(&val) != nullptr)
    {
      std::string value = *(std::get_if<std::string>(&val));

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

    if (std::get_if<int>(&val) != nullptr)
      ret = *(std::get_if<int>(&val));
    else if (std::get_if<double>(&val) != nullptr)
      ret = *(std::get_if<double>(&val));

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

    if (std::get_if<int>(&val) != nullptr)
      ret = Fmi::to_string(*(std::get_if<int>(&val)));
    else if (std::get_if<double>(&val) != nullptr)
      ret = Fmi::to_string(*(std::get_if<double>(&val)));
    else if (std::get_if<std::string>(&val) != nullptr)
      ret = *(std::get_if<std::string>(&val));

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
      return {double_value, precision};

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

//
// Parse range/list/repeat/value. If isRange is set (cube query), only value range (lo/hi)
// is accepted. Values are validated by converting to double. Throws on invalid or missing values.
//
// - if value range is given, isRange is set to true and the range values are set to loValue/hiValue
// - if value list is given, first/last value are set to loValue/hiValue
// - if repeating interval (Rn/start/step) is given, the generated value list is set to valueStr
//   and first/last value are set to loValue/hiValue
// - single value is set to loValue
//
// Returns true if nonempty valueStr is given.
//
bool parseRangeListValue(std::string& valueStr,
                         bool& isRange,
                         std::string& loValue,
                         std::string& hiValue)
{
  const int maxRepeat = 999;
  auto onlyRange = isRange;

  try
  {
    isRange = false;
    loValue.clear();
    hiValue.clear();

    auto v = boost::algorithm::trim_copy(valueStr);
    if (v.empty())
      return false;

    bool isList = false;

    std::vector<std::string> parts, listParts;
    boost::algorithm::split(parts, v, boost::algorithm::is_any_of("/"));

    if (onlyRange && (parts.size() != 2))
      throw Fmi::Exception(BCP, "");

    if (parts.size() == 1)
    {
      std::vector<std::string> listParts;
      boost::algorithm::split(listParts, parts[0], boost::algorithm::is_any_of(","));

      if (listParts.size() == 1)
      {
        loValue = boost::algorithm::trim_copy(parts[0]);
        Fmi::stod(loValue);

        return true;
      }

      parts.swap(listParts);
      isList = true;
    }

    if (!isList)
    {
      int repeat = 0;

      if (parts.size() == 3)
      {
        // Repeating interval Rn/start/step

        if (toupper(parts[0].front()) != 'R')
          throw Fmi::Exception(BCP, "");

        parts[0].erase(parts[0].begin());
        boost::algorithm::trim(parts[0]);
        repeat = Fmi::stoi(parts[0]);

        if (repeat <= 0)
          throw Fmi::Exception(BCP, "");

        parts.erase(parts.begin());
      }

      if (parts.size() != 2)
        throw Fmi::Exception(BCP, "");

      loValue = boost::algorithm::trim_copy(parts[0]);
      hiValue = boost::algorithm::trim_copy(parts[1]);

      if (
          loValue.empty() || hiValue.empty() || (repeat > maxRepeat) ||
          ((repeat == 0) && (Fmi::stod(loValue) > Fmi::stod(hiValue)))
         )
        throw Fmi::Exception(BCP, "");

      if (repeat > 0)
      {
        auto z = Fmi::stod(loValue);
        auto v = z;
        auto step = Fmi::stod(hiValue);

        valueStr.clear();

        for (int r = 0; (r < repeat); r++, z += step)
        {
          v = z;
          valueStr += (Fmi::to_string(z) + ",");
        }

        valueStr.pop_back();

        hiValue = Fmi::to_string(v);
      }

      isRange = (repeat == 0);

      return true;
    }

    for (auto& part : parts)
    {
      boost::algorithm::trim(part);
      if (part.empty())
        throw Fmi::Exception(BCP, "");

      Fmi::stod(part);
    }

    loValue = parts[0];
    hiValue = parts[parts.size() - 1];

    return true;
  }
  catch (...)
  {
    if (onlyRange)
      throw Fmi::Exception(BCP, "Invalid z parameter, numeric min/max range expected");
    else
      throw Fmi::Exception(
          BCP,
          "Invalid z parameter, numeric value, list of values, repeating interval "
          "or min/max range expected");
  }
}

//
// Parses 2d (blx,bly,trx,try) or 3d (blx,bly,blz,trx,try,trz) bbox and z -coordinate
// range/list/value (lo/hi or value[,value,...]).
//
// If bbox is 3d and z is empty, zIsRange is set to true and bbox lo/hi is set to zLo/zHi.
//
std::vector<std::string> parseBBoxAndZ(const std::string& bbox,
                                       std::string& z,
                                       bool& zIsRange,
                                       std::string& zLo,
                                       std::string& zHi)
{
  bool hasZ = parseRangeListValue(z, zIsRange, zLo, zHi);

  try
  {
    if (boost::algorithm::trim_copy(bbox).empty())
      return {};

    std::vector<std::string> parts;
    boost::algorithm::split(parts, bbox, boost::algorithm::is_any_of(","));

    if ((parts.size() != 4) && (parts.size() != 6))
      throw Fmi::Exception(BCP, "");

    for (auto& part : parts)
    {
      boost::algorithm::trim(part);
      if (part.empty())
        throw Fmi::Exception(BCP, "");

      Fmi::stod(part);
    }

    if ((!hasZ) && (parts.size() == 6))
    {
      zLo = parts[2];
      zHi = parts[5];
      zIsRange = true;
    }

    return parts;
  }
  catch (...)
  {
    throw Fmi::Exception(
        BCP,
        "Invalid bbox parameter, format is bbox=lower left corner x,lower left corner y"
        "[,lower left corner z],upper right corner x,upper right corner y"
        "[,upper right corner z], e.g. bbox=19.4,59.6,31.6,70.1");
  }
}

//
// Parses 2d (blx,bly,trx,try) or 3d (blx,bly,blz,trx,try,trz) bbox.
// Returns 2d bbox if parse2d is set.
//
std::vector<std::string> parseBBox(const std::string& bbox, bool parse2d)
{
  try
  {
    std::string z, zLo, zHi;
    bool zIsRange = false;

    auto parts = parseBBoxAndZ(bbox, z, zIsRange, zLo, zHi);

    if (parse2d && (parts.size() == 6))
    {
      parts.erase(parts.begin() + 2);
      parts.pop_back();
    }

    return parts;
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
