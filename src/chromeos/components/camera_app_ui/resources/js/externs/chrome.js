// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

/**
 * It is available since we enabled and filled data in it while creating
 * content::WebUIDataSource for CCA.
 * @typedef {{
 *   getString: function(string): string,
 *   getStringF: function(string, ...(string|number)): string,
 * }}
 */
window.loadTimeData;

/**
 * It is available since we enabled IdleDetection in origin trials map of CCA.
 * @typedef {{
 *   addEventListener: function(string, function()),
 *   screenState: string,
 *   start: function(): !Promise<void>,
 * }}
 */
window.IdleDetector;

/**
 * @typedef {{
 *   getDirectory: function(): !Promise<!FileSystemDirectoryHandle>,
 * }}
 */
navigator.storage;
