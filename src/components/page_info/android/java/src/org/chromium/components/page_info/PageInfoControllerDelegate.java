// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.components.page_info;

import android.content.Intent;
import android.graphics.drawable.Drawable;

import androidx.annotation.IntDef;
import androidx.annotation.NonNull;
import androidx.annotation.Nullable;
import androidx.fragment.app.FragmentManager;

import org.chromium.base.Callback;
import org.chromium.base.Consumer;
import org.chromium.components.browser_ui.site_settings.SiteSettingsDelegate;
import org.chromium.components.content_settings.CookieControlsBridge;
import org.chromium.components.content_settings.CookieControlsObserver;
import org.chromium.components.embedder_support.browser_context.BrowserContextHandle;
import org.chromium.components.omnibox.AutocompleteSchemeClassifier;
import org.chromium.components.page_info.PageInfoView.PageInfoViewParams;
import org.chromium.ui.modaldialog.ModalDialogManager;
import org.chromium.url.GURL;

import java.lang.annotation.Retention;
import java.lang.annotation.RetentionPolicy;

/**
 *  Provides embedder-level information to PageInfoController.
 */
public abstract class PageInfoControllerDelegate {
    @IntDef({OfflinePageState.NOT_OFFLINE_PAGE, OfflinePageState.TRUSTED_OFFLINE_PAGE,
            OfflinePageState.UNTRUSTED_OFFLINE_PAGE})
    @Retention(RetentionPolicy.SOURCE)
    public @interface OfflinePageState {
        int NOT_OFFLINE_PAGE = 1;
        int TRUSTED_OFFLINE_PAGE = 2;
        int UNTRUSTED_OFFLINE_PAGE = 3;
    }

    private final AutocompleteSchemeClassifier mAutocompleteSchemeClassifier;
    private final VrHandler mVrHandler;
    private final boolean mIsSiteSettingsAvailable;
    private final boolean mCookieControlsShown;
    protected @OfflinePageState int mOfflinePageState;
    protected boolean mIsHttpsImageCompressionApplied;
    protected String mOfflinePageUrl;

    public PageInfoControllerDelegate(AutocompleteSchemeClassifier autocompleteSchemeClassifier,
            VrHandler vrHandler, boolean isSiteSettingsAvailable, boolean cookieControlsShown) {
        mAutocompleteSchemeClassifier = autocompleteSchemeClassifier;
        mVrHandler = vrHandler;
        mIsSiteSettingsAvailable = isSiteSettingsAvailable;
        mCookieControlsShown = cookieControlsShown;
        mIsHttpsImageCompressionApplied = false;

        // These sometimes get overwritten by derived classes.
        mOfflinePageState = OfflinePageState.NOT_OFFLINE_PAGE;
        mOfflinePageUrl = null;
    }
    /**
     * Creates an AutoCompleteClassifier.
     */
    public AutocompleteSchemeClassifier createAutocompleteSchemeClassifier() {
        return mAutocompleteSchemeClassifier;
    }

    /**
     * Whether cookie controls should be shown in Page Info UI.
     */
    public boolean cookieControlsShown() {
        return mCookieControlsShown;
    }

    /**
     * Return the ModalDialogManager to be used.
     */
    public abstract ModalDialogManager getModalDialogManager();

    /**
     * Returns whether or not an instant app is available for |url|.
     */
    public boolean isInstantAppAvailable(String url) {
        return false;
    }

    /**
     * Returns whether LiteMode https image compression was applied on this page
     */
    public boolean isHttpsImageCompressionApplied() {
        return mIsHttpsImageCompressionApplied;
    }

    /**
     * Gets the instant app intent for the given URL if one exists.
     */
    public Intent getInstantAppIntentForUrl(String url) {
        return null;
    }

    /**
     * Returns a VrHandler for Page Info UI.
     */
    public VrHandler getVrHandler() {
        return mVrHandler;
    }

    /**
     * Gets the Url of the offline page being shown if any. Returns null otherwise.
     */
    @Nullable
    public String getOfflinePageUrl() {
        return mOfflinePageUrl;
    }

    /**
     * Whether the page being shown is an offline page.
     */
    public boolean isShowingOfflinePage() {
        return mOfflinePageState != OfflinePageState.NOT_OFFLINE_PAGE;
    }

    /**
     * Whether the page being shown is a paint preview.
     */
    public boolean isShowingPaintPreviewPage() {
        return false;
    }

    /**
     * Initialize viewParams with Offline Page UI info, if any.
     * @param viewParams The PageInfoViewParams to set state on.
     * @param runAfterDismiss Used to set "open Online" button callback for offline page.
     */
    public void initOfflinePageUiParams(
            PageInfoViewParams viewParams, Consumer<Runnable> runAfterDismiss) {
        viewParams.openOnlineButtonShown = false;
    }

    /**
     * Return the connection message shown for an offline page, if appropriate.
     * Returns null if there's no offline page.
     */
    @Nullable
    public String getOfflinePageConnectionMessage() {
        return null;
    }

    /**
     * Return the connection message shown for a paint preview page, if appropriate.
     * Returns null if there's no paint preview page.
     */
    @Nullable
    public String getPaintPreviewPageConnectionMessage() {
        return null;
    }

    /**
     * Returns whether or not the performance badge should be shown for |url|.
     */
    public boolean shouldShowPerformanceBadge(GURL url) {
        return false;
    }

    /**
     * Whether Site settings are available.
     */
    public boolean isSiteSettingsAvailable() {
        return mIsSiteSettingsAvailable;
    }

    /**
     * Show site settings for the URL passed in.
     * @param url The URL to show site settings for.
     */
    public abstract void showSiteSettings(String url);

    /**
     * Show cookie settings.
     */
    public abstract void showCookieSettings();

    /**
     * Creates Cookie Controls Bridge.
     * @param observer The CookieControlsObserver to create the bridge with.
     * @return the object that facilitates interfacing with native code.
     */
    @NonNull
    public abstract CookieControlsBridge createCookieControlsBridge(
            CookieControlsObserver observer);

    /**
     * Creates controller for history features.
     * @return created controller if it exists
     */
    @Nullable
    public abstract PageInfoSubpageController createHistoryController(
            PageInfoMainController mainController, PageInfoRowView rowView, String url);

    /**
     * @return Returns the browser context associated with this dialog.
     */
    @NonNull
    public abstract BrowserContextHandle getBrowserContext();

    /**
     * @return Returns the SiteSettingsDelegate for this page info.
     */
    @NonNull
    public abstract SiteSettingsDelegate getSiteSettingsDelegate();

    /**
     * Fetches a favicon for the current page and passes it to callback.
     * The UI will use a fallback icon if null is supplied.
     */
    public abstract void getFavicon(String url, Callback<Drawable> callback);

    /**
     * Checks to see that touch exploration or an accessibility service that can perform gestures
     * is enabled.
     * @return Whether or not accessibility and touch exploration are enabled.
     */
    public abstract boolean isAccessibilityEnabled();

    public abstract FragmentManager getFragmentManager();
}
