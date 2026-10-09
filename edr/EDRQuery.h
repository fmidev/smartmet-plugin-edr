// ======================================================================
/*!
 * \brief EDRQuery structure
 *
 */
// ======================================================================

#pragma once

#include <map>
#include <optional>
#include <set>
#include <string>

namespace SmartMet
{
namespace Plugin
{
namespace EDR
{
#define DEFAULT_DATA_QUERIES "default_data_queries"
#define DEFAULT_OUTPUT_FORMATS "default_output_formats"
#define DEFAULT_OUTPUT_FORMAT "default_output_format"
#define DEFAULT_LOCATIONS_KEYWORD "synop_fi"
#define DEFAULT_PRODUCER_KEY "default"
#define DEFAULT_LICENSE "default_license"

enum class EDRQueryType
{
  Position,
  Radius,
  Area,
  Cube,
  Trajectory,
  Corridor,
  Items,
  Locations,
  Instances,
  InvalidQueryType
};
enum class EDRQueryId
{
  AllCollections,
  SpecifiedCollection,
  SpecifiedCollectionAllInstances,
  SpecifiedCollectionSpecifiedInstance,
  SpecifiedCollectionLocations,
  DataQuery,
  APIQuery
};

struct EDRQuery
{
  std::string host;
  std::string collection_id;
  std::string instance_id;
  std::string location_id;
  EDRQueryType query_type{EDRQueryType::InvalidQueryType};  // query type in the request
  EDRQueryId query_id{EDRQueryId::DataQuery};
  std::string language;
  // Paging of the locations listing (OGC API-EDR limit parameter), no limit by default
  std::optional<std::size_t> limit;
  std::size_t offset = 0;
  // Request URLs of a data query for each output format of the collection (for links)
  std::map<std::string, std::string> format_urls;
};

std::string to_string(EDRQueryType query_type);
std::string to_string(EDRQueryId id);
EDRQueryType to_query_type_id(const std::string &query_type);

std::ostream &operator<<(std::ostream &out, const EDRQuery &edrQ);

}  // namespace EDR
}  // namespace Plugin
}  // namespace SmartMet

// ======================================================================
