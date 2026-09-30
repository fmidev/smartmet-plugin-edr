// ======================================================================
/*!
 * \brief Smartmet TimeSeries data postprocessing functions
 */
// ======================================================================

#pragma once

#include "CommonQuery.h"
#include "ObsParameter.h"
#include <timeseries/TimeSeriesInclude.h>

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
