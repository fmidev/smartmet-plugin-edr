// ======================================================================
/*!
 * \brief EDRQuery structure
 *
 */
// ======================================================================

#pragma once

#include <string>

namespace SmartMet
{
namespace Plugin
{
namespace EDR
{

enum class EDRQueryId{AllCollections,SpecifiedCollection,SpecifiedCollectionAllInstances,SpecifiedCollectionSpecifiedInstance,DataQuery};
 
struct EDRQuery
{
  std::string collection_id;
  std::string instance_id;
  std::string query_type;
  EDRQueryId query_id{EDRQueryId::DataQuery};
};

std::ostream& operator<<(std::ostream& out, const EDRQuery& edrQ);

}  // namespace EDR
}  // namespace Plugin
}  // namespace SmartMet

// ======================================================================
