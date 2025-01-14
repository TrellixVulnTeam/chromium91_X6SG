// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "headless/lib/browser/policy/headless_browser_policy_connector.h"

#include <memory>
#include <string>
#include <utility>

#include "base/bind.h"
#include "base/callback.h"
#include "base/task/post_task.h"
#include "base/task/thread_pool.h"
#include "build/branding_buildflags.h"
#include "build/build_config.h"
#include "components/policy/core/common/async_policy_provider.h"
#include "components/policy/policy_constants.h"
#include "headless/lib/browser/policy/headless_mode_policy.h"

#if defined(OS_WIN)
#include "base/win/registry.h"
#include "components/policy/core/common/policy_loader_win.h"
#elif defined(OS_MAC)
#include <CoreFoundation/CoreFoundation.h>
#include "base/mac/foundation_util.h"
#include "base/strings/sys_string_conversions.h"
#include "components/policy/core/common/policy_loader_mac.h"
#include "components/policy/core/common/preferences_mac.h"
#elif defined(OS_POSIX) && !defined(OS_ANDROID)
#include "components/policy/core/common/config_dir_policy_loader.h"
#endif

namespace policy {

namespace {

void PopulatePolicyHandlerParameters(PolicyHandlerParameters* parameters) {}

std::unique_ptr<ConfigurationPolicyHandlerList> BuildHandlerList(
    const Schema& chrome_schema) {
  auto handlers = std::make_unique<ConfigurationPolicyHandlerList>(
      base::BindRepeating(&PopulatePolicyHandlerParameters),
      base::BindRepeating(&GetChromePolicyDetails),
      /*allow_future_policies=*/false);

  handlers->AddHandler(std::make_unique<HeadlessModePolicyHandler>());

  return handlers;
}

}  // namespace

HeadlessBrowserPolicyConnector::HeadlessBrowserPolicyConnector()
    : BrowserPolicyConnector(base::BindRepeating(&BuildHandlerList)) {}

HeadlessBrowserPolicyConnector::~HeadlessBrowserPolicyConnector() = default;

scoped_refptr<PrefStore> HeadlessBrowserPolicyConnector::CreatePrefStore(
    policy::PolicyLevel policy_level) {
  return base::MakeRefCounted<policy::ConfigurationPolicyPrefStore>(
      this, GetPolicyService(), GetHandlerList(), policy_level);
}

void HeadlessBrowserPolicyConnector::Init(
    PrefService* local_state,
    scoped_refptr<network::SharedURLLoaderFactory> url_loader_factory) {}

bool HeadlessBrowserPolicyConnector::IsEnterpriseManaged() const {
  return false;
}

bool HeadlessBrowserPolicyConnector::IsCommandLineSwitchSupported() const {
  return false;
}

bool HeadlessBrowserPolicyConnector::HasMachineLevelPolicies() {
  return ProviderHasPolicies(GetPlatformProvider());
}

ConfigurationPolicyProvider*
HeadlessBrowserPolicyConnector::GetPlatformProvider() {
  ConfigurationPolicyProvider* provider =
      BrowserPolicyConnectorBase::GetPolicyProviderForTesting();
  return provider ? provider : platform_provider_;
}

std::vector<std::unique_ptr<policy::ConfigurationPolicyProvider>>
HeadlessBrowserPolicyConnector::CreatePolicyProviders() {
  auto providers = BrowserPolicyConnector::CreatePolicyProviders();
  if (!BrowserPolicyConnectorBase::GetPolicyProviderForTesting()) {
    std::unique_ptr<ConfigurationPolicyProvider> platform_provider =
        CreatePlatformProvider();
    if (platform_provider) {
      platform_provider_ = platform_provider.get();
      // PlatformProvider should be before all other providers (highest
      // priority).
      providers.insert(providers.begin(), std::move(platform_provider));
    }
  }
  return providers;
}

std::unique_ptr<ConfigurationPolicyProvider>
HeadlessBrowserPolicyConnector::CreatePlatformProvider() {
#if defined(OS_WIN)
  std::unique_ptr<AsyncPolicyLoader> loader(PolicyLoaderWin::Create(
      base::ThreadPool::CreateSequencedTaskRunner(
          {base::MayBlock(), base::TaskPriority::BEST_EFFORT}),
      kRegistryChromePolicyKey));
  return std::make_unique<AsyncPolicyProvider>(GetSchemaRegistry(),
                                               std::move(loader));
#elif defined(OS_MAC)
#if BUILDFLAG(GOOGLE_CHROME_BRANDING)
  // Explicitly watch the "com.google.Chrome" bundle ID, no matter what this
  // app's bundle ID actually is. All channels of Chrome should obey the same
  // policies.
  CFStringRef bundle_id = CFSTR("com.google.Chrome");
#else
  base::ScopedCFTypeRef<CFStringRef> bundle_id(
      base::SysUTF8ToCFStringRef(base::mac::BaseBundleID()));
#endif
  auto loader = std::make_unique<PolicyLoaderMac>(
      base::ThreadPool::CreateSequencedTaskRunner(
          {base::MayBlock(), base::TaskPriority::BEST_EFFORT}),
      policy::PolicyLoaderMac::GetManagedPolicyPath(bundle_id),
      new MacPreferences(), bundle_id);
  return std::make_unique<AsyncPolicyProvider>(GetSchemaRegistry(),
                                               std::move(loader));
#elif defined(OS_POSIX)
  // The following should match chrome::DIR_POLICY_FILES definition in
  // chrome/common/chrome_paths.cc
#if BUILDFLAG(GOOGLE_CHROME_BRANDING)
  base::FilePath config_dir_path(FILE_PATH_LITERAL("/etc/opt/chrome/policies"));
#else
  base::FilePath config_dir_path(FILE_PATH_LITERAL("/etc/chromium/policies"));
#endif
  std::unique_ptr<AsyncPolicyLoader> loader(new ConfigDirPolicyLoader(
      base::ThreadPool::CreateSequencedTaskRunner(
          {base::MayBlock(), base::TaskPriority::BEST_EFFORT}),
      config_dir_path, POLICY_SCOPE_MACHINE));
  return std::make_unique<AsyncPolicyProvider>(GetSchemaRegistry(),
                                               std::move(loader));
#else
  return nullptr;
#endif
}

}  // namespace policy