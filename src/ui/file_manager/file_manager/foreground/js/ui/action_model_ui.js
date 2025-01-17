// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// #import {ListContainer} from './list_container.m.js';
// #import {FilesAlertDialog} from './files_alert_dialog.m.js';

/** @interface */
/* #export */ class ActionModelUI {
  constructor() {
    /** @type {!FilesAlertDialog} */
    this.alertDialog;

    /** @type {!ListContainer} */
    this.listContainer;
  }
}
