// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/accessibility/browser_accessibility_state_impl.h"

#import <Cocoa/Cocoa.h>

#include "base/metrics/histogram_macros.h"
#include "content/browser/web_contents/web_contents_impl.h"
#include "content/public/browser/browser_thread.h"
#include "ui/gfx/animation/animation.h"

@interface NSWorkspace (Partials)

@property(readonly) BOOL accessibilityDisplayShouldDifferentiateWithoutColor;
@property(readonly) BOOL accessibilityDisplayShouldIncreaseContrast;
@property(readonly) BOOL accessibilityDisplayShouldReduceTransparency;

@end

// Only available since 10.12.
@interface NSWorkspace (AvailableSinceSierra)
@property(readonly) BOOL accessibilityDisplayShouldReduceMotion;
@end

namespace content {

namespace {
void SetupAccessibilityDisplayOptionsNotifier() {
  // We need to call into gfx::Animation and WebContentsImpl on the UI thread,
  // so ensure that we setup the notification on the correct thread.
  DCHECK_CURRENTLY_ON(BrowserThread::UI);

  // Listen to accessibility display options changing, so that we can update
  // the renderer for the prefers reduced motion settings.
  //
  // BrowserAccessibilityStateImpl is a deliberately leaked singleton, so we
  // don't need to record the notification token for later cleanup.
  [[[NSWorkspace sharedWorkspace] notificationCenter]
      addObserverForName:
          NSWorkspaceAccessibilityDisplayOptionsDidChangeNotification
                  object:nil
                   queue:nil
              usingBlock:^(NSNotification* notification) {
                gfx::Animation::UpdatePrefersReducedMotion();
                for (WebContentsImpl* wc :
                     WebContentsImpl::GetAllWebContents()) {
                  wc->OnWebPreferencesChanged();
                }
              }];
}
}  // namespace

class BrowserAccessibilityStateImplMac : public BrowserAccessibilityStateImpl {
 public:
  BrowserAccessibilityStateImplMac() = default;
  ~BrowserAccessibilityStateImplMac() override {}

 protected:
  void InitBackgroundTasks() override;
  void UpdateHistogramsOnUIThread() override;
  void UpdateHistogramsOnOtherThread() override;
  void UpdateUniqueUserHistograms() override;
};

void BrowserAccessibilityStateImplMac::InitBackgroundTasks() {
  BrowserAccessibilityStateImpl::InitBackgroundTasks();

  GetUIThreadTaskRunner({})->PostTask(
      FROM_HERE, base::BindOnce(&SetupAccessibilityDisplayOptionsNotifier));
}

void BrowserAccessibilityStateImplMac::UpdateHistogramsOnUIThread() {
  BrowserAccessibilityStateImpl::UpdateHistogramsOnUIThread();

  NSWorkspace* workspace = [NSWorkspace sharedWorkspace];

  SEL sel = @selector(accessibilityDisplayShouldReduceTransparency);
  if ([workspace respondsToSelector:sel]) {
    UMA_HISTOGRAM_BOOLEAN(
        "Accessibility.Mac.ReduceTransparency",
        workspace.accessibilityDisplayShouldReduceTransparency);
  }

  sel = @selector(accessibilityDisplayShouldReduceMotion);
  if ([workspace respondsToSelector:sel]) {
    UMA_HISTOGRAM_BOOLEAN("Accessibility.Mac.ReduceMotion",
                          workspace.accessibilityDisplayShouldReduceMotion);
  }
}

void BrowserAccessibilityStateImplMac::UpdateHistogramsOnOtherThread() {
  BrowserAccessibilityStateImpl::UpdateHistogramsOnOtherThread();

  // Screen reader metric.
  ui::AXMode mode =
      BrowserAccessibilityStateImpl::GetInstance()->GetAccessibilityMode();
  UMA_HISTOGRAM_BOOLEAN("Accessibility.Mac.ScreenReader",
                        mode.has_mode(ui::AXMode::kScreenReader));
}

void BrowserAccessibilityStateImplMac::UpdateUniqueUserHistograms() {
  BrowserAccessibilityStateImpl::UpdateUniqueUserHistograms();

  ui::AXMode mode = GetAccessibilityMode();
  UMA_HISTOGRAM_BOOLEAN("Accessibility.Mac.ScreenReader.EveryReport",
                        mode.has_mode(ui::AXMode::kScreenReader));
}

//
// BrowserAccessibilityStateImpl::GetInstance implementation that constructs
// this class instead of the base class.
//

// static
BrowserAccessibilityStateImpl* BrowserAccessibilityStateImpl::GetInstance() {
  static base::NoDestructor<BrowserAccessibilityStateImplMac> instance;
  return &*instance;
}

}  // namespace content
