#include "EDRAPI.h"
#include <boost/algorithm/string.hpp>
#include <macgyver/Exception.h>

namespace SmartMet
{
namespace Plugin
{
namespace EDR
{
namespace
{
const std::string PROBLEM_DETAIL_JSON =
R"JSON({
  "title" : "An error occurred",
  "status" : __STATUS__,
  "detail" : "__DETAIL__"
}
)JSON";
}  // namespace

bool EDRAPI::isEDRAPIQuery(const std::string& url) const
{
  std::string url_string = url;
  if (itsAPISettings.find(url_string) == itsAPISettings.end() && !url_string.empty() &&
      url_string.back() == '/')
    url_string.pop_back();
  return (itsAPISettings.find(url_string) != itsAPISettings.end());
}

std::string EDRAPI::getAPI(const std::string& url, const std::string& host) const
{
  // If no API setting defined for the requested URL return empty string
  std::string url_string = url;
  if (itsAPISettings.find(url_string) == itsAPISettings.end() && !url_string.empty() &&
      url_string.back() == '/')
    url_string.pop_back();

  auto it = itsAPISettings.find(url_string);
  if (it == itsAPISettings.end())
    return {};

  // The documents are small and rarely requested, there is no need to cache the result
  // for each host name
  auto response = it->second;
  boost::algorithm::replace_all(response, "__HOST__", host);
  return response;
}

std::string EDRAPI::getProblemDetailJson() const
{
  auto it = itsAPISettings.find(PROBLEM_DETAIL);

  if (it != itsAPISettings.end())
    return it->second;

  return PROBLEM_DETAIL_JSON;
}

void EDRAPI::setSettings(const std::string& tmpldir, const APISettings& api_settings)
{
  try
  {
    itsTemplateDir = tmpldir;
    if (itsTemplateDir.back() != '/')
      itsTemplateDir.append("/");
    itsAPISettings = api_settings;

    for (auto& item : itsAPISettings)
    {
      auto template_file = item.second;
      template_file.insert(0, itsTemplateDir);
      std::string content;
      std::ifstream in(template_file.c_str());
      if (!in)
        throw Fmi::Exception(BCP, "Failed to open '" + template_file + "' for reading!");
      content.assign(std::istreambuf_iterator<char>(in), std::istreambuf_iterator<char>());
      // Replace template filename with file content
      item.second = content;
    }
  }
  catch (...)
  {
    throw Fmi::Exception(BCP, "Operation failed!", nullptr);
  }
}

}  // namespace EDR
}  // namespace Plugin
}  // namespace SmartMet
