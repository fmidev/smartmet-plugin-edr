// ======================================================================
/*!
 * \brief EDR API
 */
// ======================================================================

#pragma once

#include "EDRDefs.h"
#include <spine/Thread.h>

namespace SmartMet
{
namespace Plugin
{
namespace EDR
{

class EDRAPI
{
public:
  EDRAPI() {}
  void setSettings(const std::string& tmpldir, const APISettings& api_settings);
  const std::string& getAPI(const std::string& url, const std::string& host) const;
  bool isEDRAPIQuery(const std::string& url) const;
private:
  APISettings itsAPISettings;
  std::string itsTemplateDir;
  mutable Spine::MutexType itsMutex;
  mutable APISettings itsAPIQueryResponses;
};

}  // namespace EDR
}  // namespace Plugin
}  // namespace SmartMet

