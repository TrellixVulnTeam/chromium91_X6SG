// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import './core/dom_extension/dom_extension.js';
import './Images/Images.js';

import * as Platform from './core/platform/platform.js';  // eslint-disable-line rulesdir/es_modules_import

function toolboxLoaded(): void {
  if (!window.opener) {
    return;
  }
  const app = window.opener.Emulation.AdvancedApp._instance();
  app.toolboxLoaded(document);
}

Platform.runOnWindowLoad(toolboxLoaded);
