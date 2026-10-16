// ======================================================================
/*!
 * \brief The producers of the TimeSeries API for the data explorer
 *
 * A compact JSON document of the producers (models and observation
 * producers) the TimeSeries API reads, generated from the EDR metadata:
 *
 *   {
 *     "producers": [ { "name", "source", "title", "description", "bbox",
 *                      "time": { "start", "end", "step", "origintime" },
 *                      "levels": [ { "type", "values" } ],
 *                      "parameters": [ names ], "stations" } ],
 *     "parameters": { name: { "name", "description", "unit" } }
 *   }
 *
 * Parameter names are lower case, as the TimeSeries API treats them case
 * insensitively; "name" in the dictionary is the name as the data gives it.
 * Grid producers are listed by their TimeSeries names, the geometries and
 * level types of a producer combined.
 */
// ======================================================================

#pragma once

#include "EDRMetaData.h"
#include <json/json.h>
#include <string>
#include <vector>

namespace SmartMet
{
namespace Plugin
{
namespace EDR
{
namespace ProducerCatalog
{
::Json::Value build(const EngineMetaData& metadata);

// The producers of one source (querydata, grid or observation) and their parameters
::Json::Value filter(const ::Json::Value& catalog, const std::string& source);

// The source names
const std::vector<std::string>& sources();

}  // namespace ProducerCatalog
}  // namespace EDR
}  // namespace Plugin
}  // namespace SmartMet
