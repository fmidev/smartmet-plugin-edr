// ======================================================================
/*!
 * \brief Coordinate reference systems supported by the crs parameter
 */
// ======================================================================

#pragma once

#include <memory>
#include <optional>
#include <string>
#include <vector>

namespace Fmi
{
class SpatialReference;
class CoordinateTransformation;
}  // namespace Fmi

namespace SmartMet
{
namespace Plugin
{
namespace EDR
{
// A supported CRS other than WGS84 longitude/latitude (CRS84)
struct CRSInfo
{
  std::string id;    // e.g. EPSG:3067
  std::string uri;   // e.g. http://www.opengis.net/def/crs/EPSG/0/3067
  std::string wkt;   // WKT definition for the collection metadata
  bool geographic = false;
  bool swapped = false;  // authority axis order is latitude/longitude or northing/easting

  // Transformations are created for each use since PROJ objects must not be shared by
  // threads, the gis library caches the spatial references. Internally the coordinates are
  // in the x/y (longitude/latitude, easting/northing) order.

  // Converts WKT coordinates given in the authority axis order to the x/y order
  std::string toXY(const std::string& wkt) const;

  // Transforms a WKT geometry in this CRS (x/y order) to WGS84 longitude/latitude
  std::string toWGS84(const std::string& wkt) const;

  // Transforms coordinates between WGS84 longitude/latitude and this CRS (x/y order)
  void fromWGS84(std::vector<double>& x, std::vector<double>& y) const;
  void toWGS84(std::vector<double>& x, std::vector<double>& y) const;
};

using CRSInfoPtr = std::shared_ptr<const CRSInfo>;

class CRSRegistry
{
 public:
  // The configured CRSs, e.g. EPSG:3067
  void init(const std::vector<std::string>& crs_list);

  // The CRS matching the identifier (EPSG:n, an OGC CRS URI or URN), nullptr if it is not
  // supported. CRS84 identifiers are not handled here.
  CRSInfoPtr find(const std::string& crs) const;

  const std::vector<CRSInfoPtr>& all() const { return itsCRSs; }

 private:
  std::vector<CRSInfoPtr> itsCRSs;
};

// True for the identifiers of WGS84 longitude/latitude
bool is_crs84(const std::string& crs);

}  // namespace EDR
}  // namespace Plugin
}  // namespace SmartMet
