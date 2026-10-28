// ======================================================================
/*!
 * \brief The details of one grid producer for the data explorer
 *
 * What the parameter keys of a grid producer may contain, read from the
 * content server like the grid-gui plugin does:
 *
 *   {
 *     "producer": "HIRLAM",                      // the name in parameter keys
 *     "runs": [ { "origintime", "status" } ],    // the latest first
 *     "run": { "origintime", "status", "start", "end",
 *              "geometries": [ { "id", "name" } ],
 *              "parameters": {
 *                name: { "name",                 // as the data gives it
 *                        "levels": [ { "type", "levelId", "unit",
 *                                      "values", "keys" } ],
 *                        "forecasts": [ { "type", "name", "description",
 *                                         "members" } ],
 *                        "aggregation": { "name", "description", "period" },
 *                        "processing": { "name", "description" },
 *                        "times" } },
 *              "aliases": { alias: name } }      // mapping names of parameters
 *   }
 *
 * Parameter names and aliases are lower case like in the producer listing.
 * The aliases are the names the parameter mappings give to the parameters
 * (air_pressure for p-pa), which requests accept too. The level
 * "values" are in the units of the levels option of a request (hPa for
 * pressure), the "keys" in the units of the parameter keys (Pa).
 */
// ======================================================================

#pragma once

#include <json/json.h>
#include <memory>
#include <string>

namespace SmartMet
{
namespace Engine
{
namespace Grid
{
class Engine;
}  // namespace Grid
}  // namespace Engine

namespace Plugin
{
namespace EDR
{
namespace ProducerDetails
{
class Cache;

std::shared_ptr<Cache> makeCache();

// Throws if the producer is unknown. An empty origintime selects the latest run.
::Json::Value grid(const Engine::Grid::Engine& engine,
                   Cache& cache,
                   const std::string& producer,
                   const std::string& origintime);

}  // namespace ProducerDetails
}  // namespace EDR
}  // namespace Plugin
}  // namespace SmartMet
