// ======================================================================
/*!
 * \brief Collection info
 */
// ======================================================================

#pragma once

#include "EDRDefs.h"
#include <map>
#include <set>
#include <string>

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
  void addInfo(SourceEngine theSource,
               const std::string& theId,
               const std::string& theTitle,
               const std::string& theDescription,
               const std::set<std::string>& theKeywords);
  void addVisibleCollections(SourceEngine theSource, const std::set<std::string>& theCollections);
  const CollectionInfo& getInfo(SourceEngine theSource, const std::string& theId) const;
  bool isVisibleCollection(SourceEngine theSource, const std::string& theCollectionName) const;

 private:
  std::map<SourceEngine, CollectionInfoItems>
      itsData;  // source -> collectionId -> info, e.g. querydata_engine -> pal_skandinavia -> info
  std::map<SourceEngine, std::set<std::string>>
      itsVisibleCollections;  // source -> set of visible collections
};

}  // namespace EDR
}  // namespace Plugin
}  // namespace SmartMet

// ======================================================================
