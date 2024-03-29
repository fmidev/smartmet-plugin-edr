// ======================================================================
/*!
 * \brief Definitions used by EDR plugin
 */
// ======================================================================

#pragma once

#include "AviCollection.h"
#include "Precision.h"
#include <boost/utility.hpp>
#include <engines/gis/GeometryStorage.h>
#include <engines/grid/Engine.h>
#include <spine/Parameter.h>
#include <spine/TableFormatterOptions.h>
#include <timeseries/RequestLimits.h>
#include <libconfig.h++>
#include <string>

namespace SmartMet
{
namespace Plugin
{
namespace EDR
{
class AviCollection;

using Precisions = std::map<std::string, Precision>;
using SupportedOutputFormats =
    std::map<std::string, std::set<std::string>>;  // producer -> output format
using SupportedDataQueries = std::map<std::string, std::set<std::string>>;  // producer -> queries
using ProducerKeywords = std::map<std::string, std::set<std::string>>;      // producer -> keywords
using AviCollections = std::list<AviCollection>;
using APISettings = std::map<std::string, std::string>;

enum class SourceEngine
{
  Querydata,
  Grid,
  Observation,
  Avi,
  Undefined
};

#define Q_ENGINE "querydata_engine"
#define OBS_ENGINE "observation_engine"
#define GRID_ENGINE "grid_engine"
#define AVI_ENGINE "avi_engine"

#define IWXXM_FORMAT "IWXXM"
#define TAC_FORMAT "TAC"
#define COVERAGE_JSON_FORMAT "CoverageJSON"
#define GEO_JSON_FORMAT "GeoJSON"

std::string get_engine_name(SourceEngine source_engine);

}  // namespace EDR
}  // namespace Plugin
}  // namespace SmartMet
