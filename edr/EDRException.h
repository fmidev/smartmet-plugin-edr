// ======================================================================
/*!
 * \brief Exceptions with the HTTP status to report for them
 */
// ======================================================================

#pragma once

#include <macgyver/Exception.h>
#include <string>

namespace SmartMet
{
namespace Plugin
{
namespace EDR
{
// Name of the exception parameter holding the HTTP status code
inline constexpr const char* HTTP_STATUS_PARAMETER = "HTTP status";

// HTTP status for a caught exception. The innermost exception with an HTTP status parameter
// decides, request limit errors are reported with 413, timeouts with 503. Others are 400,
// since most of them are caused by invalid request parameters.
int http_status(const Fmi::Exception& exception);

// The message of a caught exception for the response. A request limit error is described by
// its description parameter, otherwise the first message of the exception is used.
std::string error_message(const Fmi::Exception& exception);

}  // namespace EDR
}  // namespace Plugin
}  // namespace SmartMet

// Invalid request (400 Bad Request)
#define EDRException(description) \
  Fmi::Exception(BCP, description).addParameter(SmartMet::Plugin::EDR::HTTP_STATUS_PARAMETER, "400")

// Unknown resource such as a collection, an instance or a location (404 Not Found)
#define EDRNotFound(description) \
  Fmi::Exception(BCP, description).addParameter(SmartMet::Plugin::EDR::HTTP_STATUS_PARAMETER, "404")

// No acceptable output format in the Accept header (406 Not Acceptable)
#define EDRNotAcceptable(description) \
  Fmi::Exception(BCP, description).addParameter(SmartMet::Plugin::EDR::HTTP_STATUS_PARAMETER, "406")

// Request exceeds the configured limits (413 Content Too Large)
#define EDRTooLarge(description) \
  Fmi::Exception(BCP, description).addParameter(SmartMet::Plugin::EDR::HTTP_STATUS_PARAMETER, "413")
