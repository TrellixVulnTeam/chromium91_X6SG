// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/public/common/switches.h"

namespace blink {
namespace switches {

// Allows processing of input before a frame has been committed.
// TODO(schenney): crbug.com/987626. Used by headless. Look for a way not
// involving a command line switch.
const char kAllowPreCommitInput[] = "allow-pre-commit-input";

#if defined(USE_NEVA_APPRUNTIME)
// Allow script to close windows that was opened by another scripts
const char kAllowScriptsToCloseWindows[] = "allow-scripts-to-close-windows";
// Set min and max time of first purging after a renderer is backgrounded.
// Given value will replace default time.
const char kMaxTimeToPurgeAfterBackgroundedInSeconds[] =
    "max-delay-to-purge-after-backgrounded-in-seconds";
const char kMinTimeToPurgeAfterBackgroundedInSeconds[] =
    "min-delay-to-purge-after-backgrounded-in-seconds";
#endif  // defined(USE_NEVA_APPRUNTIME)

// Set blink settings. Format is <name>[=<value],<name>[=<value>],...
// The names are declared in Settings.json5. For boolean type, use "true",
// "false", or omit '=<value>' part to set to true. For enum type, use the int
// value of the enum value. Applied after other command line flags and prefs.
const char kBlinkSettings[] = "blink-settings";

// Sets dark mode settings. Format is [<param>=<value>],[<param>=<value>],...
// The params take either int or float values. If params are not specified,
// the default dark mode settings is used. Valid params are given below.
// "InversionAlgorithm" takes int value of DarkModeInversionAlgorithm enum.
// "ImagePolicy" takes int value of DarkModeImagePolicy enum.
// "IsGrayScale" takes 1 or 0, 1 means grayscale is true, false otherwise.
// "TextBrightnessThreshold" takes 0 to 255 int value.
// "BackgroundBrightnessThreshold" takes 0 to 255 int value.
// "ContrastPercent" takes -1.0 to 1.0 float value. Higher the value, more
// the contrast.
// "ImageGrayScalePercent" takes 0.0 to 1.0 float value. Higher the value,
// image would be more grayish.
const char kDarkModeSettings[] = "dark-mode-settings";

// Sets the tile size used by composited layers.
const char kDefaultTileWidth[] = "default-tile-width";
const char kDefaultTileHeight[] = "default-tile-height";

// Disallow image animations to be reset to the beginning to avoid skipping
// many frames. Only effective if compositor image animations are enabled.
const char kDisableImageAnimationResync[] = "disable-image-animation-resync";

// When using CPU rasterizing disable low resolution tiling. This uses
// less power, particularly during animations, but more white may be seen
// during fast scrolling especially on slower devices.
const char kDisableLowResTiling[] = "disable-low-res-tiling";

// Disable partial raster in the renderer. Disabling this switch also disables
// the use of persistent gpu memory buffers.
const char kDisablePartialRaster[] = "disable-partial-raster";

// Disable the creation of compositing layers when it would prevent LCD text.
const char kDisablePreferCompositingToLCDText[] =
    "disable-prefer-compositing-to-lcd-text";

// Disables RGBA_4444 textures.
const char kDisableRGBA4444Textures[] = "disable-rgba-4444-textures";

// Disable multithreaded, compositor scrolling of web content.
const char kDisableThreadedScrolling[] = "disable-threaded-scrolling";

// Disable rasterizer that writes directly to GPU memory associated with tiles.
const char kDisableZeroCopy[] = "disable-zero-copy";

// Specify that all compositor resources should be backed by GPU memory buffers.
const char kEnableGpuMemoryBufferCompositorResources[] =
    "enable-gpu-memory-buffer-compositor-resources";

// When using CPU rasterizing generate low resolution tiling. Low res
// tiles may be displayed during fast scrolls especially on slower devices.
const char kEnableLowResTiling[] = "enable-low-res-tiling";

// Enable the creation of compositing layers when it would prevent LCD text.
const char kEnablePreferCompositingToLCDText[] =
    "enable-prefer-compositing-to-lcd-text";

// Enables RGBA_4444 textures.
const char kEnableRGBA4444Textures[] = "enable-rgba-4444-textures";

// Enables raster side dark mode for images.
const char kEnableRasterSideDarkModeForImages[] =
    "enable-raster-side-dark-mode-for-images";

// Enable rasterizer that writes directly to GPU memory associated with tiles.
const char kEnableZeroCopy[] = "enable-zero-copy";

// The number of multisample antialiasing samples for GPU rasterization.
// Requires MSAA support on GPU to have an effect. 0 disables MSAA.
const char kGpuRasterizationMSAASampleCount[] =
    "gpu-rasterization-msaa-sample-count";

// Used to communicate managed policy for the IntensiveWakeUpThrottling feature.
// This feature is typically controlled by base::Feature (see
// renderer/platform/scheduler/common/features.*) but requires an enterprise
// policy override. This is implicitly a tri-state, and can be either unset, or
// set to "1" for force enable, or "0" for force disable.
extern const char kIntensiveWakeUpThrottlingPolicy[] =
    "intensive-wake-up-throttling-policy";
extern const char kIntensiveWakeUpThrottlingPolicy_ForceDisable[] = "0";
extern const char kIntensiveWakeUpThrottlingPolicy_ForceEnable[] = "1";

// Sets the width and height above which a composited layer will get tiled.
const char kMaxUntiledLayerHeight[] = "max-untiled-layer-height";
const char kMaxUntiledLayerWidth[] = "max-untiled-layer-width";

// Sets the min tile height for GPU raster.
const char kMinHeightForGpuRasterTile[] = "min-height-for-gpu-raster-tile";

// Sets the timeout seconds of the network-quiet timers in IdlenessDetector.
// Used by embedders who want to change the timeout time in order to run web
// contents on various embedded devices and changeable network bandwidths in
// different regions. For example, it's useful when using FirstMeaningfulPaint
// signal to dismiss a splash screen.
const char kNetworkQuietTimeout[] = "network-quiet-timeout";

// Override the default value for the 'passive' field in javascript
// addEventListener calls. Values are defined as:
//  'documentonlytrue' to set the default be true only for document level nodes.
//  'true' to set the default to be true on all nodes (when not specified).
//  'forcealltrue' to force the value on all nodes.
const char kPassiveListenersDefault[] = "passive-listeners-default";

// Visibly render a border around layout shift rects in the web page to help
// debug and study layout shifts.
const char kShowLayoutShiftRegions[] = "show-layout-shift-regions";

// Visibly render a border around paint rects in the web page to help debug
// and study painting behavior.
const char kShowPaintRects[] = "show-paint-rects";

// Controls how text selection granularity changes when touch text selection
// handles are dragged. Should be "character" or "direction". If not specified,
// the platform default is used.
const char kTouchTextSelectionStrategy[] = "touch-selection-strategy";

// Used to communicate managed policy for the UserAgentClientHint feature.
// This feature is typically controlled by base::Feature (see
// renderer/platform/scheduler/common/features.*) but requires an enterprise
// policy override.

extern const char kUserAgentClientHintDisable[] =
    "user-agent-client-hint-disable";

}  // namespace switches
}  // namespace blink
