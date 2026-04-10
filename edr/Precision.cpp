#include "Precision.h"
#include <macgyver/Exception.h>
#include <macgyver/StringConversion.h>
#include <spine/ConfigBase.h>
#include <spine/Exceptions.h>

using SmartMet::Plugin::EDR::Precision;

namespace
{
    int get_int(const libconfig::Setting& setting)
    {
        try
        {
            return setting;
        }
        catch (...)
        {
            std::ostringstream msg;
            msg << "Expected an integer value for setting "
                << setting.getPath() << " but got '";
            SmartMet::Spine::ConfigBase::dump_setting(msg, setting);
            msg << "'";
            throw Fmi::Exception::Trace(BCP, msg.str());
        }
    }
}

Precision::Precision(const libconfig::Setting &settings)
{
  try
  {
    if (!settings.isGroup())
      throw Fmi::Exception(BCP,
                           "Precision settings for point forecasts must "
                           "be stored in groups delimited by {}: line " +
                               Fmi::to_string(settings.getSourceLine()));

    for (int i = 0; i < settings.getLength(); ++i)
    {
      const std::string paramname = settings[i].getName();

      try
      {
        if (paramname == "default")
        {
          const int value = get_int(settings[i]);
          default_precision = value;
        }
        else if (paramname == "timeseries")
        {
          if (!settings[i].isGroup())
            throw Fmi::Exception(BCP,
                                 "Timeseries precision overrides for point forecasts must "
                                 "be stored in groups delimited by {}: line " +
                                     Fmi::to_string(settings[i].getSourceLine()));
          // timeseries precision overrides are stored in a separate map in the Precision struct
          for (int j = 0; j < settings[i].getLength(); ++j)
          {
            const std::string ts_paramname = settings[i][j].getName();
            if (ts_paramname == "default")
            {
              const int value = get_int(settings[i][j]);
              default_timeseries_precision = value;
            }

            try
            {
              const int ts_value = get_int(settings[i][j]);
              ts_parameter_precisions_overrides.insert(
                  Precision::Map::value_type(ts_paramname, ts_value));
            }
            catch (...)
            {
              Spine::Exceptions::handle("EDR plugin");
            }
          }
        }
        else
        {
          const int value = get_int(settings[i]);
          parameter_precisions.insert(Precision::Map::value_type(paramname, value));
        }
      }
      catch (...)
      {
        Spine::Exceptions::handle("EDR plugin");
      }
    }
  }
  catch (...)
  {
    throw Fmi::Exception::Trace(BCP, "Operation failed!");
  }
}


int Precision::get_precision(const std::string& parameter_name, bool is_timeseries_query) const
{
    if (is_timeseries_query)
    {
        // Check for timeseries-specific override first
        auto it_override = ts_parameter_precisions_overrides.find(parameter_name);
        if (it_override != ts_parameter_precisions_overrides.end())
            return it_override->second;
    }

    // Check for general parameter-specific precision
    auto it = parameter_precisions.find(parameter_name);
    if (it != parameter_precisions.end())
        return it->second;

    // Fall back to default timeseries precision if it's a timeseries query and
    // default_timeseries_precision is set
    if (is_timeseries_query && default_timeseries_precision)
        return *default_timeseries_precision;

    // Finally, fall back to default precision
    return default_precision;
}

