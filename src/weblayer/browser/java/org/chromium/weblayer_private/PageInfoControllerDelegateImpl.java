// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.weblayer_private;

import android.content.Context;
import android.content.Intent;
import android.graphics.Bitmap;
import android.graphics.drawable.BitmapDrawable;
import android.graphics.drawable.Drawable;
import android.webkit.ValueCallback;

import androidx.annotation.NonNull;
import androidx.annotation.Nullable;
import androidx.fragment.app.FragmentManager;

import org.chromium.base.Callback;
import org.chromium.base.StrictModeContext;
import org.chromium.components.browser_ui.site_settings.ContentSettingsResources;
import org.chromium.components.browser_ui.site_settings.SiteSettingsCategory;
import org.chromium.components.browser_ui.site_settings.SiteSettingsDelegate;
import org.chromium.components.content_settings.ContentSettingsType;
import org.chromium.components.content_settings.CookieControlsBridge;
import org.chromium.components.content_settings.CookieControlsObserver;
import org.chromium.components.embedder_support.browser_context.BrowserContextHandle;
import org.chromium.components.embedder_support.util.UrlConstants;
import org.chromium.components.page_info.PageInfoControllerDelegate;
import org.chromium.components.page_info.PageInfoMainController;
import org.chromium.components.page_info.PageInfoRowView;
import org.chromium.components.page_info.PageInfoSubpageController;
import org.chromium.content_public.browser.WebContents;
import org.chromium.ui.modaldialog.ModalDialogManager;
import org.chromium.url.GURL;
import org.chromium.weblayer_private.interfaces.ObjectWrapper;
import org.chromium.weblayer_private.interfaces.SettingsIntentHelper;
import org.chromium.weblayer_private.settings.WebLayerSiteSettingsDelegate;

/**
 * WebLayer's customization of PageInfoControllerDelegate.
 */
public class PageInfoControllerDelegateImpl extends PageInfoControllerDelegate {
    private final Context mContext;
    private final WebContents mWebContents;
    private final BrowserImpl mBrowser;
    private final ProfileImpl mProfile;

    static PageInfoControllerDelegateImpl create(WebContents webContents) {
        TabImpl tab = TabImpl.fromWebContents(webContents);
        assert tab != null;
        return new PageInfoControllerDelegateImpl(webContents, tab.getBrowser());
    }

    private PageInfoControllerDelegateImpl(WebContents webContents, BrowserImpl browser) {
        super(new AutocompleteSchemeClassifierImpl(),
                /** vrHandler= */ null,
                /** isSiteSettingsAvailable= */
                isHttpOrHttps(webContents.getVisibleUrl()),
                /** cookieControlsShown= */
                CookieControlsBridge.isCookieControlsEnabled(browser.getProfile()));
        mContext = browser.getContext();
        mWebContents = webContents;
        mBrowser = browser;
        mProfile = browser.getProfile();
    }

    /**
     * {@inheritDoc}
     */
    @Override
    public ModalDialogManager getModalDialogManager() {
        return mBrowser.getWindowAndroid().getModalDialogManager();
    }

    /**
     * {@inheritDoc}
     */
    @Override
    public void showSiteSettings(String url) {
        Intent intent = SettingsIntentHelper.createIntentForSiteSettingsSingleWebsite(
                mContext, mProfile.getName(), mProfile.isIncognito(), url);

        launchIntent(intent);
    }

    @Override
    public void showCookieSettings() {
        String category = SiteSettingsCategory.preferenceKey(SiteSettingsCategory.Type.COOKIES);
        String title = mContext.getResources().getString(
                ContentSettingsResources.getTitle(ContentSettingsType.COOKIES));
        Intent intent = SettingsIntentHelper.createIntentForSiteSettingsSingleCategory(
                mContext, mProfile.getName(), mProfile.isIncognito(), category, title);
        launchIntent(intent);
    }

    private void launchIntent(Intent intent) {
        // Disabling StrictMode to avoid violations (https://crbug.com/819410).
        try (StrictModeContext ignored = StrictModeContext.allowDiskReads()) {
            mContext.startActivity(intent);
        }
    }

    /**
     * {@inheritDoc}
     */
    @Override
    @NonNull
    public CookieControlsBridge createCookieControlsBridge(CookieControlsObserver observer) {
        return new CookieControlsBridge(observer, mWebContents, null);
    }

    /**
     * {@inheritDoc}
     */
    @Override
    @Nullable
    public PageInfoSubpageController createHistoryController(
            PageInfoMainController mainController, PageInfoRowView rowView, String url) {
        return null;
    }

    /**
     * {@inheritDoc}
     */
    @Override
    @NonNull
    public BrowserContextHandle getBrowserContext() {
        return mProfile;
    }

    /**
     * {@inheritDoc}
     */
    @Override
    @NonNull
    public SiteSettingsDelegate getSiteSettingsDelegate() {
        return new WebLayerSiteSettingsDelegate(getBrowserContext());
    }

    @Override
    public void getFavicon(String url, Callback<Drawable> callback) {
        mProfile.getCachedFaviconForPageUri(
                url, ObjectWrapper.wrap((ValueCallback<Bitmap>) (bitmap) -> {
                    if (bitmap != null) {
                        callback.onResult(new BitmapDrawable(mContext.getResources(), bitmap));
                    } else {
                        callback.onResult(null);
                    }
                }));
    }

    /**
     * {@inheritDoc}
     */
    @Override
    public boolean isAccessibilityEnabled() {
        return WebLayerAccessibilityUtil.get().isAccessibilityEnabled();
    }

    private static boolean isHttpOrHttps(GURL url) {
        String scheme = url.getScheme();
        return UrlConstants.HTTP_SCHEME.equals(scheme) || UrlConstants.HTTPS_SCHEME.equals(scheme);
    }

    /**
     * {@inheritDoc}
     */
    @Override
    public FragmentManager getFragmentManager() {
        return mBrowser.getFragmentManager();
    }
}
