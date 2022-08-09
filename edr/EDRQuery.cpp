#include "EDRQuery.h"
#include <iostream>

namespace SmartMet
{
namespace Plugin
{
namespace EDR
{

std::string to_string(const EDRQueryId& id)
{
  if(id == EDRQueryId::AllCollections)
    return "EDRQueryId::AllCollections";
  if(id == EDRQueryId::SpecifiedCollection)
    return "EDRQueryId::SpecifiedCollection";
  if(id == EDRQueryId::SpecifiedCollectionAllInstances)
    return "EDRQueryId::SpecifiedCollectionAllInstances";
  if(id == EDRQueryId::SpecifiedCollectionSpecifiedInstance)
    return "EDRQueryId::SpecifiedCollectionSpecifiedInstance";
  if(id == EDRQueryId::SpecifiedCollectionLocations)
    return "EDRQueryId::SpecifiedCollectionLocations";
  if(id == EDRQueryId::DataQuery)
    return "EDRQueryId::DataQuery";
  
  return "";
}
  
std::ostream& operator<<(std::ostream& out, const EDRQuery& edrQ)
{
  out << "EDR query:"
      << "\nhost: " << edrQ.host 
      << "\ncollection_id: " << edrQ.collection_id 
      << "\ninstance_id: " << edrQ.instance_id
      << "\nquery_id: " << to_string(edrQ.query_id) << std::endl;

  return out;
}

}  // namespace EDR
}  // namespace Plugin
}  // namespace SmartMet

// ======================================================================
