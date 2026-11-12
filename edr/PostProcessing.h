// ======================================================================
/*!
 * \brief Smartmet TimeSeries data postprocessing functions
 */
// ======================================================================

#pragma once

#include "ObsParameter.h"
#include <timeseries/TimeSeries.h>
#include <timeseries/TimeSeriesInclude.h>
#include <timeseries/TimeSeriesUtility.h>
#include <vector>

namespace SmartMet::Plugin::EDR
{
class CommonQuery;
}  // namespace SmartMet::Plugin::EDR
namespace SmartMet::Spine
{
class Table;
}  // namespace SmartMet::Spine

namespace SmartMet
{
namespace Plugin
{
namespace EDR
{
namespace PostProcessing
{
void store_data(TS::TimeSeriesVectorPtr aggregatedData, CommonQuery& query, TS::OutputData& outputData);
void store_data(std::vector<TS::TimeSeriesData>& aggregatedData,
                CommonQuery& query,
                TS::OutputData& outputData);
void fill_table(CommonQuery& query, TS::OutputData& outputData, Spine::Table& table);
void fix_precisions(CommonQuery& masterquery, const ObsParameters& obsParameters);

}  // namespace PostProcessing
}  // namespace EDR
}  // namespace Plugin
}  // namespace SmartMet

// ======================================================================
