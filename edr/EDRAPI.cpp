#include "EDRAPI.h"
#include <macgyver/Exception.h>
#include <boost/algorithm/string.hpp>

namespace SmartMet
{
namespace Plugin
{
namespace EDR
{

static std::string EMPTY_STRING;

bool EDRAPI::isEDRAPIQuery(const std::string& url) const
{
  return (itsAPISettings.find(url) != itsAPISettings.end());
}

const std::string& EDRAPI::getAPI(const std::string& url, const std::string& host) const
{
  // If no API setting defined for the requested URL return empty string
  if(itsAPISettings.find(url) == itsAPISettings.end())
	return EMPTY_STRING;

  // If response isnt in the map insert it
  {
	Spine::ReadLock lock(itsMutex);
	if(itsAPIQueryResponses.find(host+url) == itsAPIQueryResponses.end())
	  {
		auto template_file = itsAPISettings.at(url);	  
		boost::algorithm::replace_all(template_file, "__HOST__", host);
		itsAPIQueryResponses[host+url] = template_file;
	  }
  }

  return itsAPIQueryResponses.at(host+url);
}

void EDRAPI::setSettings(const std::string& tmpldir, const APISettings& api_settings)
{
  try
	{
	  itsTemplateDir = tmpldir;
	  if(itsTemplateDir.back() != '/')
		itsTemplateDir.append("/");
	  itsAPISettings = api_settings;

	  for(auto& item : itsAPISettings)
		{
		  auto template_file = item.second;
		  template_file.insert(0, itsTemplateDir);
		  std::string content;
		  std::ifstream in(template_file.c_str());
		  if (!in)
			throw Fmi::Exception(BCP,
								 "Failed to open '" + std::string(template_file.c_str()) + "' for reading!");
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

