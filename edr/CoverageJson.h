#include "EDRMetaData.h"
#include "EDRQuery.h"
#include "CoordinateFilter.h"
//#include <json/json.h>
#include "Json.h"
#include <spine/Parameter.h>
#include <spine/Location.h>
#include <timeseries/TimeSeriesInclude.h>

namespace SmartMet {
namespace Plugin {
namespace EDR {
namespace CoverageJson {
Json::Value
formatOutputData(TS::OutputData &outputData, const EDRMetaData &emd,
				 EDRQueryType query_type,
                 const std::set<int>& levels,
				 const CoordinateFilter& coordinate_filter,
                 const std::vector<Spine::Parameter> &query_parameters);

Json::Value parseEDRMetaData(const EDRQuery &edr_query, const EngineMetaData& emd);
Json::Value parseEDRMetaData(const Spine::LocationList& locations, const EngineMetaData& emd);

} // namespace CoverageJson
} // namespace EDR
} // namespace Plugin
} // namespace SmartMet
