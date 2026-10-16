#include "ProducerCatalog.h"
#include <macgyver/DateTime.h>
#include <macgyver/Exception.h>
#include <algorithm>
#include <limits>
#include <map>
#include <set>
#include <string>
#include <vector>

namespace SmartMet
{
namespace Plugin
{
namespace EDR
{
namespace ProducerCatalog
{
namespace
{
using JsonValue = ::Json::Value;

std::string isoTime(const Fmi::DateTime& t)
{
  return Fmi::date_time::to_iso_extended_string(t) + "Z";
}

const char* sourceName(SourceEngine source)
{
  switch (source)
  {
    case SourceEngine::Querydata:
      return "querydata";
    case SourceEngine::Grid:
      return "grid";
    case SourceEngine::Observation:
      return "observation";
    default:
      return "";
  }
}

// One producer, possibly combined from several EDR collections
struct Entry
{
  SourceEngine source = SourceEngine::Undefined;
  std::string title;
  std::string description;
  double xmin = std::numeric_limits<double>::max();
  double ymin = std::numeric_limits<double>::max();
  double xmax = std::numeric_limits<double>::lowest();
  double ymax = std::numeric_limits<double>::lowest();
  Fmi::DateTime start;
  Fmi::DateTime end;
  Fmi::DateTime origin;
  int step = 0;
  std::map<std::string, std::set<std::string>> levels;  // level type -> values
  std::set<std::string> parameters;
  std::size_t stations = 0;

