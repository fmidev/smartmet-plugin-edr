// ======================================================================
/*!
 * \brief Query specific state variables and caches
 *
 * The State fixes the wall clock time for the duration of the query
 * so that different locations will not get different times simply
 * because the minute or hour may change during the calculations.
 *
 * The State object is often used to pass requests to the plugin
 * so that the results may be cached. This is not for speed, but
 * to make sure the same selection is made again if needed.
 *
 * Note that in general there are no thread safety issues since the
 * object is query specific.
 */
// ======================================================================

#pragma once
#include "EDRMetaData.h"
#include "Engines.h"
#include <engines/querydata/OriginTime.h>
#include <engines/querydata/Producer.h>
#include <engines/querydata/Q.h>
#include <macgyver/DateTime.h>
#include <macgyver/TimeZones.h>
#include <timeseries/TimeSeriesInclude.h>

namespace Fmi
{
class TimeZones;
}

namespace SmartMet
{
namespace Plugin
{
namespace EDR
{
class Plugin;

class State
{
 public:
  // We always construct with the plugin only
  explicit State(const Plugin& thePlugin);
  ~State() = default;
  State() = delete;
  State(const State& other) = delete;
  State(State&& other) = delete;
  State& operator=(const State& other) = delete;
  State& operator=(State&& other) = delete;

  // Access engines
  const Engine::Querydata::Engine& getQEngine() const;
  const Engine::Geonames::Engine& getGeoEngine() const;
  const Engine::Grid::Engine* getGridEngine() const;
#ifndef WITHOUT_OBSERVATION
  Engine::Observation::Engine* getObsEngine() const;
#endif
#ifndef WITHOUT_AVI
  const Engine::Avi::Engine* getAviEngine() const;
  const EDRProducerMetaData& getAviMetaData() const;
#endif
  const Plugin& getPlugin() const;

  const Fmi::TimeZones& getTimeZones() const;
  // The fixed time during the query may also be overridden
  // for testing purposes
  const Fmi::DateTime& getTime() const;
  void setTime(const Fmi::DateTime& theTime);

  // Get querydata for the given input
  Engine::Querydata::Q get(const Engine::Querydata::Producer& theProducer) const;
  Engine::Querydata::Q get(const Engine::Querydata::Producer& theProducer,
                           const Engine::Querydata::OriginTime& theOriginTime) const;
  EDRMetaData getProducerMetaData(const std::string& producer) const;
  bool isValidCollection(const std::string& producer) const;

  void setPretty(bool flag) { itsPretty = flag; }
  bool pretty() const { return itsPretty; }

 private:
  const Plugin& itsPlugin;
  Fmi::DateTime itsTime;

  // Querydata caches - always make the same choice for same locations and producers
  using QCache = std::map<Engine::Querydata::Producer, Engine::Querydata::Q>;
  using TimedQCache = std::map<Engine::Querydata::OriginTime, QCache>;

  mutable QCache itsQCache;
  mutable TimedQCache itsTimedQCache;
  bool itsPretty = false;

};  // class State

}  // namespace EDR
}  // namespace Plugin
}  // namespace SmartMet
