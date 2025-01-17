// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

/**
 * @fileoverview
 * @suppress {externsValidation} this file is used as externs and also
 * as JS module, Closure fails to compile as JS module.
 */

// #import {DriveSyncHandler} from './drive_sync_handler.m.js';
// #import {ProgressCenter} from './progress_center.m.js';
// #import {FileOperationManager} from './file_operation_manager.m.js';
// #import {mediaImportInterfaces} from './media_import_handler.m.js';
// #import {mediaScannerInterfaces} from './media_scanner.m.js';
// #import {importerHistoryInterfaces} from './import_history.m.js';
// #import {Crostini} from './crostini.m.js';
// #import {BackgroundBase} from './background_base.m.js';

/**
 * @interface
 */
/* #export */ class FileBrowserBackgroundFull extends BackgroundBase {
  constructor() {
    super();

    /**
     * @type {!DriveSyncHandler}
     */
    this.driveSyncHandler;

    /**
     * @type {!ProgressCenter}
     */
    this.progressCenter;

    /**
     * String assets.
     * @type {Object<string>}
     */
    this.stringData;

    /**
     * @type {FileOperationManager}
     */
    this.fileOperationManager;

    /**
     * @type {!mediaImportInterfaces.ImportRunner}
     */
    this.mediaImportHandler;

    /**
     * @type {!mediaScannerInterfaces.MediaScanner}
     */
    this.mediaScanner;

    /**
     * @type {!importerHistoryInterfaces.HistoryLoader}
     */
    this.historyLoader;

    /**
     * @type {!Crostini}
     */
    this.crostini;
  }

  /**
   * Register callback to be invoked after initialization of the background
   * page. If the initialization is already done, the callback is invoked
   * immediately.
   *
   * @param {function()} callback
   */
  ready(callback) {}

  /**
   * Forces File Operation Util to return error for automated tests.
   * @param {boolean} enable
   */
  forceFileOperationErrorForTest(enable) {}

  /**
   * Registers a dialog (file picker or save as) in the background page.
   * Dialogs are opened by the browser directly and should register themselves
   * in the background page.
   * @param {!Window} window
   */
  registerDialog(window) {}

  /**
   * Launches a new File Manager window.
   *
   * @param {Object=} opt_appState App state.
   * @return {!Promise<chrome.app.window.AppWindow|string>} Resolved with the
   *     App ID.
   */
  async launchFileManager(opt_appState) {}
}