  void add(const EDRMetaData& md)
  {
    if (title.empty())
    {
      const auto* info = md.collection_info;
      if (info != nullptr && !info->title.empty())
      {
        title = info->title;
        description = info->description;
      }
      else if (!md.isGridProducer())
      {
        // The engine titles of grid collections name one geometry and level of the producer
        title = md.collection_info_engine.title;
        description = md.collection_info_engine.description;
      }
    }

    const auto& s = md.spatial_extent;
    if (s.bbox_xmin < s.bbox_xmax || s.bbox_ymin < s.bbox_ymax)
    {
      xmin = std::min(xmin, s.bbox_xmin);
      ymin = std::min(ymin, s.bbox_ymin);
      xmax = std::max(xmax, s.bbox_xmax);
      ymax = std::max(ymax, s.bbox_ymax);
    }

    const auto& t = md.temporal_extent;
    for (const auto* periods : {&t.time_periods, &t.single_time_periods})
      for (const auto& p : *periods)
      {
        if (!p.start_time.is_not_a_date_time() &&
            (start.is_not_a_date_time() || p.start_time < start))
          start = p.start_time;
        if (!p.end_time.is_not_a_date_time() && (end.is_not_a_date_time() || p.end_time > end))
          end = p.end_time;
        if (p.timestep > 0 && (step == 0 || p.timestep < step))
          step = p.timestep;
      }
    if (!t.origin_time.is_not_a_date_time() &&
        (origin.is_not_a_date_time() || t.origin_time > origin))
      origin = t.origin_time;

    const auto& v = md.vertical_extent;
    if (!v.levels.empty())
      levels[v.level_type].insert(v.levels.begin(), v.levels.end());

    parameters.insert(md.parameter_names.begin(), md.parameter_names.end());
    stations = std::max(stations, md.stationMetaData.size());
  }
};

// The TimeSeries name of an EDR collection: grid collections are named producer.geometry.level,
// and the producer name may contain dots itself
std::string producerName(SourceEngine source, const std::string& collection)
{
  if (source != SourceEngine::Grid)
    return collection;
  auto pos = collection.rfind('.');
  if (pos == std::string::npos || pos == 0)
    return collection;
  pos = collection.rfind('.', pos - 1);
  return (pos == std::string::npos ? collection : collection.substr(0, pos));
}

}  // namespace

JsonValue build(const EngineMetaData& metadata)
{
  try
  {
    std::map<std::pair<std::string, SourceEngine>, Entry> entries;
    JsonValue dictionary(::Json::objectValue);

    for (const auto& engine_item : metadata.getMetaData())
    {
      const auto source = engine_item.first;
      if (sourceName(source)[0] == '\0')
        continue;

      for (const auto& producer_item : engine_item.second)
      {
        auto& entry = entries[{producerName(source, producer_item.first), source}];
        entry.source = source;
        for (const auto& md : producer_item.second)
        {
          entry.add(md);
          // Only what adds to the name: most observation parameters have no description
          for (const auto& p : md.parameters)
          {
            if (dictionary.isMember(p.first))
              continue;
            JsonValue info(::Json::objectValue);
            if (p.second.name != p.first)
              info["name"] = p.second.name;
            const auto& desc = p.second.description;
            if (!desc.empty() && desc != p.first && desc != p.second.name)
              info["description"] = desc;
            if (!p.second.unit.empty())
              info["unit"] = p.second.unit;
            if (info.size() > 0)
              dictionary[p.first] = info;
          }
        }
      }
    }

    JsonValue producers(::Json::arrayValue);
    for (const auto& item : entries)
    {
      const auto& e = item.second;
      JsonValue p;
      p["name"] = item.first.first;
      p["source"] = sourceName(e.source);
      if (!e.title.empty())
        p["title"] = e.title;
      if (!e.description.empty())
        p["description"] = e.description;
      if (e.xmin <= e.xmax)
        for (double v : {e.xmin, e.ymin, e.xmax, e.ymax})
          p["bbox"].append(v);
      if (!e.start.is_not_a_date_time())
        p["time"]["start"] = isoTime(e.start);
      if (!e.end.is_not_a_date_time())
        p["time"]["end"] = isoTime(e.end);
      if (e.step > 0)
        p["time"]["step"] = e.step;
      if (!e.origin.is_not_a_date_time())
        p["time"]["origintime"] = isoTime(e.origin);
      for (const auto& level : e.levels)
      {
        JsonValue l;
        l["type"] = level.first;
        // Numeric order where possible
        std::vector<std::string> values(level.second.begin(), level.second.end());
        std::sort(values.begin(),
                  values.end(),
                  [](const std::string& a, const std::string& b)
                  {
                    try
                    {
                      return std::stod(a) < std::stod(b);
                    }
                    catch (...)
                    {
                      return a < b;
                    }
                  });
        for (const auto& v : values)
          l["values"].append(v);
        p["levels"].append(l);
      }
      p["parameters"] = JsonValue(::Json::arrayValue);
      for (const auto& name : e.parameters)
        p["parameters"].append(name);
      if (e.stations > 0)
        p["stations"] = static_cast<::Json::UInt64>(e.stations);
      producers.append(p);
    }

    JsonValue doc;
    doc["producers"] = producers;
    doc["parameters"] = dictionary;
    return doc;
  }
  catch (...)
  {
    throw Fmi::Exception::Trace(BCP, "Failed to build the producer catalog");
  }
}

JsonValue filter(const JsonValue& catalog, const std::string& source)
{
  try
  {
    JsonValue doc;
    doc["producers"] = JsonValue(::Json::arrayValue);
    doc["parameters"] = JsonValue(::Json::objectValue);
    const auto& dictionary = catalog["parameters"];
    for (const auto& producer : catalog["producers"])
    {
      if (producer["source"].asString() != source)
        continue;
      doc["producers"].append(producer);
      for (const auto& name : producer["parameters"])
      {
        const auto key = name.asString();
        if (dictionary.isMember(key))
          doc["parameters"][key] = dictionary[key];
      }
    }
    return doc;
  }
  catch (...)
  {
    throw Fmi::Exception::Trace(BCP, "Failed to filter the producer catalog");
  }
}

const std::vector<std::string>& sources()
{
  static const std::vector<std::string> names{"querydata", "grid", "observation"};
  return names;
}

}  // namespace ProducerCatalog
}  // namespace EDR
}  // namespace Plugin
}  // namespace SmartMet
