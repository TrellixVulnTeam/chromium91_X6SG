/*
 * Copyright (C) 2012 Google Inc. All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions are
 * met:
 *
 *     * Redistributions of source code must retain the above copyright
 * notice, this list of conditions and the following disclaimer.
 *     * Redistributions in binary form must reproduce the above
 * copyright notice, this list of conditions and the following disclaimer
 * in the documentation and/or other materials provided with the
 * distribution.
 *     * Neither the name of Google Inc. nor the names of its
 * contributors may be used to endorse or promote products derived from
 * this software without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS
 * "AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT
 * LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR
 * A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT
 * OWNER OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL,
 * SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT
 * LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
 * DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
 * THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
 * OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

import * as Common from '../../core/common/common.js';  // eslint-disable-line no-unused-vars
import {createShadowRootWithCoreStyles} from './utils/create-shadow-root-with-core-styles.js';

/**
 * @implements {Common.Progress.Progress}
 */
export class ProgressIndicator {
  constructor() {
    this.element = document.createElement('div');
    this.element.classList.add('progress-indicator');
    this._shadowRoot = createShadowRootWithCoreStyles(
        this.element,
        {cssFile: 'ui/legacy/progressIndicator.css', enableLegacyPatching: false, delegatesFocus: undefined});
    this._contentElement = this._shadowRoot.createChild('div', 'progress-indicator-shadow-container');

    this._labelElement = this._contentElement.createChild('div', 'title');
    /** @type {!HTMLProgressElement} */
    this._progressElement = /** @type {!HTMLProgressElement} */ (this._contentElement.createChild('progress'));
    this._progressElement.value = 0;
    this._stopButton = this._contentElement.createChild('button', 'progress-indicator-shadow-stop-button');
    this._stopButton.addEventListener('click', this.cancel.bind(this));

    this._isCanceled = false;
    this._worked = 0;
  }

  /**
   * @param {!Element} parent
   */
  show(parent) {
    parent.appendChild(this.element);
  }

  /**
   * @override
   */
  done() {
    if (this._isDone) {
      return;
    }
    this._isDone = true;
    this.element.remove();
  }

  cancel() {
    this._isCanceled = true;
  }

  /**
   * @override
   * @return {boolean}
   */
  isCanceled() {
    return this._isCanceled;
  }

  /**
   * @override
   * @param {string} title
   */
  setTitle(title) {
    this._labelElement.textContent = title;
  }

  /**
   * @override
   * @param {number} totalWork
   */
  setTotalWork(totalWork) {
    this._progressElement.max = totalWork;
  }

  /**
   * @override
   * @param {number} worked
   * @param {string=} title
   */
  setWorked(worked, title) {
    this._worked = worked;
    this._progressElement.value = worked;
    if (title) {
      this.setTitle(title);
    }
  }

  /**
   * @override
   * @param {number=} worked
   */
  worked(worked) {
    this.setWorked(this._worked + (worked || 1));
  }
}
