/*
 * Copyright (C) 2011 Google Inc.  All rights reserved.
 * Copyright (C) 2006, 2007, 2008 Apple Inc.  All rights reserved.
 * Copyright (C) 2007 Matt Lilek (pewtermoose@gmail.com).
 * Copyright (C) 2009 Joseph Pecoraro
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 *
 * 1.  Redistributions of source code must retain the above copyright
 *     notice, this list of conditions and the following disclaimer.
 * 2.  Redistributions in binary form must reproduce the above copyright
 *     notice, this list of conditions and the following disclaimer in the
 *     documentation and/or other materials provided with the distribution.
 * 3.  Neither the name of Apple Computer, Inc. ("Apple") nor the names of
 *     its contributors may be used to endorse or promote products derived
 *     from this software without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY APPLE AND ITS CONTRIBUTORS "AS IS" AND ANY
 * EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED
 * WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE
 * DISCLAIMED. IN NO EVENT SHALL APPLE OR ITS CONTRIBUTORS BE LIABLE FOR ANY
 * DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES
 * (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES;
 * LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND
 * ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF
 * THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

import * as Common from '../core/common/common.js';
import * as Platform from '../core/platform/platform.js';
import * as Root from '../core/root/root.js';

/**
 * @type {!ThemeSupport}
 */
let themeSupportInstance;

export class ThemeSupport {
  /**
   * @private
   * @param {!Common.Settings.Setting<string>} setting
   */
  constructor(setting) {
    const systemPreferredTheme = window.matchMedia('(prefers-color-scheme: dark)').matches ? 'dark' : 'default';
    this._themeName = setting.get() === 'systemPreferred' ? systemPreferredTheme : setting.get();
    this._themableProperties = new Set([
      'color', 'box-shadow', 'text-shadow', 'outline-color', 'background-image', 'background-color',
      'border-left-color', 'border-right-color', 'border-top-color', 'border-bottom-color', '-webkit-border-image',
      'fill', 'stroke'
    ]);
    /** @type {!Map<string, string>} */
    this._cachedThemePatches = new Map();
    this._setting = setting;
    this._customSheets = new Set();
    this._computedRoot = Common.Lazy.lazy(() => window.getComputedStyle(document.documentElement));
  }

  static hasInstance() {
    return typeof themeSupportInstance !== 'undefined';
  }

  /**
   * @param {{forceNew: ?boolean, setting: ?Common.Settings.Setting<string>}} opts
   */
  static instance(opts = {forceNew: null, setting: null}) {
    const {forceNew, setting} = opts;
    if (!themeSupportInstance || forceNew) {
      if (!setting) {
        throw new Error(`Unable to create theme support: setting must be provided: ${new Error().stack}`);
      }

      themeSupportInstance = new ThemeSupport(setting);
    }

    return themeSupportInstance;
  }

  /**
   * @param {string} variableName
   * @returns {string}
   */
  getComputedValue(variableName) {
    const computedRoot = this._computedRoot();

    if (typeof computedRoot === 'symbol') {
      throw new Error(`Computed value for property (${variableName}) could not be found on :root.`);
    }

    return computedRoot.getPropertyValue(variableName);
  }

  /**
   * @return {boolean}
   */
  hasTheme() {
    return this._themeName !== 'default';
  }

  /**
   * @return {string}
   */
  themeName() {
    return this._themeName;
  }

  /**
   * @param {!Element|!ShadowRoot} element
   */
  injectHighlightStyleSheets(element) {
    this._injectingStyleSheet = true;
    this._appendStyle(element, 'ui/legacy/inspectorSyntaxHighlight.css', {enableLegacyPatching: true});
    if (this._themeName === 'dark') {
      this._appendStyle(element, 'ui/legacy/inspectorSyntaxHighlightDark.css', {enableLegacyPatching: true});
    }
    this._injectingStyleSheet = false;
  }

