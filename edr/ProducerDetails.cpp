#include "ProducerDetails.h"
#include <engines/grid/Engine.h>
#include <fmt/format.h>
#include <grid-content/contentServer/definition/ServiceInterface.h>
#include <grid-files/identification/GridDef.h>
#include <macgyver/Exception.h>
#include <macgyver/StringConversion.h>
#include <cmath>
#include <map>
#include <mutex>
#include <set>
#include <string>

namespace SmartMet
{
namespace Plugin
{
namespace EDR
{
namespace ProducerDetails
{
namespace
{
using JsonValue = ::Json::Value;

// The details of a finished model run do not change
const std::size_t max_cached_runs = 100;

// 20080909T000000 -> 2008-09-09T00:00:00Z
std::string isoTime(const std::string& t)
{
  if (t.size() < 15 || t[8] != 'T')
    return t;
  return fmt::format("{}-{}-{}T{}:{}:{}Z",
                     t.substr(0, 4),
                     t.substr(4, 2),
                     t.substr(6, 2),
                     t.substr(9, 2),
                     t.substr(11, 2),
                     t.substr(13, 2));
}

std::string isoTime(time_t t)
{
  return Fmi::date_time::to_iso_extended_string(Fmi::date_time::from_time_t(t)) + "Z";
}

const char* statusName(uchar status)
{
  switch (status)
  {
    case T::GenerationInfo::Status::Ready:
      return "ready";
    case T::GenerationInfo::Status::Running:
      return "running";
    default:
      return "disabled";
  }
}

// The level type names of the producer listing, and the scale from the units of the parameter
// keys to the units of the levels option
struct LevelKind
{
  std::string type;
  std::string unit;
  double scale = 1;
};

LevelKind levelKind(T::ParamLevelId levelId)
{
  LevelKind kind;
  Identification::LevelDef def;
  if (Identification::gridDef.getFmiLevelDef(levelId, def))
  {
    kind.type = Fmi::ascii_tolower_copy(def.mName);
    kind.unit = def.mUnits;
  }
  else
    kind.type = fmt::format("level {}", levelId);

  if (levelId == 2)  // pressure
  {
    kind.type = "pressure";
    kind.unit = "hPa";
    kind.scale = 0.01;
  }
  else if (levelId == 3)
    kind.type = "hybrid";
  return kind;
}

struct ParameterInfo
{
  std::string name;
  std::map<T::ParamLevelId, std::set<T::ParamLevel>> levels;
  std::map<T::ForecastType, std::set<T::ForecastNumber>> forecasts;
  int aggregationId = 0;
  int aggregationPeriod = 0;
  int processingTypeId = 0;
  uint times = 0;
};

JsonValue parameterJson(const ParameterInfo& info)
{
  JsonValue p;
  p["name"] = info.name;

  p["levels"] = JsonValue(::Json::arrayValue);
  for (const auto& item : info.levels)
  {
    const auto kind = levelKind(item.first);
    JsonValue l;
    l["type"] = kind.type;
    l["levelId"] = item.first;
    if (!kind.unit.empty())
      l["unit"] = kind.unit;
    l["values"] = JsonValue(::Json::arrayValue);
    l["keys"] = JsonValue(::Json::arrayValue);
    // Pressure from the ground up, other levels in increasing order
    std::vector<T::ParamLevel> values(item.second.begin(), item.second.end());
    if (item.first == 2)
      std::reverse(values.begin(), values.end());
    for (auto v : values)
    {
      const double value = v * kind.scale;
      if (value == std::floor(value) && std::abs(value) < 1e9)
        l["values"].append(static_cast<::Json::Int>(value));
      else
        l["values"].append(value);
      l["keys"].append(v);
    }
    p["levels"].append(l);
  }

  p["forecasts"] = JsonValue(::Json::arrayValue);
  for (const auto& item : info.forecasts)
  {
    JsonValue f;
    f["type"] = item.first;
    Identification::ForecastTypeDef def;
    if (Identification::gridDef.getFmiForecastTypeDef(item.first, def))
    {
      f["name"] = def.mName;
      f["description"] = def.mDescription;
    }
    f["members"] = JsonValue(::Json::arrayValue);
    for (auto n : item.second)
      f["members"].append(n);
    p["forecasts"].append(f);
  }

  // No aggregation (1) and no processing (1) are not worth mentioning, nor are codes without a
  // definition
  Identification::AggregationDef aggregation;
  if (info.aggregationId > 1 &&
      Identification::gridDef.getFmiAggregationDef(info.aggregationId, aggregation))
  {
    p["aggregation"]["name"] = Fmi::ascii_tolower_copy(aggregation.mName);
    p["aggregation"]["description"] = aggregation.mDescription;
    if (info.aggregationPeriod > 0)
      p["aggregation"]["period"] = info.aggregationPeriod;
  }
  Identification::ProcessingTypeDef processing;
  if (info.processingTypeId > 1 &&
      Identification::gridDef.getFmiProcessingTypeDef(info.processingTypeId, processing))
  {
    p["processing"]["name"] = Fmi::ascii_tolower_copy(processing.mName);
    p["processing"]["description"] = processing.mDescription;
  }

  p["times"] = info.times;
  return p;
}

JsonValue runJson(ContentServer::ServiceInterface& content, const T::GenerationInfo& generation)
{
  JsonValue run;
  run["origintime"] = isoTime(generation.mAnalysisTime);
  run["status"] = statusName(generation.mStatus);

  time_t start = 0;
  time_t end = 0;
  if (content.getContentTimeRangeByGenerationId(0, generation.mGenerationId, start, end) == 0 &&
      start > 0 && end >= start)
  {
    run["start"] = isoTime(start);
    run["end"] = isoTime(end);
  }

  run["geometries"] = JsonValue(::Json::arrayValue);
  std::set<T::GeometryId> geometries;
  content.getContentGeometryIdListByGenerationId(0, generation.mGenerationId, geometries);
  for (auto id : geometries)
  {
    JsonValue g;
    g["id"] = id;
    std::string name;
    if (Identification::gridDef.getGeometryNameById(id, name))
      g["name"] = name;
    run["geometries"].append(g);
  }

  // One record per parameter, level, forecast type and member; the message index counts the
  // times
  T::ContentInfoList list;
  content.getContentParamListByGenerationId(0, generation.mGenerationId, list);
  std::map<std::string, ParameterInfo> parameters;
  for (uint i = 0; i < list.getLength(); i++)
  {
    const auto* c = list.getContentInfoByIndex(i);
    if (c == nullptr)
      continue;
    const std::string name = c->getFmiParameterName();
    if (name.empty())
      continue;
    auto& info = parameters[Fmi::ascii_tolower_copy(name)];
    info.name = name;
    info.levels[c->mFmiParameterLevelId].insert(c->mParameterLevel);
    info.forecasts[c->mForecastType].insert(c->mForecastNumber);
    if (info.aggregationId == 0)
    {
      info.aggregationId = c->mAggregationId;
      info.aggregationPeriod = c->mAggregationPeriod;
      info.processingTypeId = c->mProcessingTypeId;
    }
    info.times = std::max(info.times, static_cast<uint>(c->mMessageIndex));
  }

  run["parameters"] = JsonValue(::Json::objectValue);
  for (const auto& item : parameters)
    run["parameters"][item.first] = parameterJson(item.second);
  return run;
}

}  // namespace

class Cache
{
 public:
  std::shared_ptr<const JsonValue> find(T::GenerationId id)
  {
    std::lock_guard<std::mutex> lock(itsMutex);
    auto pos = itsRuns.find(id);
    return (pos == itsRuns.end() ? nullptr : pos->second);
  }

