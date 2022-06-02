#include <json/json.h>
#include <timeseries/TimeSeriesInclude.h>
#include <spine/Parameter.h>
#include <json/json.h>
//#include <timeseries/TimeSeriesUtility.h>
#include "EDRMetaData.h"
#include "EDRQuery.h"

/*
#include <string>
#include <set>
#include <map>
#include <json/json.h>
#include <boost/date_time/posix_time/ptime.hpp>
#include <boost/optional.hpp>
*/
namespace SmartMet
{
namespace Engine
{
  namespace Querydata
  {
    class Engine;
  }
#ifndef WITHOUT_OBSERVATION
  namespace Observation
  {
    class Engine;
  }
#endif
}  // namespace Engine

namespace Plugin
{
namespace EDR
{
namespace CoverageJson
{

  Json::Value formatOutputData(TS::OutputData& outputData,
			       const EDRMetaData& emd,
			       const std::set<int>& levels,
			       const std::vector<Spine::Parameter>& query_parameters);

  EDRMetaData getProducerMetaData(const std::string& producer, Engine::Querydata::Engine* qEngine);
  Json::Value processEDRMetaDataQuery(const EDRQuery& edr_query,
				      Engine::Querydata::Engine* qEngine);
#ifndef WITHOUT_OBSERVATION
  EDRMetaData getProducerMetaData(const std::string& producer, Engine::Observation::Engine* obsEngine);
  Json::Value processEDRMetaDataQuery(const EDRQuery& edr_query,
				      Engine::Querydata::Engine* qEngine,
				      Engine::Observation::Engine* obsEngine);
#endif  
}  // namespace CoverageJson
}  // namespace EDR
}  // namespace Plugin
}  // namespace SmartMet
