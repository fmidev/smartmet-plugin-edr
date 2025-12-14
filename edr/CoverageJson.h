#include "CoordinateFilter.h"
#include "EDRMetaData.h"
#include "EDRQuery.h"
// #include <json/json.h>
#include "Json.h"
#include <spine/Parameter.h>
#include <timeseries/TimeSeriesInclude.h>

namespace SmartMet
{
namespace Plugin
{
namespace EDR
{
namespace CoverageJson
{
Json::Value formatOutputData(const TS::OutputData &outputData,
                             const EDRMetaData &emd,
                             EDRQueryType query_type,
                             const std::set<int> &levels,
                             const CoordinateFilter &coordinate_filter,
                             const std::vector<Spine::Parameter> &query_parameters,
                             bool useDataLevels);

Json::Value parseEDRMetaData(
  const EDRQuery &edr_query, const EngineMetaData &emd, const ProducerLicenses &licenses);
Json::Value reportError(int code, const std::string &description);

}  // namespace CoverageJson
}  // namespace EDR
}  // namespace Plugin
}  // namespace SmartMet
