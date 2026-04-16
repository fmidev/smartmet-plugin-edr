// ======================================================================
/*!
 * \brief SmartMet EDR plugin implementation
 */
// ======================================================================

#include "Plugin.h"
#include <macgyver/Exception.h>
#include <spine/Convenience.h>
#include <spine/Reactor.h>
#include <spine/SmartMet.h>
#include <functional>
#include <iostream>

namespace SmartMet
{
namespace Plugin
{
namespace EDR
{

// ----------------------------------------------------------------------
/*!
 * \brief Plugin constructor
 */
// ----------------------------------------------------------------------

Plugin::Plugin(Spine::Reactor* theReactor, const char* theConfig)
    : itsModuleName("EDR"), itsConfigFile(theConfig), itsReactor(theReactor)
{
  try
  {
    if (theReactor->getRequiredAPIVersion() != SMARTMET_API_VERSION)
    {
      std::cerr << "*** EDRPlugin and Server SmartMet API version mismatch ***\n";
      return;
    }
  }
  catch (...)
  {
    throw Fmi::Exception::Trace(BCP, "Operation failed!");
  }
}

// ----------------------------------------------------------------------
/*!
 * \brief Initialize in a separate thread due to PostGIS slowness
 */
// ----------------------------------------------------------------------

void Plugin::init()
{
  try
  {
    // Create and initialize new PluginImpl (may throw; leaves current impl unchanged if it does)
    auto new_impl = std::make_shared<PluginImpl>(itsReactor, itsConfigFile);
    new_impl->init();

    // Atomically store new impl so request handlers see it immediately
    plugin_impl.store(new_impl);

    // Re-register content handlers (handles URL changes between reloads)
    itsReactor->removeContentHandlers(this);

    if (!itsReactor->addContentHandler(
            this,
            new_impl->itsConfig.defaultUrl(),
            [this](Spine::Reactor& theReactor,
                   const Spine::HTTP::Request& theRequest,
                   Spine::HTTP::Response& theResponse)
            { callRequestHandler(theReactor, theRequest, theResponse); },
            {},
            true))
      throw Fmi::Exception(BCP, "Failed to register edr content handler");

    if (!new_impl->itsConfig.timeSeriesUrl().empty())
    {
      if (!itsReactor->addContentHandler(
              this,
              new_impl->itsConfig.timeSeriesUrl(),
              [this](Spine::Reactor& theReactor,
                     const Spine::HTTP::Request& theRequest,
                     Spine::HTTP::Response& theResponse)
              { plugin_impl.load()->timeSeriesRequestHandler(theReactor, theRequest, theResponse); },
              {},
              false))
        throw Fmi::Exception(BCP, "Failed to register timeseries content handler");
    }

    // Register admin request handler for forced reload (authenticated)
    using AdminRequestAccess = Spine::ContentHandlerMap::AdminRequestAccess;
    itsReactor->addAdminBoolRequestHandler(
        this,
        "edr:reload",
        AdminRequestAccess::RequiresAuthentication,
        [this](Spine::Reactor&, const Spine::HTTP::Request&) -> bool { return reload(); },
        "Reloads the EDR plugin configuration");

    ensureUpdateLoopStarted();
  }
  catch (...)
  {
    throw Fmi::Exception::Trace(BCP, "Operation failed!");
  }
}

// ----------------------------------------------------------------------
/*!
 * \brief Reload the plugin from the config file
 */
// ----------------------------------------------------------------------

bool Plugin::reload()
{
  if (itsReloading.exchange(true))
    return false;  // Another reload already in progress

  try
  {
    std::cout << Spine::log_time_str() << " [EDR] [INFO] Plugin reload started\n";
    init();
    std::cout << Spine::log_time_str() << " [EDR] [INFO] Plugin reload succeeded\n";
  }
  catch (...)
  {
    itsReloading = false;
    Fmi::Exception ex(BCP, "EDR plugin reload failed", nullptr);
    ex.printError();
    return false;
  }
  itsReloading = false;
  return true;
}

// ----------------------------------------------------------------------
/*!
 * \brief Shutdown the plugin
 */
// ----------------------------------------------------------------------

void Plugin::shutdown()
{
  try
  {
    std::cout << "  -- Shutdown requested (EDR)\n";
    stopUpdateLoop();
    auto impl = plugin_impl.load();
    if (impl)
      impl->shutdown();
  }
  catch (...)
  {
    throw Fmi::Exception::Trace(BCP, "Operation failed!");
  }
}

// ----------------------------------------------------------------------
/*!
 * \brief Destructor
 */
// ----------------------------------------------------------------------

Plugin::~Plugin()
{
  stopUpdateLoop();
}

// ----------------------------------------------------------------------
/*!
 * \brief Return the plugin name
 */
// ----------------------------------------------------------------------

const std::string& Plugin::getPluginName() const
{
  return itsModuleName;
}

// ----------------------------------------------------------------------
/*!
 * \brief Return the required version
 */
// ----------------------------------------------------------------------

int Plugin::getRequiredAPIVersion() const
{
  return SMARTMET_API_VERSION;
}

// ----------------------------------------------------------------------
/*!
 * \brief Return true if the plugin has been initialized
 */
// ----------------------------------------------------------------------

bool Plugin::ready() const
{
  auto impl = plugin_impl.load();
  return impl && impl->itsReady;
}

Fmi::Cache::CacheStatistics Plugin::getCacheStats() const
{
  return plugin_impl.load()->getCacheStats();
}

// ----------------------------------------------------------------------
/*!
 * \brief Main content handler - delegates to current PluginImpl
 */
// ----------------------------------------------------------------------

void Plugin::requestHandler(Spine::Reactor& theReactor,
                            const Spine::HTTP::Request& theRequest,
                            Spine::HTTP::Response& theResponse)
{
  plugin_impl.load()->requestHandler(theReactor, theRequest, theResponse);
}

// ----------------------------------------------------------------------
/*!
 * \brief Background loop: polls config file for changes and triggers reload
 */
// ----------------------------------------------------------------------

void Plugin::updateLoop()
{
  auto updateCheck = [this]() -> bool
  {
    std::unique_lock<std::mutex> lock(itsUpdateNotifyMutex);
    if (!itsShuttingDown)
      itsUpdateNotifyCond.wait_for(lock, std::chrono::seconds(60));
    return !itsShuttingDown;
  };

  try
  {
    while (updateCheck())
    {
      try
      {
        auto impl = plugin_impl.load();
        if (impl && impl->itsConfig.enableConfigurationPolling() && impl->isReloadRequired())
        {
          bool ok = reload();
          std::cout << Spine::log_time_str() << " [EDR] [INFO] Auto-reload "
                    << (ok ? "succeeded" : "failed") << "\n";
        }
      }
      catch (...)
      {
        Fmi::Exception ex(BCP, "EDR configuration polling failed", nullptr);
        ex.printError();
      }
    }
  }
  catch (...)
  {
    throw Fmi::Exception::Trace(BCP, "Update loop failed!");
  }
}

void Plugin::ensureUpdateLoopStarted()
{
  if (!itsShuttingDown)
  {
    std::unique_lock<std::mutex> lock(itsUpdateNotifyMutex);
    if (!itsUpdateLoopThread)
    {
      itsUpdateLoopThread.reset(
          new std::thread(std::bind(&Plugin::updateLoop, this)));
    }
  }
}

void Plugin::stopUpdateLoop()
{
  itsShuttingDown = true;
  std::unique_ptr<std::thread> tmp;
  {
    std::unique_lock<std::mutex> lock(itsUpdateNotifyMutex);
    std::swap(tmp, itsUpdateLoopThread);
    itsUpdateNotifyCond.notify_all();
  }
  if (tmp && tmp->joinable())
    tmp->join();
  itsShuttingDown = false;
}

}  // namespace EDR
}  // namespace Plugin
}  // namespace SmartMet

/*
 * Server knows us through the 'SmartMetPlugin' virtual interface, which
 * the 'Plugin' class implements.
 */

extern "C" SmartMetPlugin* create(SmartMet::Spine::Reactor* them, const char* config)
{
  return new SmartMet::Plugin::EDR::Plugin(them, config);
}

extern "C" void destroy(SmartMetPlugin* us)
{
  // This will call 'Plugin::~Plugin()' since the destructor is virtual
  delete us;
}

// ======================================================================
