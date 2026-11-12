// ======================================================================
/*!
 * \brief Smartmet TimeSeries plugin grid engine query processing
 */
// ======================================================================

#pragma once

#include "GridInterface.h"
#include "Producers.h"
#include <grid-content/queryServer/definition/QueryStreamer.h>
#include <spine/Location.h>
#include <timeseries/TimeSeriesInclude.h>
#include <timeseries/TimeSeriesUtility.h>
#include <memory>
#include <vector>

namespace SmartMet::Plugin::EDR
{
class CommonQuery;
class PluginImpl;
class ProducerDataPeriod;
class State;
}  // namespace SmartMet::Plugin::EDR
namespace SmartMet::T
{
class Coordinate;
}  // namespace SmartMet::T

namespace SmartMet
{

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
  void getWktLocationDefinition(Spine::LocationPtr& loc,
                                std::vector<std::vector<T::Coordinate>>& polygonPath,
                                const Spine::TaggedLocation& tloc,
                                CommonQuery& query) const;
  void getBBoxLocationDefinition(Spine::LocationPtr& loc,
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
