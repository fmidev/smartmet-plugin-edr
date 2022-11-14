#include "EDRMetaData.h"
#include "EDRQuery.h"
#include "CoordinateFilter.h"
//#include <json/json.h>
#include "Json.h"
#include <spine/Parameter.h>
#include <timeseries/TimeSeriesInclude.h>
//#include <spine/Location.h>

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
Json::Value reportError(int code, const std::string& description);

} // namespace CoverageJson
} // namespace EDR
} // namespace Plugin
} // namespace SmartMet