  void insert(T::GenerationId id, std::shared_ptr<const JsonValue> run)
  {
    std::lock_guard<std::mutex> lock(itsMutex);
    if (itsRuns.size() >= max_cached_runs)
      itsRuns.clear();
    itsRuns[id] = std::move(run);
  }

 private:
  std::mutex itsMutex;
  std::map<T::GenerationId, std::shared_ptr<const JsonValue>> itsRuns;
};

std::shared_ptr<Cache> makeCache()
{
  return std::make_shared<Cache>();
}

JsonValue grid(const Engine::Grid::Engine& engine,
               Cache& cache,
               const std::string& producer,
               const std::string& origintime)
{
  try
  {
    auto content = engine.getContentServer_sptr();
    if (!content)
      throw Fmi::Exception(BCP, "The grid content server is not available");

    T::ProducerInfo producerInfo;
    if (content->getProducerInfoByName(0, engine.getProducerName(producer), producerInfo) != 0 &&
        content->getProducerInfoByName(0, producer, producerInfo) != 0)
      throw Fmi::Exception(BCP, "Unknown grid producer").addParameter("producer", producer);

    T::GenerationInfoList generations;
    content->getGenerationInfoListByProducerId(0, producerInfo.mProducerId, generations);
    generations.sort(T::GenerationInfo::ComparisonMethod::analysisTime_generationId);

    JsonValue result;
    result["producer"] = producerInfo.mName;
    result["runs"] = JsonValue(::Json::arrayValue);

    // By default the latest finished run, or the latest one if none has finished
    const T::GenerationInfo* latest = nullptr;
    const T::GenerationInfo* latestReady = nullptr;
    const T::GenerationInfo* wanted = nullptr;
    for (uint i = generations.getLength(); i > 0; i--)
    {
      const auto* g = generations.getGenerationInfoByIndex(i - 1);
      if (g == nullptr || g->mStatus == T::GenerationInfo::Status::Disabled ||
          (g->mFlags & T::GenerationInfo::Flags::DeletedGeneration) != 0)
        continue;
      JsonValue run;
      run["origintime"] = isoTime(g->mAnalysisTime);
      run["status"] = statusName(g->mStatus);
      result["runs"].append(run);

      if (latest == nullptr)
        latest = g;
      if (latestReady == nullptr && g->mStatus == T::GenerationInfo::Status::Ready)
        latestReady = g;
      if (wanted == nullptr && run["origintime"].asString() == origintime)
        wanted = g;
    }
    const auto* chosen =
        (!origintime.empty() ? wanted : (latestReady != nullptr ? latestReady : latest));

    if (chosen == nullptr)
    {
      if (!origintime.empty())
        throw Fmi::Exception(BCP, "Unknown model run")
            .addParameter("producer", producer)
            .addParameter("origintime", origintime);
      return result;
    }

    auto run =
        (chosen->mStatus == T::GenerationInfo::Status::Ready ? cache.find(chosen->mGenerationId)
                                                             : nullptr);
    if (!run)
    {
      run = std::make_shared<const JsonValue>(runJson(*content, *chosen));
      if (chosen->mStatus == T::GenerationInfo::Status::Ready)
        cache.insert(chosen->mGenerationId, run);
    }
    result["run"] = *run;
    return result;
  }
  catch (...)
  {
    throw Fmi::Exception::Trace(BCP, "Operation failed!");
  }
}

}  // namespace ProducerDetails
}  // namespace EDR
}  // namespace Plugin
}  // namespace SmartMet
