// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

/**
 * @fileoverview This is a table column representation
 */

// clang-format off
// #import {NativeEventTarget as EventTarget} from 'chrome://resources/js/cr/event_target.m.js';
// #import {getPropertyDescriptor, dispatchPropertyChange} from 'chrome://resources/js/cr.m.js';
// clang-format on

cr.define('cr.ui.table', function() {
  /**
   * A table column that wraps column ids and settings.
   */
  /* #export */ class TableColumn extends cr.EventTarget {
    /**
     * @param {string} id
     * @param {string} name
     * @param {number} width
     * @param {boolean=} opt_endAlign
     */
    constructor(id, name, width, opt_endAlign) {
      super();

      this.id_ = id;
      this.name_ = name;
      this.width_ = width;
      this.endAlign_ = !!opt_endAlign;
      this.visible_ = true;
      this.defaultOrder_ = 'asc';
    }

    /**
     * Clones column.
     * @return {cr.ui.table.TableColumn} Clone of the given column.
     */
    clone() {
      const tableColumn =
          new TableColumn(this.id_, this.name_, this.width_, this.endAlign_);
      tableColumn.renderFunction = this.renderFunction_;
      tableColumn.headerRenderFunction = this.headerRenderFunction_;
      tableColumn.defaultOrder = this.defaultOrder_;

      tableColumn.visible_ = this.visible_;

      return tableColumn;
    }

    /**
     * Renders table cell. This is the default render function.
     * @param {*} dataItem The data item to be rendered.
     * @param {string} columnId The column id.
     * @param {Element} table The table.
     * @return {HTMLElement} Rendered element.
     */
    renderFunction_(dataItem, columnId, table) {
      const div = /** @type {HTMLElement} */
          (table.ownerDocument.createElement('div'));
      div.textContent = dataItem[columnId];
      div.hidden = !this.visible;
      return div;
    }

    /**
     * Renders table header. This is the default render function.
     * @param {Element} table The table.
     * @return {Text} Rendered text node.
     */
    headerRenderFunction_(table) {
      return table.ownerDocument.createTextNode(this.name);
    }

    /**
     * The width of the column.  Hidden columns have zero width.
     * @type {number}
     */
    get width() {
      return this.visible_ ? this.width_ : 0;
    }

    set width(value) {
      const oldValue = this.width;
      if (value !== oldValue) {
        this.width_ = value;
        cr.dispatchPropertyChange(this, 'width', value, oldValue);
      }
    }

    /**
     * The width of the column, disregarding visibility.  For hidden columns,
     * this would be the width of the column if it were to be made visible.
     * @type {number}
     */
    get absoluteWidth() {
      return this.width_;
    }
  }

  /**
   * The column id.
   * @type {string}
   */
  TableColumn.prototype.id;
  Object.defineProperty(
      TableColumn.prototype, 'id', cr.getPropertyDescriptor('id'));

  /**
   * The column name
   * @type {string}
   */
  TableColumn.prototype.name;
  Object.defineProperty(
      TableColumn.prototype, 'name', cr.getPropertyDescriptor('name'));

  /**
   * The column visibility.
   * @type {boolean}
   */
  TableColumn.prototype.visible;
  Object.defineProperty(
      TableColumn.prototype, 'visible', cr.getPropertyDescriptor('visible'));

  /**
   * True if the column is aligned to end.
   * @type {boolean}
   */
  TableColumn.prototype.endAlign;
  Object.defineProperty(
      TableColumn.prototype, 'endAlign', cr.getPropertyDescriptor('endAlign'));

  /**
   * The column render function.
   * @type {function(*, string, Element): HTMLElement}
   */
  TableColumn.prototype.renderFunction;
  Object.defineProperty(
      TableColumn.prototype, 'renderFunction',
      cr.getPropertyDescriptor('renderFunction'));

  /**
   * The column header render function.
   * @type {function(Element):Node}
   */
  TableColumn.prototype.headerRenderFunction;
  Object.defineProperty(
      TableColumn.prototype, 'headerRenderFunction',
      cr.getPropertyDescriptor('headerRenderFunction'));

  /**
   * Default sorting order for the column ('asc' or 'desc').
   * @type {string}
   */
  TableColumn.prototype.defaultOrder;
  Object.defineProperty(
      TableColumn.prototype, 'defaultOrder',
      cr.getPropertyDescriptor('defaultOrder'));

  // #cr_define_end
  return {TableColumn: TableColumn};
});
