// ======================================================================
/*!
 * \brief Smartmet EDR plugin interface
 */
// ======================================================================

#pragma once

#include "PluginImpl.h"
#include <macgyver/AtomicSharedPtr.h>
#include <spine/SmartMetPlugin.h>
#include <atomic>
#include <condition_variable>
#include <memory>
#include <mutex>
#include <string>
#include <thread>

namespace SmartMet
{
namespace Plugin
{
namespace EDR
{

class Plugin : public SmartMetPlugin
{
 public:
  Plugin() = delete;
  Plugin(const Plugin& other) = delete;
  Plugin& operator=(const Plugin& other) = delete;
  Plugin(Plugin&& other) = delete;
  Plugin& operator=(Plugin&& other) = delete;
  Plugin(Spine::Reactor* theReactor, const char* theConfig);
  ~Plugin() override;

  const std::string& getPluginName() const override;
  int getRequiredAPIVersion() const override;

  bool ready() const;

  // Trigger a reload of the plugin configuration; returns true on success.
  bool reload();

 protected:
  void init() override;
  void shutdown() override;
  void requestHandler(Spine::Reactor& theReactor,
                      const Spine::HTTP::Request& theRequest,
                      Spine::HTTP::Response& theResponse) override;

 private:
  Fmi::Cache::CacheStatistics getCacheStats() const override;

  void updateLoop();
  void ensureUpdateLoopStarted();
  void stopUpdateLoop();

  const std::string itsModuleName;
  const char* itsConfigFile;
  Spine::Reactor* itsReactor = nullptr;

  Fmi::AtomicSharedPtr<PluginImpl> plugin_impl;

  std::atomic<bool> itsShuttingDown{false};
  std::atomic<bool> itsReloading{false};
  std::unique_ptr<std::thread> itsUpdateLoopThread;
  std::condition_variable itsUpdateNotifyCond;
  std::mutex itsUpdateNotifyMutex;

};  // class Plugin

}  // namespace EDR
}  // namespace Plugin
}  // namespace SmartMet

// ======================================================================
