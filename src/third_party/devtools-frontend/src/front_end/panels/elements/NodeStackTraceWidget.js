// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import * as Components from '../../components/components.js';
import * as i18n from '../../core/i18n/i18n.js';
import * as SDK from '../../core/sdk/sdk.js';
import * as UI from '../../ui/legacy/legacy.js';

const UIStrings = {
  /**
  *@description Message displayed when no JavaScript stack trace is available for the DOM node in the Stack Trace widget of the Elements panel
  */
  noStackTraceAvailable: 'No stack trace available',
};
const str_ = i18n.i18n.registerUIStrings('panels/elements/NodeStackTraceWidget.js', UIStrings);
const i18nString = i18n.i18n.getLocalizedString.bind(undefined, str_);
/** @type {!NodeStackTraceWidget} */
let nodeStackTraceWidgetInstance;

export class NodeStackTraceWidget extends UI.ThrottledWidget.ThrottledWidget {
  constructor() {
    super(true /* isWebComponent */);
    this.registerRequiredCSS('panels/elements/nodeStackTraceWidget.css', {enableLegacyPatching: false});

    this._noStackTraceElement = this.contentElement.createChild('div', 'gray-info-message');
    this._noStackTraceElement.textContent = i18nString(UIStrings.noStackTraceAvailable);
    this._creationStackTraceElement = this.contentElement.createChild('div', 'stack-trace');

    this._linkifier = new Components.Linkifier.Linkifier(MaxLengthForLinks);
  }

  /**
   * @param {{forceNew: ?boolean}=} opts
   * @return {!NodeStackTraceWidget}
   */
  static instance(opts = {forceNew: null}) {
    const {forceNew} = opts;
    if (!nodeStackTraceWidgetInstance || forceNew) {
      nodeStackTraceWidgetInstance = new NodeStackTraceWidget();
    }

    return nodeStackTraceWidgetInstance;
  }

  /**
   * @override
   */
  wasShown() {
    UI.Context.Context.instance().addFlavorChangeListener(SDK.DOMModel.DOMNode, this.update, this);
    this.update();
  }

  /**
   * @override
   */
  willHide() {
    UI.Context.Context.instance().removeFlavorChangeListener(SDK.DOMModel.DOMNode, this.update, this);
  }

  /**
   * @override
   * @protected
   * @return {!Promise<void>}
   */
  async doUpdate() {
    const node = UI.Context.Context.instance().flavor(SDK.DOMModel.DOMNode);

    if (!node) {
      this._noStackTraceElement.classList.remove('hidden');
      this._creationStackTraceElement.classList.add('hidden');
      return;
    }

    const creationStackTrace = await node.creationStackTrace();
    if (creationStackTrace) {
      this._noStackTraceElement.classList.add('hidden');
      this._creationStackTraceElement.classList.remove('hidden');

      const stackTracePreview = Components.JSPresentationUtils.buildStackTracePreviewContents(
          node.domModel().target(), this._linkifier, {stackTrace: creationStackTrace, tabStops: undefined});
      this._creationStackTraceElement.removeChildren();
      this._creationStackTraceElement.appendChild(stackTracePreview.element);
    } else {
      this._noStackTraceElement.classList.remove('hidden');
      this._creationStackTraceElement.classList.add('hidden');
    }
  }
}

/**
 * @const
 * @type {number}
 */
export const MaxLengthForLinks = 40;