  /**
   * Note: this is a duplicate of the function in ui/utils. It exists here
   * so there is no circular dependency between ui/utils and theme_support.
   *
   * @param {!Node} node
   * @param {string} cssFile
   * @param {!{enableLegacyPatching:boolean}} options
   */
  _appendStyle(node, cssFile, options = {enableLegacyPatching: false}) {
    const content = Root.Runtime.cachedResources.get(cssFile) || '';
    if (!content) {
      console.error(cssFile + ' not preloaded. Check module.json');
    }
    let styleElement = document.createElement('style');
    styleElement.textContent = content;
    node.appendChild(styleElement);

    /**
   * We are incrementally removing patching support in favour of CSS variables for supporting dark mode.
   * See https://docs.google.com/document/d/1QrSSRsJRzaQBY3zz73ZL84bTcFUV60yMtE5cuu6ED14 for details.
   */
    if (options.enableLegacyPatching) {
      const themeStyleSheet = ThemeSupport.instance().themeStyleSheet(cssFile, content);
      if (themeStyleSheet) {
        styleElement = document.createElement('style');
        styleElement.textContent = themeStyleSheet + '\n' + Root.Runtime.Runtime.resolveSourceURL(cssFile + '.theme');
        node.appendChild(styleElement);
      }
    }
  }

  /**
   * @param {!Element|!ShadowRoot} element
   */
  injectCustomStyleSheets(element) {
    for (const sheet of this._customSheets) {
      const styleElement = document.createElement('style');
      styleElement.textContent = sheet;
      element.appendChild(styleElement);
    }
  }

  /**
   * @return {boolean}
   */
  isForcedColorsMode() {
    return window.matchMedia('(forced-colors: active)').matches;
  }

  /**
   * @param {string} sheetText
   */
  addCustomStylesheet(sheetText) {
    this._customSheets.add(sheetText);
  }

  /**
   * @param {!Document} document
   */
  applyTheme(document) {
    if (!this.hasTheme() || this.isForcedColorsMode()) {
      return;
    }

    if (this._themeName === 'dark') {
      document.documentElement.classList.add('-theme-with-dark-background');
    }

    const styleSheets = document.styleSheets;
    const result = [];
    for (let i = 0; i < styleSheets.length; ++i) {
      const href = styleSheets[i].href;
      if (!href) {
        continue;
      }
      result.push(this._patchForTheme(href, /** @type {!CSSStyleSheet} */ (styleSheets[i])));
    }
    result.push('/*# sourceURL=inspector.css.theme */');

    const styleElement = document.createElement('style');
    styleElement.textContent = result.join('\n');
    document.head.appendChild(styleElement);
  }

  /**
   * @param {string} id
   * @param {string} text
   * @return {string}
   */
  themeStyleSheet(id, text) {
    if (!this.hasTheme() || this._injectingStyleSheet || this.isForcedColorsMode()) {
      return '';
    }

    let patch = this._cachedThemePatches.get(id);
    if (!patch) {
      const styleElement = document.createElement('style');
      styleElement.textContent = text;
      document.body.appendChild(styleElement);

      const {sheet} = styleElement;
      if (!sheet) {
        throw new Error('No sheet in stylesheet object');
      }
      patch = this._patchForTheme(id, sheet);
      document.body.removeChild(styleElement);
    }
    return patch;
  }

  /**
   * @param {string} id
   * @param {!CSSStyleSheet} styleSheet
   * @return {string}
   */
  _patchForTheme(id, styleSheet) {
    const cached = this._cachedThemePatches.get(id);
    if (cached) {
      return cached;
    }

    try {
      const rules = styleSheet.cssRules;
      const result = [];
      for (let j = 0; j < rules.length; ++j) {
        const rule = rules[j];
        if (rule instanceof CSSImportRule) {
          result.push(this._patchForTheme(rule.styleSheet.href || '', rule.styleSheet));
          continue;
        }

        if (!(rule instanceof CSSStyleRule)) {
          continue;
        }

        /** @type {!Array<string>} */
        const output = [];
        const style = rule.style;
        const selectorText = rule.selectorText;
        for (let i = 0; style && i < style.length; ++i) {
          this._patchProperty(selectorText, style, style[i], output);
        }
        if (output.length) {
          result.push(rule.selectorText + '{' + output.join('') + '}');
        }
      }

      const fullText = result.join('\n');
      this._cachedThemePatches.set(id, fullText);
      return fullText;
    } catch (e) {
      this._setting.set('default');
      return '';
    }
  }

