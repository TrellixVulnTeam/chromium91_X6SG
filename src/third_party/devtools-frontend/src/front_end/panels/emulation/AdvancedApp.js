// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import * as Common from '../../core/common/common.js';  // eslint-disable-line no-unused-vars
import * as Host from '../../core/host/host.js';
import * as UI from '../../ui/legacy/legacy.js';

import {DeviceModeWrapper} from './DeviceModeWrapper.js';
import {Events, InspectedPagePlaceholder} from './InspectedPagePlaceholder.js';  // eslint-disable-line no-unused-vars

/** @type {!AdvancedApp} */
let _appInstance;

/**
 * @implements {Common.App.App}
 */
export class AdvancedApp {
  constructor() {
    UI.DockController.DockController.instance().addEventListener(
        UI.DockController.Events.BeforeDockSideChanged, this._openToolboxWindow, this);

    // These attributes are guaranteed to be initialised in presentUI that
    // is invoked immediately after construction.
    /**
     * @type {!UI.SplitWidget.SplitWidget}
     */
    this._rootSplitWidget;

    /**
     * @type {!DeviceModeWrapper}
     */
    this._deviceModeView;

    /**
     * @type {!InspectedPagePlaceholder}
     */
    this._inspectedPagePlaceholder;
  }

  /**
   * Note: it's used by toolbox.ts without real type checks.
   * @return {!AdvancedApp}
   */
  static _instance() {
    if (!_appInstance) {
      _appInstance = new AdvancedApp();
    }
    return _appInstance;
  }

  /**
   * @override
   * @param {!Document} document
   */
  presentUI(document) {
    const rootView = new UI.RootView.RootView();

    this._rootSplitWidget = new UI.SplitWidget.SplitWidget(false, true, 'InspectorView.splitViewState', 555, 300, true);
    this._rootSplitWidget.show(rootView.element);
    this._rootSplitWidget.setSidebarWidget(UI.InspectorView.InspectorView.instance());
    this._rootSplitWidget.setDefaultFocusedChild(UI.InspectorView.InspectorView.instance());
    UI.InspectorView.InspectorView.instance().setOwnerSplit(this._rootSplitWidget);

    this._inspectedPagePlaceholder = InspectedPagePlaceholder.instance();
    this._inspectedPagePlaceholder.addEventListener(Events.Update, this._onSetInspectedPageBounds.bind(this), this);
    this._deviceModeView =
        DeviceModeWrapper.instance({inspectedPagePlaceholder: this._inspectedPagePlaceholder, forceNew: false});

    UI.DockController.DockController.instance().addEventListener(
        UI.DockController.Events.BeforeDockSideChanged, this._onBeforeDockSideChange, this);
    UI.DockController.DockController.instance().addEventListener(
        UI.DockController.Events.DockSideChanged, this._onDockSideChange, this);
    UI.DockController.DockController.instance().addEventListener(
        UI.DockController.Events.AfterDockSideChanged, this._onAfterDockSideChange, this);
    this._onDockSideChange();

    console.timeStamp('AdvancedApp.attachToBody');
    rootView.attachToDocument(document);
    rootView.focus();
    this._inspectedPagePlaceholder.update();
  }

  /**
   * @param {!Common.EventTarget.EventTargetEvent} event
   */
  _openToolboxWindow(event) {
    if (/** @type {string} */ (event.data.to) !== UI.DockController.State.Undocked) {
      return;
    }

    if (this._toolboxWindow) {
      return;
    }

    const url = window.location.href.replace('devtools_app.html', 'toolbox.html');
    this._toolboxWindow = window.open(url, undefined);
  }

  /**
   * @param {!Document} toolboxDocument
   */
  toolboxLoaded(toolboxDocument) {
    UI.UIUtils.initializeUIUtils(
        toolboxDocument, Common.Settings.Settings.instance().createSetting('uiTheme', 'default'));
    UI.UIUtils.installComponentRootStyles(/** @type {!Element} */ (toolboxDocument.body));
    UI.ContextMenu.ContextMenu.installHandler(toolboxDocument);
    UI.Tooltip.Tooltip.installHandler(toolboxDocument);

    this._toolboxRootView = new UI.RootView.RootView();
    this._toolboxRootView.attachToDocument(toolboxDocument);

    this._updateDeviceModeView();
  }

  _updateDeviceModeView() {
    if (this._isDocked()) {
      this._rootSplitWidget.setMainWidget(this._deviceModeView);
    } else if (this._toolboxRootView) {
      this._deviceModeView.show(this._toolboxRootView.element);
    }
  }

