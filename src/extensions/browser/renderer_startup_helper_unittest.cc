// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "extensions/browser/renderer_startup_helper.h"

#include "base/stl_util.h"
#include "components/crx_file/id_util.h"
#include "content/public/test/mock_render_process_host.h"
#include "extensions/browser/extension_prefs.h"
#include "extensions/browser/extension_registry.h"
#include "extensions/browser/extension_registry_factory.h"
#include "extensions/browser/extension_util.h"
#include "extensions/browser/extensions_test.h"
#include "extensions/browser/test_extensions_browser_client.h"
#include "extensions/common/extension_builder.h"
#include "extensions/common/extension_messages.h"
#include "extensions/common/mojom/renderer.mojom.h"
#include "extensions/common/permissions/permissions_data.h"
#include "mojo/public/cpp/bindings/associated_receiver_set.h"

namespace extensions {

// Class that implements the binding of a new Renderer mojom interface and
// can receive callbacks on it for testing validation.
class InterceptingRendererStartupHelper : public RendererStartupHelper,
                                          public mojom::Renderer {
 public:
  explicit InterceptingRendererStartupHelper(
      content::BrowserContext* browser_context)
      : RendererStartupHelper(browser_context) {}

  size_t num_activated_extensions() { return activated_extensions_.size(); }

  size_t num_unloaded_extensions() { return unloaded_extensions_.size(); }

  const URLPatternSet& default_policy_blocked_hosts() const {
    return default_blocked_hosts_;
  }

  const URLPatternSet& default_policy_allowed_hosts() const {
    return default_allowed_hosts_;
  }

 protected:
  mojo::PendingAssociatedRemote<mojom::Renderer> BindNewRendererRemote(
      content::RenderProcessHost* process) override {
    mojo::AssociatedRemote<mojom::Renderer> remote;
    receivers_.Add(this, remote.BindNewEndpointAndPassDedicatedReceiver());
    return remote.Unbind();
  }

 private:
  // mojom::Renderer implementation:
  void ActivateExtension(const std::string& extension_id) override {
    activated_extensions_.push_back(extension_id);
  }
  void SetActivityLoggingEnabled(bool enabled) override {}

  void UnloadExtension(const std::string& extension_id) override {
    unloaded_extensions_.push_back(extension_id);
  }

  void SuspendExtension(
      const std::string& extension_id,
      mojom::Renderer::SuspendExtensionCallback callback) override {
    std::move(callback).Run();
  }

  void CancelSuspendExtension(const std::string& extension_id) override {}

  void SetSessionInfo(version_info::Channel channel,
                      mojom::FeatureSessionType session,
                      bool is_lock_screen_context) override {}
  void SetSystemFont(const std::string& font_family,
                     const std::string& font_size) override {}

  void SetWebViewPartitionID(const std::string& partition_id) override {}

  void SetScriptingAllowlist(
      const std::vector<std::string>& extension_ids) override {}

  void ShouldSuspend(ShouldSuspendCallback callback) override {
    std::move(callback).Run();
  }

  void TransferBlobs(TransferBlobsCallback callback) override {
    std::move(callback).Run();
  }

  void UpdateDefaultPolicyHostRestrictions(
      const URLPatternSet& default_policy_blocked_hosts,
      const URLPatternSet& default_policy_allowed_hosts) override {
    default_blocked_hosts_.AddPatterns(default_policy_blocked_hosts);
    default_allowed_hosts_.AddPatterns(default_policy_allowed_hosts);
  }

  void UpdateTabSpecificPermissions(const std::string& extension_id,
                                    const URLPatternSet& new_hosts,
                                    int tab_id,
                                    bool update_origin_whitelist) override {}

  void UpdateUserScripts(base::ReadOnlySharedMemoryRegion shared_memory,
                         mojom::HostIDPtr host_id,
                         std::vector<mojom::HostIDPtr> changed_hosts,
                         bool allowlisted_only) override {}

  void ClearTabSpecificPermissions(
      const std::vector<std::string>& extension_ids,
      int tab_id,
      bool update_origin_whitelist) override {}

  void WatchPages(const std::vector<std::string>& css_selectors) override {}

  URLPatternSet default_blocked_hosts_;
  URLPatternSet default_allowed_hosts_;
  std::vector<std::string> activated_extensions_;
  std::vector<std::string> unloaded_extensions_;
  mojo::AssociatedReceiverSet<mojom::Renderer> receivers_;
};

class RendererStartupHelperTest : public ExtensionsTest {
 public:
  RendererStartupHelperTest() {}
  ~RendererStartupHelperTest() override {}

