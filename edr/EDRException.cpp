#include "EDRException.h"
#include <macgyver/StringConversion.h>
#include <cstring>

namespace SmartMet
{
namespace Plugin
{
namespace EDR
{
int http_status(const Fmi::Exception& exception)
{
  int status = 0;
  bool timeout = false;

  // The chain runs from the outermost exception to the innermost one
  for (const auto* e = &exception; e != nullptr; e = e->getPrevException())
  {
    if (const char* value = e->getParameterValue(HTTP_STATUS_PARAMETER))
    {
      try
      {
        status = Fmi::stoi(value);
      }
      catch (...)
      {
      }
    }
    const char* what = e->getWhat();
    if (what != nullptr)
    {
      if (std::strcmp(what, "RequestLimitError") == 0)
        status = 413;
      else if (std::strstr(what, "timeout") != nullptr)
        timeout = true;
    }
  }

  if (status != 0)
    return status;
  if (timeout)
    return 503;
  return 400;
}

std::string error_message(const Fmi::Exception& exception)
{
  for (const auto* e = &exception; e != nullptr; e = e->getPrevException())
  {
    const char* what = e->getWhat();
    if (what != nullptr && std::strcmp(what, "RequestLimitError") == 0)
    {
      if (const char* description = e->getParameterValue("description"))
        return description;
    }
  }
  return exception.what();
}

}  // namespace EDR
}  // namespace Plugin
}  // namespace SmartMet