  /**
   * @param {string} selectorText
   * @param {!CSSStyleDeclaration} style
   * @param {string} name
   * @param {!Array<string>} output
   *
   * Theming API is primarily targeted at making dark theme look good.
   * - If rule has ".-theme-preserve" in selector, it won't be affected.
   * - One can create specializations for dark themes via body.-theme-with-dark-background selector in host context.
   */
  _patchProperty(selectorText, style, name, output) {
    if (!this._themableProperties.has(name)) {
      return;
    }

    const value = style.getPropertyValue(name);
    if (!value || value === 'none' || value === 'inherit' || value === 'initial' || value === 'transparent') {
      return;
    }
    if (name === 'background-image' && value.indexOf('gradient') === -1) {
      return;
    }

    if (selectorText.indexOf('-theme-') !== -1) {
      return;
    }

    let colorUsage = ThemeSupport.ColorUsage.Unknown;
    if (name.indexOf('background') === 0 || name.indexOf('border') === 0) {
      colorUsage |= ThemeSupport.ColorUsage.Background;
    }
    if (name.indexOf('background') === -1) {
      colorUsage |= ThemeSupport.ColorUsage.Foreground;
    }

    output.push(name);
    output.push(':');
    if (/^var\(.*\)$/.test(value)) {
      // Don't translate CSS variables.
      output.push(value);
    } else {
      const items = value.replace(Common.Color.Regex, '\0$1\0').split('\0');
      for (const item of items) {
        output.push(this.patchColorText(item, /** @type {!ThemeSupport.ColorUsage} */ (colorUsage)));
      }
    }
    if (style.getPropertyPriority(name)) {
      output.push(' !important');
    }
    output.push(';');
  }

  /**
   * @param {string} text
   * @param {!ThemeSupport.ColorUsage} colorUsage
   * @return {string}
   */
  patchColorText(text, colorUsage) {
    const color = Common.Color.Color.parse(text);
    if (!color) {
      return text;
    }
    const outColor = this.patchColor(color, colorUsage);
    let outText = outColor.asString(null);
    if (!outText) {
      outText = outColor.asString(outColor.hasAlpha() ? Common.Color.Format.RGBA : Common.Color.Format.RGB);
    }
    return outText || text;
  }

  /**
   * @param {!Common.Color.Color} color
   * @param {!ThemeSupport.ColorUsage} colorUsage
   * @return {!Common.Color.Color}
   */
  patchColor(color, colorUsage) {
    const hsla = color.hsla();
    this._patchHSLA(hsla, colorUsage);

    /** @type {!Array<number>} */
    const rgba = [];
    Common.Color.Color.hsl2rgb(hsla, rgba);
    return new Common.Color.Color(rgba, color.format());
  }

  /**
   * @param {!Array<number>} hsla
   * @param {!ThemeSupport.ColorUsage} colorUsage
   */
  _patchHSLA(hsla, colorUsage) {
    const hue = hsla[0];
    const sat = hsla[1];
    let lit = hsla[2];
    const alpha = hsla[3];

    switch (this._themeName) {
      case 'dark': {
        const minCap = colorUsage & ThemeSupport.ColorUsage.Background ? 0.14 : 0;
        const maxCap = colorUsage & ThemeSupport.ColorUsage.Foreground ? 0.9 : 1;
        lit = 1 - lit;
        if (lit < minCap * 2) {
          lit = minCap + lit / 2;
        } else if (lit > 2 * maxCap - 1) {
          lit = maxCap - 1 / 2 + lit / 2;
        }
        break;
      }
    }
    hsla[0] = Platform.NumberUtilities.clamp(hue, 0, 1);
    hsla[1] = Platform.NumberUtilities.clamp(sat, 0, 1);
    hsla[2] = Platform.NumberUtilities.clamp(lit, 0, 1);
    hsla[3] = Platform.NumberUtilities.clamp(alpha, 0, 1);
  }
}

/**
 * @enum {number}
 */
ThemeSupport.ColorUsage = {
  Unknown: 0,
  Foreground: 1,
  Background: 2,
};
