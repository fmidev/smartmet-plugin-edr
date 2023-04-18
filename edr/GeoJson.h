#include "CoordinateFilter.h"
#include "EDRMetaData.h"
#include "EDRQuery.h"
#include "Json.h"
#include <spine/Parameter.h>
#include <timeseries/TimeSeriesInclude.h>

namespace SmartMet
{
namespace Plugin
{
namespace EDR
{
namespace GeoJson
{
Json::Value formatOutputData(const TS::OutputData &outputData,
                             const EDRMetaData &emd,
                             EDRQueryType query_type,
                             const std::set<int> &levels,
                             const CoordinateFilter &coordinate_filter,
                             const std::vector<Spine::Parameter> &query_parameters);
}  // namespace GeoJson
}  // namespace EDR
}  // namespace Plugin
}  // namespace SmartMet
