// ======================================================================
/*!
 * \brief Links of responses, in the response body and in the Link header
 */
// ======================================================================

#pragma once

#include "Json.h"
#include <string>
#include <vector>

namespace SmartMet
{
namespace Plugin
{
namespace EDR
{
struct Link
{
  std::string href;
  std::string rel;
  std::string type;
};

using Links = std::vector<Link>;

// The links as a JSON array of link objects
Json::Value links_json(const Links& links);

// RFC 8288 Link header value, e.g. <href>; rel="next"; type="application/geo+json"
std::string link_header(const Links& links);

}  // namespace EDR
}  // namespace Plugin
}  // namespace SmartMet
