// ======================================================================
/*!
 * \brief Utility functions for parsing the request
 */
// ======================================================================

#include "Query.h"
#include "Config.h"

namespace SmartMet
{
namespace Plugin
{
namespace EDR
{

// ----------------------------------------------------------------------
/*!
 * \brief The constructor parses the query string
 *
 * EDRQueryParams is initialized first (listed first in the class definition)
 * so it can modify req (adding 'producer' and 'param' parameters) before
 * CommonQuery reads them.
 */
// ----------------------------------------------------------------------

Query::Query(const State& state, const Spine::HTTP::Request& request, Config& config)
    : EDRQueryParams(state, request, config),
      CommonQuery(state, EDRQueryParams::req, config)
{
  // BRAINSTORM-3178: EDR table data is JSON format string, ignore the "format" request param
  format = "ascii";

  // Metadata queries do not need full parameter parsing
  if (isEDRMetaDataQuery())
    return;

  // AVI producer validator: use the static method from EDRQueryParams
  auto avi_lambda = [&state](const std::string& p) -> bool
  {
    return EDRQueryParams::isAviProducer(state.getAviMetaData(), p);
  };

  // EDR: step defaults to 0.0 (coordinates given explicitly), DataTimes mode when no timestep set
  commonInit(state, EDRQueryParams::req, config, 0.0, true, avi_lambda);
}

}  // namespace EDR
}  // namespace Plugin
}  // namespace SmartMet

// ======================================================================
