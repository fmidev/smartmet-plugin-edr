// ======================================================================
/*!
 * \brief Smartmet TimeSeries plugin grid engine query processing
 */
// ======================================================================

#pragma once

#include "CommonQuery.h"
#include "GridInterface.h"
#include "PluginImpl.h"

namespace SmartMet
{
namespace Engine
{
namespace Grid
{
class Engine;
}
}  // namespace Engine

namespace Plugin
{
namespace EDR
{
class GridEngineQuery
{
 public:
  GridEngineQuery(const PluginImpl& thePlugin);

  bool processGridEngineQuery(const State& state,
                              CommonQuery& query,
                              TS::OutputData& outputData,
                              const QueryServer::QueryStreamer_sptr& queryStreamer,
                              const AreaProducers& areaproducers,
                              const ProducerDataPeriod& producerDataPeriod) const;
  bool isGridEngineQuery(const AreaProducers& theProducers, const CommonQuery& theQuery) const;

 private:
  void getLocationDefinition(Spine::LocationPtr& loc,
                             std::vector<std::vector<T::Coordinate>>& polygonPath,
                             const Spine::TaggedLocation& tloc,
                             CommonQuery& query) const;

  const PluginImpl& itsPlugin;

  std::unique_ptr<GridInterface> itsGridInterface;
};

}  // namespace EDR
}  // namespace Plugin
}  // namespace SmartMet

// ======================================================================
