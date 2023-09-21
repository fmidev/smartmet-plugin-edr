// ======================================================================
/*!
 * \brief Smartmet TimeSeries plugin query processing hub
 */
// ======================================================================

#pragma once

#include "Plugin.h"
#include "QEngineQuery.h"
#include "ObsEngineQuery.h"
#include "AviEngineQuery.h"
#include "GridEngineQuery.h"
#include "Json.h"
#include <spine/HTTP.h>

namespace SmartMet
{
namespace Plugin
{
namespace EDR
{

class QueryProcessingHub
{
public:
  QueryProcessingHub(const Plugin& thePlugin);

  boost::shared_ptr<std::string> processQuery(const State& state,
											  Spine::Table& table,
											  Query& masterquery,
											  const QueryServer::QueryStreamer_sptr& queryStreamer,
											  size_t& product_hash);

  Json::Value processMetaDataQuery(const State& state, const EDRQuery& edr_query) const;
	
  std::size_t hash_value(const State& state,
						 const Spine::HTTP::Request& request,
						 Query masterquery) const;

private:

  QEngineQuery itsQEngineQuery;
  ObsEngineQuery itsObsEngineQuery;
  AviEngineQuery itsAviEngineQuery;
  GridEngineQuery itsGridEngineQuery;

};

}  // namespace EDR
}  // namespace Plugin
}  // namespace SmartMet

// ======================================================================