  /**
   * @param {!Common.EventTarget.EventTargetEvent} event
   */
  _onBeforeDockSideChange(event) {
    if (/** @type {string} */ (event.data.to) === UI.DockController.State.Undocked && this._toolboxRootView) {
      // Hide inspectorView and force layout to mimic the undocked state.
      this._rootSplitWidget.hideSidebar();
      this._inspectedPagePlaceholder.update();
    }

    this._changingDockSide = true;
  }

  /**
   * @param {!Common.EventTarget.EventTargetEvent=} event
   */
  _onDockSideChange(event) {
    this._updateDeviceModeView();

    const toDockSide =
        event ? /** @type {string} */ (event.data.to) : UI.DockController.DockController.instance().dockSide();
    if (toDockSide === UI.DockController.State.Undocked) {
      this._updateForUndocked();
    } else if (
        this._toolboxRootView && event &&
        /** @type {string} */ (event.data.from) === UI.DockController.State.Undocked) {
      // Don't update yet for smooth transition.
      this._rootSplitWidget.hideSidebar();
    } else {
      this._updateForDocked(toDockSide);
    }
  }

  /**
   * @param {!Common.EventTarget.EventTargetEvent} event
   */
  _onAfterDockSideChange(event) {
    // We may get here on the first dock side change while loading without BeforeDockSideChange.
    if (!this._changingDockSide) {
      return;
    }
    if (/** @type {string} */ (event.data.from) === UI.DockController.State.Undocked) {
      // Restore docked layout in case of smooth transition.
      this._updateForDocked(/** @type {string} */ (event.data.to));
    }
    this._changingDockSide = false;
    this._inspectedPagePlaceholder.update();
  }

  /**
   * @param {string} dockSide
   */
  _updateForDocked(dockSide) {
    const resizerElement = /** @type {!HTMLElement} */ (this._rootSplitWidget.resizerElement());
    resizerElement.style.transform = dockSide === UI.DockController.State.DockedToRight ?
        'translateX(2px)' :
        dockSide === UI.DockController.State.DockedToLeft ? 'translateX(-2px)' : '';
    this._rootSplitWidget.setVertical(
        dockSide === UI.DockController.State.DockedToRight || dockSide === UI.DockController.State.DockedToLeft);
    this._rootSplitWidget.setSecondIsSidebar(
        dockSide === UI.DockController.State.DockedToRight || dockSide === UI.DockController.State.DockedToBottom);
    this._rootSplitWidget.toggleResizer(this._rootSplitWidget.resizerElement(), true);
    this._rootSplitWidget.toggleResizer(
        UI.InspectorView.InspectorView.instance().topResizerElement(),
        dockSide === UI.DockController.State.DockedToBottom);
    this._rootSplitWidget.showBoth();
  }

  _updateForUndocked() {
    this._rootSplitWidget.toggleResizer(this._rootSplitWidget.resizerElement(), false);
    this._rootSplitWidget.toggleResizer(UI.InspectorView.InspectorView.instance().topResizerElement(), false);
    this._rootSplitWidget.hideMain();
  }

  _isDocked() {
    return UI.DockController.DockController.instance().dockSide() !== UI.DockController.State.Undocked;
  }

  /**
   * @param {!Common.EventTarget.EventTargetEvent} event
   */
  _onSetInspectedPageBounds(event) {
    if (this._changingDockSide) {
      return;
    }
    const window = this._inspectedPagePlaceholder.element.window();
    if (!window.innerWidth || !window.innerHeight) {
      return;
    }
    if (!this._inspectedPagePlaceholder.isShowing()) {
      return;
    }
    const bounds = /** @type {{x: number, y: number, width: number, height: number}} */ (event.data);
    console.timeStamp('AdvancedApp.setInspectedPageBounds');
    Host.InspectorFrontendHost.InspectorFrontendHostInstance.setInspectedPageBounds(bounds);
  }
}

// Export required for usage in toolbox.ts
globalThis.Emulation = globalThis.Emulation || {};
globalThis.Emulation.AdvancedApp = AdvancedApp;

/** @type {!AdvancedAppProvider} */
let advancedAppProviderInstance;

/**
 * @implements {Common.AppProvider.AppProvider}
 */
export class AdvancedAppProvider {
  /**
   * @param {{forceNew: ?boolean}} opts
   */
  static instance(opts = {forceNew: null}) {
    const {forceNew} = opts;
    if (!advancedAppProviderInstance || forceNew) {
      advancedAppProviderInstance = new AdvancedAppProvider();
    }

    return advancedAppProviderInstance;
  }

  /**
   * @override
   * @return {!Common.App.App}
   */
  createApp() {
    return AdvancedApp._instance();
  }
}
