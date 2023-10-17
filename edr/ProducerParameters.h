// ======================================================================
/*!
 * \brief ProducerParameters read from config
 */
// ======================================================================

#pragma once

#include <map>
#include <libconfig.h++>

namespace SmartMet
{
namespace Plugin
{
namespace EDR
{

class ProducerParameters
{
public:
  ProducerParameters() = default;
  void init(const libconfig::Config& config);

  bool parametersDefined(const std::string& producer) const { return (itsProducerParameters.find(producer) != itsProducerParameters.end()); }
  bool isValidParameter(const std::string& producer, const std::string&  parameter_db_name) const;
  const std::string& parameterName(const std::string& producer, const std::string& parameter_db_name) const;
  
private:
  std::map<std::string, std::map<std::string, std::string>> itsProducerParameters; // producer name -> parameter name in db -> given name
};

}  // namespace EDR
}  // namespace Plugin
}  // namespace SmartMet

// ======================================================================
