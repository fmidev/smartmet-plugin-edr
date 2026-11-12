#include "EDRDefs.h"
#include "Json.h"
#include <timeseries/TimeSeriesInclude.h>
#include <timeseries/TimeSeriesUtility.h>
#include <set>
#include <string>
#include <vector>

namespace SmartMet::Plugin::EDR
{
class CoordinateFilter;
enum class EDRQueryType;
struct EDRMetaData;
}  // namespace SmartMet::Plugin::EDR
namespace SmartMet::Spine
{
class Parameter;
}  // namespace SmartMet::Spine

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
                             const std::vector<Spine::Parameter> &query_parameters,
                             const CustomDimReferences &custom_dim_refs,
                             const std::string &language);
}  // namespace GeoJson
}  // namespace EDR
}  // namespace Plugin
}  // namespace SmartMet
