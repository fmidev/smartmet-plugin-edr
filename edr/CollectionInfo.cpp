#include "CollectionInfo.h"
#include <iostream>


namespace SmartMet
{
namespace Plugin
{
namespace EDR
{
static CollectionInfo EMPTY_COLLECTION_INFO;

CollectionInfoContainer::CollectionInfoContainer()
{
}

void CollectionInfoContainer::addInfo(SourceEngine theSource, const std::string& theId,const std::string& theTitle,const std::string& theDescription, const std::set<std::string>& theKeywords)
{
  CollectionInfoItems& info_items = itsData[theSource];
  CollectionInfo& info = info_items[theId];
  info.title = theTitle;
  info.description = theDescription;
  info.keywords = theKeywords;
}

void CollectionInfoContainer::addVisibleCollections(SourceEngine theSource, const std::set<std::string>& theCollections)
{
  itsVisibleCollections[theSource] = theCollections;
}

const CollectionInfo& CollectionInfoContainer::getInfo(SourceEngine theSource, const std::string& theId) const
{
  if(itsData.find(theSource) == itsData.end())
	{
	  return EMPTY_COLLECTION_INFO;
	}

  const CollectionInfoItems& info_items = itsData.at(theSource);

  if(info_items.find(theId) != info_items.end())
	return info_items.at(theId);

  return EMPTY_COLLECTION_INFO;
}

bool CollectionInfoContainer::isVisibleCollection(SourceEngine theSource, const std::string& theCollectionName) const
{
  if(itsVisibleCollections.find(theSource) == itsVisibleCollections.end())
	return false;

  const auto& visible_collections = itsVisibleCollections.at(theSource);

  return (visible_collections.find(theCollectionName) != visible_collections.end() || visible_collections.find("*") != visible_collections.end());
}

}  // namespace EDR
}  // namespace Plugin
}  // namespace SmartMet

// ======================================================================
