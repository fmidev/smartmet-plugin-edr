// ======================================================================
/*!
 * \brief Collection info
 */
// ======================================================================

#pragma once

#include <string>
#include <set>
#include <map>

namespace SmartMet
{
namespace Plugin
{
namespace EDR
{
struct CollectionInfo
{
  CollectionInfo() : title(""), description("") {}
  std::string title;
  std::string description;
  std::set<std::string> keywords;
};

using CollectionInfoItems = std::map<std::string, CollectionInfo>;

class CollectionInfoContainer
{
public:
  CollectionInfoContainer();
  void addInfo(const std::string& theSource, const std::string& theId,const std::string& theTitle,const std::string& theDescription, const std::set<std::string>& theKeywords);
  const CollectionInfo& getInfo(const std::string& theSource, const std::string& theId) const;

 private:
  std::map<std::string, CollectionInfoItems> itsData; // source -> collectionId -> info, e.g. querydata_engine -> pal_skandinavia -> info
};

}  // namespace EDR
}  // namespace Plugin
}  // namespace SmartMet

// ======================================================================
