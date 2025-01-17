// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.ui.base;

import android.content.ClipData;
import android.content.ClipboardManager;
import android.content.Context;
import android.text.SpannableString;
import android.text.Spanned;
import android.text.style.BackgroundColorSpan;

import androidx.test.filters.SmallTest;

import org.hamcrest.Matchers;
import org.junit.Assert;
import org.junit.Test;
import org.junit.runner.RunWith;

import org.chromium.base.BuildInfo;
import org.chromium.base.test.BaseJUnit4ClassRunner;
import org.chromium.base.test.util.Batch;
import org.chromium.base.test.util.CallbackHelper;
import org.chromium.base.test.util.Criteria;
import org.chromium.base.test.util.CriteriaHelper;
import org.chromium.base.test.util.MinAndroidSdkLevel;
import org.chromium.content_public.browser.test.NativeLibraryTestUtils;
import org.chromium.content_public.browser.test.util.TestThreadUtils;
import org.chromium.ui.test.util.DummyUiActivityTestCase;

import java.util.concurrent.TimeoutException;

/**
 * Clipboard tests for Android platform that depend on access to the ClipboardManager.
 */
@RunWith(BaseJUnit4ClassRunner.class)
@Batch(Batch.UNIT_TESTS)
public class ClipboardAndroidTest extends DummyUiActivityTestCase {
    private static final String TEXT_URL = "http://www.foo.com/";
    private static final String MIX_TEXT_URL = "test http://www.foo.com http://www.bar.com";
    private static final String MIX_TEXT_URL_NO_PROTOCOL = "test www.foo.com www.bar.com";

    @Override
    public void setUpTest() throws Exception {
        super.setUpTest();
        NativeLibraryTestUtils.loadNativeLibraryNoBrowserProcess();
    }

    @Override
    public void tearDownTest() throws Exception {
        ClipboardAndroidTestSupport.cleanup();
        super.tearDownTest();
    }

    /**
     * Test that if another application writes some text to the pasteboard the clipboard properly
     * invalidates other types.
     */
    @Test
    @SmallTest
    public void internalClipboardInvalidation() throws TimeoutException {
        // Write to the clipboard in native and ensure that is propagated to the platform clipboard.
        final String originalText = "foo";
        TestThreadUtils.runOnUiThreadBlocking(() -> {
            Assert.assertTrue("Original text was not written to the native clipboard.",
                    ClipboardAndroidTestSupport.writeHtml(originalText));
        });

        CallbackHelper helper = new CallbackHelper();
        ClipboardManager.OnPrimaryClipChangedListener clipboardChangedListener =
                new ClipboardManager.OnPrimaryClipChangedListener() {
                    @Override
                    public void onPrimaryClipChanged() {
                        helper.notifyCalled();
                    }
                };

        // Assert that the ClipboardManager contains the original text. Then simulate another
        // application writing to the clipboard.
        final String invalidatingText = "Hello, World!";
        TestThreadUtils.runOnUiThreadBlocking(() -> {
            ClipboardManager clipboardManager =
                    (ClipboardManager) getActivity().getSystemService(Context.CLIPBOARD_SERVICE);
            clipboardManager.addPrimaryClipChangedListener(clipboardChangedListener);

            Assert.assertEquals("Original text not found in ClipboardManager.", originalText,
                    Clipboard.getInstance().clipDataToHtmlText(clipboardManager.getPrimaryClip()));

            clipboardManager.setPrimaryClip(ClipData.newPlainText(null, invalidatingText));
        });

        helper.waitForFirst("ClipboardManager did not notify of PrimaryClip change.");

        // Assert that the overwrite from another application is registered by the native clipboard.
        TestThreadUtils.runOnUiThreadBlocking(() -> {
            Assert.assertTrue("Invalidating text not found in the native clipboard.",
                    ClipboardAndroidTestSupport.clipboardContains(invalidatingText));

            ClipboardManager clipboardManager =
                    (ClipboardManager) getActivity().getSystemService(Context.CLIPBOARD_SERVICE);
            clipboardManager.removePrimaryClipChangedListener(clipboardChangedListener);
        });
    }

    @Test
    @SmallTest
    public void hasHTMLOrStyledTextForNormalTextTest() {
        TestThreadUtils.runOnUiThreadBlocking(() -> {
            Clipboard.getInstance().setText("SampleTextToCopy");
            Assert.assertFalse(Clipboard.getInstance().hasHTMLOrStyledText());
        });
    }

    @Test
    @SmallTest
    public void hasHTMLOrStyledTextForStyledTextTest() {
        SpannableString spanString = new SpannableString("SpannableString");
        spanString.setSpan(new BackgroundColorSpan(0), 0, 4, Spanned.SPAN_EXCLUSIVE_EXCLUSIVE);
        ClipData clipData =
                ClipData.newPlainText("text", spanString.subSequence(0, spanString.length() - 1));
        TestThreadUtils.runOnUiThreadBlocking(() -> {
            Clipboard.getInstance().setPrimaryClipNoException(clipData);
            Assert.assertTrue(Clipboard.getInstance().hasHTMLOrStyledText());
        });
    }

    @Test
    @SmallTest
    public void hasHTMLOrStyledTextForHtmlTextTest() {
        TestThreadUtils.runOnUiThreadBlocking(() -> {
            Clipboard.getInstance().setHTMLText(
                    "<span style=\"color: red;\">HTMLTextToCopy</span>", "HTMLTextToCopy");
            Assert.assertTrue(Clipboard.getInstance().hasHTMLOrStyledText());
        });
    }

    @Test
    @SmallTest
    public void hasUrlAndGetUrlTest() {
        TestThreadUtils.runOnUiThreadBlocking(() -> { Clipboard.getInstance().setText(TEXT_URL); });

        CriteriaHelper.pollUiThread(() -> {
            Criteria.checkThat(Clipboard.getInstance().hasUrl(), Matchers.is(true));
            Criteria.checkThat(Clipboard.getInstance().getUrl(), Matchers.is(TEXT_URL));
        });
    }

    // Only first URL is returned on S+ if clipboard contains multiple URLs.
    @Test
    @SmallTest
    @MinAndroidSdkLevel(BuildInfo.ANDROID_S_API_SDK_INT)
    public void hasUrlAndGetUrlMixTextAndLinkTest() {
        TestThreadUtils.runOnUiThreadBlocking(
                () -> { Clipboard.getInstance().setText(MIX_TEXT_URL); });

        CriteriaHelper.pollUiThread(() -> {
            Criteria.checkThat(Clipboard.getInstance().hasUrl(), Matchers.is(true));
            Criteria.checkThat(Clipboard.getInstance().getUrl(), Matchers.is(TEXT_URL));
        });
    }

    // Only first URL is returned on S+ if clipboard contains multiple URLs.
    @Test
    @SmallTest
    @MinAndroidSdkLevel(BuildInfo.ANDROID_S_API_SDK_INT)
    public void hasUrlAndGetUrlMixTextAndLinkWithoutProtocolTest() {
        TestThreadUtils.runOnUiThreadBlocking(
                () -> { Clipboard.getInstance().setText(MIX_TEXT_URL_NO_PROTOCOL); });

        CriteriaHelper.pollUiThread(() -> {
            Criteria.checkThat(Clipboard.getInstance().hasUrl(), Matchers.is(true));
            Criteria.checkThat(Clipboard.getInstance().getUrl(), Matchers.is(TEXT_URL));
        });
    }
}
