// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// #import {importer} from './importer_common.m.js';
// #import {TestCallRecorder} from './unittest_util.m.js';

// Namespace
/* #export */ const importerTest = {};

/**
 * Sets up a logger for use in unit tests.  The test logger doesn't attempt to
 * access chrome's sync file system.  Call this during setUp.
 * @return {!importerTest.TestLogger}
 * @suppress{accessControls} For testing.
 */
importerTest.setupTestLogger = () => {
  const logger = new importerTest.TestLogger();
  importer.logger_ = logger;
  return logger;
};

/**
 * A {@code importer.Logger} for testing.  Just sends output to the console.
 *
 * @implements {importer.Logger}
 * @final
 */
importerTest.TestLogger = class {
  constructor() {
    /** @public {!TestCallRecorder} */
    this.errorRecorder = new TestCallRecorder();

    /** @type {boolean} Should we print stuff to console */
    this.quiet_ = false;
  }

  /**
   * Causes logger to stop printing anything to console.
   * This can be useful when errors are expected in tests.
   */
  quiet() {
    this.quiet_ = true;
  }

  /** @override */
  info(content) {
    if (!this.quiet_) {
      console.log(content);
    }
  }

  /** @override */
  error(content) {
    this.errorRecorder.callback(content);
    if (!this.quiet_) {
      console.error(content);
      console.error(new Error('Error stack').stack);
    }
  }

  /** @override */
  catcher(context) {
    return error => {
      this.error(
          'Caught promise error. Context: ' + context +
          ' Error: ' + error.message);
      if (!this.quiet_) {
        console.error(error.stack);
      }
    };
  }
};
