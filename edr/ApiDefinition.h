// ======================================================================
/*!
 * \brief Generated API documents of the EDR plugin
 *
 * The landing page, the conformance declaration and the OpenAPI definitions
 * of the EDR API and the TimeSeries API are generated from the plugin
 * configuration and the current metadata. Nothing is read from templates.
 */
// ======================================================================

#pragma once

#include <json/json.h>
#include <spine/OpenApi.h>
#include <string>

namespace SmartMet
{
namespace Plugin
{
namespace EDR
{
class Config;
class EngineMetaData;

namespace ApiDefinition
{
// GET <root>, where 'root' is the URL of the EDR API used in the links
::Json::Value landingPage(const Config& config, const std::string& root);

// GET <root>/conformance
::Json::Value conformance();

// The OpenAPI definition of the EDR API. Paths are relative to the EDR root URL.
Spine::OpenAPI::Document edr(const Config& config, const EngineMetaData& metadata);

// The OpenAPI definition of the TimeSeries API served at 'url' (e.g. "/timeseries").
// Paths are absolute, the server URL is the host root.
// The title and description of the TimeSeries API, also used in the API catalog
extern const char* const TIMESERIES_TITLE;
extern const char* const TIMESERIES_DESCRIPTION;

Spine::OpenAPI::Document timeseries(const Config& config,
                                    const EngineMetaData& metadata,
                                    const std::string& url);

}  // namespace ApiDefinition
}  // namespace EDR
}  // namespace Plugin
}  // namespace SmartMet
