// ======================================================================
/*!
 * \brief Utility functions for parsing the request
 *
 * Separated from PluginImpl to clarify the implementation
 */
// ======================================================================

#pragma once

#include "CommonQuery.h"
#include "EDRQueryParams.h"

namespace SmartMet
{
namespace Plugin
{
namespace EDR
{
class Config;

// EDRQueryParams must be listed first so it initializes (and modifies req)
// before CommonQuery reads req.
class Query : public EDRQueryParams, public CommonQuery
{
 public:
  Query() = delete;
  Query(const State& state, const Spine::HTTP::Request& req, Config& config);
};

}  // namespace EDR
}  // namespace Plugin
}  // namespace SmartMet

// ======================================================================
