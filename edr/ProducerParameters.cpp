#include "ProducerParameters.h"
#include <boost/algorithm/string.hpp>
#include <macgyver/Exception.h>
#include <macgyver/StringConversion.h>

namespace SmartMet
{
namespace Plugin
{
namespace EDR
{

namespace
{
std::vector<std::string> get_parameters(const libconfig::Config& config,
                                        const std::string& producer)
{
  try
  {
    std::vector<std::string> ret;

    std::string producer_parameter_name = (std::string("producer_parameters.") + producer);
    const auto& producer_parameters = config.lookup(producer_parameter_name);
    if (!producer_parameters.isArray())
      throw Fmi::Exception(BCP,
                           "Configured value of " + producer_parameter_name + " must be an array!");

    ret.reserve(producer_parameters.getLength());
    for (int j = 0; j < producer_parameters.getLength(); j++)
      ret.push_back(producer_parameters[j]);

    return ret;
  }
  catch (...)
  {
    throw Fmi::Exception::Trace(BCP, "Operation failed!");
  }
}

void parse_parameters(const std::string& producer,
                      const std::vector<std::string>& raw_params,
                      std::map<std::string, std::map<std::string, std::string>>& parsed_params)
{
  try
  {
    std::map<std::string, std::string> parameter_names;
    for (const auto& raw_param : raw_params)
    {
      auto db_name = Fmi::ascii_tolower_copy(raw_param);
      auto given_name = db_name;
      auto given_name_index = db_name.find(" as ");
      if (given_name_index != std::string::npos)
      {
        given_name = given_name.substr(given_name_index + 4);
        db_name.resize(given_name_index);
      }
      boost::algorithm::trim(db_name);
      boost::algorithm::trim(given_name);
      parameter_names[db_name] = given_name;
    }
    parsed_params[producer] = parameter_names;
  }
  catch (...)
  {
    throw Fmi::Exception::Trace(BCP, "Operation failed!");
  }
}

}  // namespace

void ProducerParameters::init(const libconfig::Config& config)
{
  try
  {
    if (config.exists("producer_parameters"))
    {
      const auto& producers = config.lookup("producer_parameters");

      for (int i = 0; i < producers.getLength(); ++i)
      {
        const auto* producer = producers[i].getName();
        auto producer_params = get_parameters(config, producer);
        parse_parameters(producer, producer_params, itsProducerParameters);
      }
    }
  }
  catch (...)
  {
    throw Fmi::Exception::Trace(BCP, "Operation failed!");
  }
}

std::string ProducerParameters::parameterName(const std::string& producer,
                                              const std::string& parameter_db_name) const
{
  try
  {
    // If no producer defined in config -> return original name
    if (itsProducerParameters.find(producer) == itsProducerParameters.end())
      return parameter_db_name;  // cannot return const ref because this could be a temp obj

    auto db_pname = parameter_db_name;
    boost::algorithm::to_lower(db_pname);
    boost::algorithm::trim(db_pname);

    const auto& producer_parameters = itsProducerParameters.at(producer);

    // If parameter not found in the list -> return original name (or should we return empty
    // string..) Anyway isValidParameter() should be called before this function
    if (producer_parameters.find(db_pname) == producer_parameters.end())
      return parameter_db_name;

    return producer_parameters.at(db_pname);
  }
  catch (...)
  {
    throw Fmi::Exception::Trace(BCP, "Operation failed!");
  }
}

bool ProducerParameters::isValidParameter(const std::string& producer,
                                          const std::string& parameter_db_name) const
{
  try
  {
    // If producer has not been defined in config -> all parameters are considered valid
    if (itsProducerParameters.find(producer) == itsProducerParameters.end())
      return true;

    const auto& params = itsProducerParameters.at(producer);

    auto db_pname = parameter_db_name;
    boost::algorithm::to_lower(db_pname);
    boost::algorithm::trim(db_pname);

    return (params.find(db_pname) != params.end());
  }
  catch (...)
  {
    throw Fmi::Exception::Trace(BCP, "Operation failed!");
  }
}

}  // namespace EDR
}  // namespace Plugin
}  // namespace SmartMet

// ======================================================================
