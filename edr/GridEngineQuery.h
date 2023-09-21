// ======================================================================
/*!
 * \brief Smartmet TimeSeries plugin grid engine query processing
 */
// ======================================================================

#pragma once

#include "Plugin.h"
#include "GridInterface.h"

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
  GridEngineQuery(const Plugin& thePlugin);

  bool processGridEngineQuery(const State& state,
							  Query& query,
							  TS::OutputData& outputData,
							  const QueryServer::QueryStreamer_sptr& queryStreamer,
							  const AreaProducers& areaproducers,
							  const ProducerDataPeriod& producerDataPeriod) const;
  bool isGridEngineQuery(const AreaProducers& theProducers, const Query& theQuery) const;

private:

  const Plugin& itsPlugin;

  std::unique_ptr<GridInterface> itsGridInterface;
};

}  // namespace EDR
}  // namespace Plugin
}  // namespace SmartMet

// ======================================================================