  void SetUp() override {
    ExtensionsTest::SetUp();
    helper_ =
        std::make_unique<InterceptingRendererStartupHelper>(browser_context());
    registry_ =
        ExtensionRegistryFactory::GetForBrowserContext(browser_context());
    render_process_host_ =
        std::make_unique<content::MockRenderProcessHost>(browser_context());
    incognito_render_process_host_ =
        std::make_unique<content::MockRenderProcessHost>(incognito_context());
    extension_ = CreateExtension("ext_1");
  }

  void TearDown() override {
    render_process_host_.reset();
    incognito_render_process_host_.reset();
    helper_.reset();
    ExtensionsTest::TearDown();
  }

 protected:
  void SimulateRenderProcessCreated(content::RenderProcessHost* rph) {
    helper_->OnRenderProcessHostCreated(rph);
  }

  void SimulateRenderProcessTerminated(content::RenderProcessHost* rph) {
    helper_->RenderProcessHostDestroyed(rph);
  }

  scoped_refptr<const Extension> CreateExtension(const std::string& id_input) {
    std::unique_ptr<base::DictionaryValue> manifest =
        DictionaryBuilder()
            .Set("name", "extension")
            .Set("description", "an extension")
            .Set("manifest_version", 2)
            .Set("version", "0.1")
            .Build();
    return CreateExtension(id_input, std::move(manifest));
  }

  scoped_refptr<const Extension> CreateTheme(const std::string& id_input) {
    std::unique_ptr<base::DictionaryValue> manifest =
        DictionaryBuilder()
            .Set("name", "theme")
            .Set("description", "a theme")
            .Set("theme", DictionaryBuilder().Build())
            .Set("manifest_version", 2)
            .Set("version", "0.1")
            .Build();
    return CreateExtension(id_input, std::move(manifest));
  }

  scoped_refptr<const Extension> CreatePlatformApp(
      const std::string& id_input) {
    std::unique_ptr<base::Value> background =
        DictionaryBuilder()
            .Set("scripts", ListBuilder().Append("background.js").Build())
            .Build();
    std::unique_ptr<base::DictionaryValue> manifest =
        DictionaryBuilder()
            .Set("name", "platform_app")
            .Set("description", "a platform app")
            .Set("app", DictionaryBuilder()
                            .Set("background", std::move(background))
                            .Build())
            .Set("manifest_version", 2)
            .Set("version", "0.1")
            .Build();
    return CreateExtension(id_input, std::move(manifest));
  }

  void AddExtensionToRegistry(scoped_refptr<const Extension> extension) {
    registry_->AddEnabled(extension);
  }

  void RemoveExtensionFromRegistry(scoped_refptr<const Extension> extension) {
    registry_->RemoveEnabled(extension->id());
  }

  bool IsProcessInitialized(content::RenderProcessHost* rph) {
    return base::Contains(helper_->process_mojo_map_, rph);
  }

  bool IsExtensionLoaded(const Extension& extension) {
    return base::Contains(helper_->extension_process_map_, extension.id());
  }

  bool IsExtensionLoadedInProcess(const Extension& extension,
                                  content::RenderProcessHost* rph) {
    return IsExtensionLoaded(extension) &&
           base::Contains(helper_->extension_process_map_[extension.id()], rph);
  }

  bool IsExtensionPendingActivationInProcess(const Extension& extension,
                                             content::RenderProcessHost* rph) {
    return base::Contains(helper_->pending_active_extensions_, rph) &&
           base::Contains(helper_->pending_active_extensions_[rph],
                          extension.id());
  }

  std::unique_ptr<InterceptingRendererStartupHelper> helper_;
  ExtensionRegistry* registry_;  // Weak.
  std::unique_ptr<content::MockRenderProcessHost> render_process_host_;
  std::unique_ptr<content::MockRenderProcessHost>
      incognito_render_process_host_;
  scoped_refptr<const Extension> extension_;

