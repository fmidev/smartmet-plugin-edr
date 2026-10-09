// ======================================================================
/*!
 * \brief Smartmet TimeSeries plugin AVI engine query processing
 */
// ======================================================================

#pragma once

#include "Json.h"
#include "PluginImpl.h"
#include <engines/avi/Engine.h>

namespace SmartMet
{
namespace Plugin
{
namespace EDR
{

class AviEngineQuery
{
 public:
  explicit AviEngineQuery(const PluginImpl &thePlugin);

#ifndef WITHOUT_AVI
  void processAviEngineQuery(const Config &config,
                             const State &state,
                             const Query &query,
                             const std::string &producer,
                             TS::OutputData &outputData) const;

  bool isAviProducer(const std::string &producer) const;

  // OGC API-EDR items query of an aviation collection: the messages as GeoJSON features
  Json::Value processItemsQuery(const Config &config,
                                const State &state,
                                const EDRQuery &edr_query) const;

#endif

  const PluginImpl &itsPlugin;
};

}  // namespace EDR
}  // namespace Plugin
}  // namespace SmartMet

// ======================================================================
