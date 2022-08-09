// ======================================================================
/*!
 * \brief EDRQuery structure
 *
 */
// ======================================================================

#pragma once

#include <string>
#include <map>
#include <set>

namespace SmartMet
{
namespace Plugin
{
namespace EDR
{

enum class EDRQueryType{Position,Radius,Area,Cube,Trajectory,Corridor,Items,Locations,Instances,InvalidQueryType};
enum class EDRQueryId{AllCollections,SpecifiedCollection,SpecifiedCollectionAllInstances,SpecifiedCollectionSpecifiedInstance,SpecifiedCollectionLocations,DataQuery};
#define DEFAULT_DATA_QUERIES "default_data_queries"
 
struct EDRQuery
{
  std::string host;
  std::string collection_id;
  std::string instance_id;
  std::map<std::string, std::set<EDRQueryType>> data_queries;
  EDRQueryId query_id{EDRQueryId::DataQuery};
};

std::ostream& operator<<(std::ostream& out, const EDRQuery& edrQ);

}  // namespace EDR
}  // namespace Plugin
}  // namespace SmartMet

// ======================================================================
