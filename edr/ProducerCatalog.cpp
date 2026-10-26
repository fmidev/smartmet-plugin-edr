#include "ProducerCatalog.h"
#include <fmt/format.h>
#include <macgyver/DateTime.h>
#include <macgyver/Exception.h>
#include <macgyver/StringConversion.h>
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

// The level types of querydata and grid in one vocabulary, with the unit the TimeSeries levels
// option takes. Grid pressure levels are in Pa, the request takes hPa.
struct LevelKind
{
  const char* type;
  const char* unit;
  double scale;
};

LevelKind levelKind(SourceEngine source, const std::string& type)
{
  static const std::map<std::string, LevelKind> kinds = {
      {"pressurelevel", {"pressure", "hPa", 1}},
      {"pressure", {"pressure", "hPa", 1}},
      {"hybridlevel", {"hybrid", "", 1}},
      {"hybrid", {"hybrid", "", 1}},
      {"heightlevel", {"height", "m", 1}},
      {"height", {"height", "m", 1}},
      {"altitude", {"altitude", "m", 1}},
      {"depth", {"depth", "m", 1}},
      {"depthlevel", {"depth", "m", 1}},
      {"ground_depth", {"ground depth", "m", 1}},
      {"soundinglevel", {"sounding", "", 1}},
      {"ensemblemember", {"member", "", 1}}};
  auto it = kinds.find(Fmi::ascii_tolower_copy(type));
  if (it == kinds.end())
    return {nullptr, "", 1};
  auto kind = it->second;
  if (source == SourceEngine::Grid && std::string(kind.type) == "pressure")
    kind.scale = 0.01;
  return kind;
}

std::string scaledLevel(const std::string& value, double scale)
{
  if (scale == 1)
    return value;
  try
  {
    return fmt::format("{}", std::stod(value) * scale);
  }
  catch (...)
  {
    return value;
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
      addTitle(md);
    addBBox(md.spatial_extent);
    addTime(md.temporal_extent);

    const auto& v = md.vertical_extent;
    if (!v.levels.empty())
      levels[v.level_type].insert(v.levels.begin(), v.levels.end());

    parameters.insert(md.parameter_names.begin(), md.parameter_names.end());
    for (const auto& p : md.timeseries_parameters)
      parameters.insert(p.first);
    stations = std::max(stations, md.stationMetaData.size());
  }

 private:
  void addTitle(const EDRMetaData& md)
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

  void addBBox(const edr_spatial_extent& s)
  {
    if (!(s.bbox_xmin < s.bbox_xmax || s.bbox_ymin < s.bbox_ymax))
      return;
    xmin = std::min(xmin, s.bbox_xmin);
    ymin = std::min(ymin, s.bbox_ymin);
    xmax = std::max(xmax, s.bbox_xmax);
    ymax = std::max(ymax, s.bbox_ymax);
  }

  void addTime(const edr_temporal_extent& t)
  {
    for (const auto* periods : {&t.time_periods, &t.single_time_periods})
      for (const auto& p : *periods)
      {
        start = earliest(start, p.start_time);
        end = latest(end, p.end_time);
        if (p.timestep > 0 && (step == 0 || p.timestep < step))
          step = p.timestep;
      }
    origin = latest(origin, t.origin_time);
  }

  // The earlier or later of the times, not-a-date-time meaning unknown
  static Fmi::DateTime earliest(const Fmi::DateTime& a, const Fmi::DateTime& b)
  {
    if (b.is_not_a_date_time() || (!a.is_not_a_date_time() && a <= b))
      return a;
    return b;
  }

  static Fmi::DateTime latest(const Fmi::DateTime& a, const Fmi::DateTime& b)
  {
    if (b.is_not_a_date_time() || (!a.is_not_a_date_time() && a >= b))
      return a;
    return b;
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

namespace
{
// What adds to the name of a parameter: most observation parameters have no description
JsonValue parameterInfo(const std::string& key, const edr_parameter& parameter)
{
  JsonValue info(::Json::objectValue);
  if (parameter.name != key)
    info["name"] = parameter.name;
  const auto& desc = parameter.description;
  if (!desc.empty() && desc != key && desc != parameter.name)
    info["description"] = desc;
  if (!parameter.unit.empty())
    info["unit"] = parameter.unit;
  return info;
}

// The first producer having a field gives it, the others fill in what is still missing, so
// that a producer without a description does not hide those of the others
void addParameters(const EDRMetaData& md, JsonValue& dictionary)
{
  for (const auto* list : {&md.parameters, &md.timeseries_parameters})
    for (const auto& p : *list)
    {
      const auto info = parameterInfo(p.first, p.second);
      if (info.size() == 0)
        continue;
      auto& known = dictionary[p.first];
      for (const auto& field : info.getMemberNames())
        if (!known.isMember(field))
          known[field] = info[field];
    }
}

// Numeric order where possible
bool levelLess(const std::string& a, const std::string& b)
{
  try
  {
    return std::stod(a) < std::stod(b);
  }
  catch (...)
  {
    return a < b;
  }
}

JsonValue levelJson(SourceEngine source,
                    const std::string& type,
                    const std::set<std::string>& values)
{
  JsonValue l;
  const auto kind = levelKind(source, type);
  l["type"] = (kind.type != nullptr ? std::string(kind.type) : type);
  if (*kind.unit != '\0')
    l["unit"] = kind.unit;
  std::vector<std::string> scaled;
  for (const auto& v : values)
    scaled.push_back(scaledLevel(v, kind.scale));
  std::sort(scaled.begin(), scaled.end(), levelLess);
  for (const auto& v : scaled)
    l["values"].append(v);
  return l;
}

JsonValue timeJson(const Entry& e)
{
  JsonValue t;
  if (!e.start.is_not_a_date_time())
    t["start"] = isoTime(e.start);
  if (!e.end.is_not_a_date_time())
    t["end"] = isoTime(e.end);
  if (e.step > 0)
    t["step"] = e.step;
  // Observations have no model run: their origin time is that of the metadata
  if (!e.origin.is_not_a_date_time() && e.source != SourceEngine::Observation)
    t["origintime"] = isoTime(e.origin);
  return t;
}

JsonValue producerJson(const std::string& name, const Entry& e)
{
  JsonValue p;
  p["name"] = name;
  p["source"] = sourceName(e.source);
  if (!e.title.empty())
    p["title"] = e.title;
  if (!e.description.empty())
    p["description"] = e.description;
  if (e.xmin <= e.xmax)
    for (double v : {e.xmin, e.ymin, e.xmax, e.ymax})
      p["bbox"].append(v);
  auto time = timeJson(e);
  if (!time.isNull())
    p["time"] = time;
  for (const auto& level : e.levels)
    p["levels"].append(levelJson(e.source, level.first, level.second));
  p["parameters"] = JsonValue(::Json::arrayValue);
  for (const auto& name : e.parameters)
    p["parameters"].append(name);
  if (e.stations > 0)
    p["stations"] = static_cast<::Json::UInt64>(e.stations);
  return p;
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
          addParameters(md, dictionary);
        }
      }
    }

    JsonValue producers(::Json::arrayValue);
    for (const auto& item : entries)
      producers.append(producerJson(item.first.first, item.second));

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