 private:
  scoped_refptr<const Extension> CreateExtension(
      const std::string& id_input,
      std::unique_ptr<base::DictionaryValue> manifest) {
    return ExtensionBuilder()
        .SetManifest(std::move(manifest))
        .SetID(crx_file::id_util::GenerateId(id_input))
        .Build();
  }

  DISALLOW_COPY_AND_ASSIGN(RendererStartupHelperTest);
};

// Tests extension loading, unloading and activation and render process creation
// and termination.
TEST_F(RendererStartupHelperTest, NormalExtensionLifecycle) {
  // Initialize render process.
  EXPECT_FALSE(IsProcessInitialized(render_process_host_.get()));
  SimulateRenderProcessCreated(render_process_host_.get());
  EXPECT_TRUE(IsProcessInitialized(render_process_host_.get()));

  IPC::TestSink& sink = render_process_host_->sink();

  // Enable extension.
  sink.ClearMessages();
  EXPECT_FALSE(IsExtensionLoaded(*extension_));
  AddExtensionToRegistry(extension_);
  helper_->OnExtensionLoaded(*extension_);
  EXPECT_TRUE(
      IsExtensionLoadedInProcess(*extension_, render_process_host_.get()));
  EXPECT_FALSE(IsExtensionPendingActivationInProcess(
      *extension_, render_process_host_.get()));
  ASSERT_EQ(1u, sink.message_count());
  EXPECT_EQ(static_cast<uint32_t>(ExtensionMsg_Loaded::ID),
            sink.GetMessageAt(0)->type());

  // Activate extension.
  sink.ClearMessages();
  helper_->ActivateExtensionInProcess(*extension_, render_process_host_.get());
  EXPECT_FALSE(IsExtensionPendingActivationInProcess(
      *extension_, render_process_host_.get()));
  base::RunLoop().RunUntilIdle();
  ASSERT_EQ(1u, helper_->num_activated_extensions());

  // Disable extension.
  sink.ClearMessages();
  RemoveExtensionFromRegistry(extension_);
  helper_->OnExtensionUnloaded(*extension_);
  EXPECT_FALSE(IsExtensionLoaded(*extension_));
  base::RunLoop().RunUntilIdle();
  ASSERT_EQ(1u, helper_->num_unloaded_extensions());

  // Extension enabled again.
  sink.ClearMessages();
  AddExtensionToRegistry(extension_);
  helper_->OnExtensionLoaded(*extension_);
  EXPECT_TRUE(
      IsExtensionLoadedInProcess(*extension_, render_process_host_.get()));
  EXPECT_FALSE(IsExtensionPendingActivationInProcess(
      *extension_, render_process_host_.get()));
  ASSERT_EQ(1u, sink.message_count());
  EXPECT_EQ(static_cast<uint32_t>(ExtensionMsg_Loaded::ID),
            sink.GetMessageAt(0)->type());

  // Render Process terminated.
  SimulateRenderProcessTerminated(render_process_host_.get());
  EXPECT_FALSE(IsProcessInitialized(render_process_host_.get()));
  EXPECT_TRUE(IsExtensionLoaded(*extension_));
  EXPECT_FALSE(
      IsExtensionLoadedInProcess(*extension_, render_process_host_.get()));
  EXPECT_FALSE(IsExtensionPendingActivationInProcess(
      *extension_, render_process_host_.get()));
}

// Tests that activating an extension in an uninitialized render process works
// fine.
TEST_F(RendererStartupHelperTest, EnabledBeforeProcessInitialized) {
  EXPECT_FALSE(IsProcessInitialized(render_process_host_.get()));
  IPC::TestSink& sink = render_process_host_->sink();

  // Enable extension. The render process isn't initialized yet, so the
  // extension should be added to the list of extensions awaiting activation.
  sink.ClearMessages();
  AddExtensionToRegistry(extension_);
  helper_->OnExtensionLoaded(*extension_);
  helper_->ActivateExtensionInProcess(*extension_, render_process_host_.get());
  EXPECT_EQ(0u, sink.message_count());
  EXPECT_TRUE(IsExtensionLoaded(*extension_));
  EXPECT_FALSE(
      IsExtensionLoadedInProcess(*extension_, render_process_host_.get()));
  EXPECT_TRUE(IsExtensionPendingActivationInProcess(
      *extension_, render_process_host_.get()));

  // Initialize PermissionsData default policy hosts restrictions.
  // During the process initialization, UpdateDefaultPolicyHostRestrictions
  // will be called with the default policy values returned by PermissionsData.
  URLPatternSet default_blocked_hosts;
  URLPatternSet default_allowed_hosts;
  default_blocked_hosts.AddPattern(
      URLPattern(URLPattern::SCHEME_ALL, "*://*.example.com/*"));
  default_allowed_hosts.AddPattern(
      URLPattern(URLPattern::SCHEME_ALL, "*://test.example2.com/*"));
  PermissionsData::SetDefaultPolicyHostRestrictions(
      util::GetBrowserContextId(browser_context()), default_blocked_hosts,
      default_allowed_hosts);

  // Initialize the render process.
  SimulateRenderProcessCreated(render_process_host_.get());
  // The renderer would have been sent multiple initialization messages
  // including the loading and activation messages for the extension.
  EXPECT_LE(1u, sink.message_count());

  // Method UpdateDefaultPolicyHostRestrictions() from mojom::Renderer should
  // have been called with the default policy for blocked/allowed hosts given by
  // PermissionsData, which was initialized above.
  base::RunLoop().RunUntilIdle();
  EXPECT_EQ(default_blocked_hosts, helper_->default_policy_blocked_hosts());
  EXPECT_EQ(default_allowed_hosts, helper_->default_policy_allowed_hosts());

  EXPECT_TRUE(IsProcessInitialized(render_process_host_.get()));
  EXPECT_TRUE(
      IsExtensionLoadedInProcess(*extension_, render_process_host_.get()));
  EXPECT_FALSE(IsExtensionPendingActivationInProcess(
      *extension_, render_process_host_.get()));
}

// Tests that themes aren't loaded in a renderer process.
TEST_F(RendererStartupHelperTest, LoadTheme) {
  // Initialize render process.
  EXPECT_FALSE(IsProcessInitialized(render_process_host_.get()));
  SimulateRenderProcessCreated(render_process_host_.get());
  EXPECT_TRUE(IsProcessInitialized(render_process_host_.get()));

  scoped_refptr<const Extension> extension(CreateTheme("theme"));
  ASSERT_TRUE(extension->is_theme());

  IPC::TestSink& sink = render_process_host_->sink();

  // Enable the theme. Verify it isn't loaded and activated in the renderer.
  sink.ClearMessages();
  EXPECT_FALSE(IsExtensionLoaded(*extension));
  AddExtensionToRegistry(extension_);
  helper_->OnExtensionLoaded(*extension);
  EXPECT_EQ(0u, sink.message_count());
  EXPECT_TRUE(IsExtensionLoaded(*extension));
  EXPECT_FALSE(
      IsExtensionLoadedInProcess(*extension, render_process_host_.get()));

  helper_->ActivateExtensionInProcess(*extension, render_process_host_.get());
  EXPECT_EQ(0u, sink.message_count());
  EXPECT_FALSE(IsExtensionPendingActivationInProcess(
      *extension, render_process_host_.get()));

  helper_->OnExtensionUnloaded(*extension);
  EXPECT_EQ(0u, sink.message_count());
  EXPECT_FALSE(IsExtensionLoaded(*extension));
}

// Tests that only incognito-enabled extensions are loaded in an incognito
// context.
TEST_F(RendererStartupHelperTest, ExtensionInIncognitoRenderer) {
  // Initialize the incognito renderer.
  EXPECT_FALSE(IsProcessInitialized(incognito_render_process_host_.get()));
  SimulateRenderProcessCreated(incognito_render_process_host_.get());
  EXPECT_TRUE(IsProcessInitialized(incognito_render_process_host_.get()));

  IPC::TestSink& sink = render_process_host_->sink();
  IPC::TestSink& incognito_sink = incognito_render_process_host_->sink();

  // Enable the extension. It should not be loaded in the initialized incognito
  // renderer.
  sink.ClearMessages();
  incognito_sink.ClearMessages();
  EXPECT_FALSE(util::IsIncognitoEnabled(extension_->id(), browser_context()));
  EXPECT_FALSE(IsExtensionLoaded(*extension_));
  AddExtensionToRegistry(extension_);
  helper_->OnExtensionLoaded(*extension_);
  EXPECT_EQ(0u, sink.message_count());
  EXPECT_EQ(0u, incognito_sink.message_count());
  EXPECT_TRUE(IsExtensionLoaded(*extension_));
  EXPECT_FALSE(IsExtensionLoadedInProcess(
      *extension_, incognito_render_process_host_.get()));
  EXPECT_FALSE(
      IsExtensionLoadedInProcess(*extension_, render_process_host_.get()));

  // Initialize the normal renderer. The extension should get loaded in it.
  sink.ClearMessages();
  incognito_sink.ClearMessages();
  EXPECT_FALSE(IsProcessInitialized(render_process_host_.get()));
  SimulateRenderProcessCreated(render_process_host_.get());
  EXPECT_TRUE(IsProcessInitialized(render_process_host_.get()));
  EXPECT_TRUE(
      IsExtensionLoadedInProcess(*extension_, render_process_host_.get()));
  EXPECT_FALSE(IsExtensionLoadedInProcess(
      *extension_, incognito_render_process_host_.get()));
  // Multiple initialization messages including the extension load message
  // should be dispatched to the non-incognito renderer.
  EXPECT_LE(1u, sink.message_count());
  EXPECT_EQ(0u, incognito_sink.message_count());

  // Enable the extension in incognito mode. This will reload the extension.
  sink.ClearMessages();
  incognito_sink.ClearMessages();
  ExtensionPrefs::Get(browser_context())
      ->SetIsIncognitoEnabled(extension_->id(), true);
  helper_->OnExtensionUnloaded(*extension_);
  helper_->OnExtensionLoaded(*extension_);
  EXPECT_TRUE(IsExtensionLoadedInProcess(*extension_,
                                         incognito_render_process_host_.get()));
  EXPECT_TRUE(
      IsExtensionLoadedInProcess(*extension_, render_process_host_.get()));
  // The extension would not have been unloaded from the incognito renderer
  // since it wasn't loaded.
  ASSERT_EQ(1u, incognito_sink.message_count());
  EXPECT_EQ(static_cast<uint32_t>(ExtensionMsg_Loaded::ID),
            incognito_sink.GetMessageAt(0)->type());
  // The extension would be first unloaded and then loaded from the normal
  // renderer.
  base::RunLoop().RunUntilIdle();
  ASSERT_EQ(1u, helper_->num_unloaded_extensions());
  EXPECT_EQ(static_cast<uint32_t>(ExtensionMsg_Loaded::ID),
            sink.GetMessageAt(0)->type());
}

// Tests that platform apps are always loaded in an incognito renderer.
TEST_F(RendererStartupHelperTest, PlatformAppInIncognitoRenderer) {
  // Initialize the incognito renderer.
  EXPECT_FALSE(IsProcessInitialized(incognito_render_process_host_.get()));
  SimulateRenderProcessCreated(incognito_render_process_host_.get());
  EXPECT_TRUE(IsProcessInitialized(incognito_render_process_host_.get()));

  IPC::TestSink& incognito_sink = incognito_render_process_host_->sink();

  scoped_refptr<const Extension> platform_app(
      CreatePlatformApp("platform_app"));
  ASSERT_TRUE(platform_app->is_platform_app());
  EXPECT_FALSE(util::IsIncognitoEnabled(platform_app->id(), browser_context()));
  EXPECT_FALSE(util::CanBeIncognitoEnabled(platform_app.get()));

  // Enable the app. It should get loaded in the incognito renderer even though
  // IsIncognitoEnabled returns false for it, since it can't be enabled for
  // incognito.
  incognito_sink.ClearMessages();
  AddExtensionToRegistry(platform_app);
  helper_->OnExtensionLoaded(*platform_app);
  EXPECT_TRUE(IsExtensionLoadedInProcess(*platform_app,
                                         incognito_render_process_host_.get()));
  ASSERT_EQ(1u, incognito_sink.message_count());
  EXPECT_EQ(static_cast<uint32_t>(ExtensionMsg_Loaded::ID),
            incognito_sink.GetMessageAt(0)->type());
}

}  // namespace extensions
