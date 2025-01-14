// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.components.browser_ui.notifications;

import android.app.Notification;
import android.app.PendingIntent;
import android.graphics.Bitmap;
import android.graphics.drawable.Icon;
import android.net.Uri;
import android.os.Bundle;
import android.support.v4.media.session.MediaSessionCompat;
import android.widget.RemoteViews;

import androidx.core.app.NotificationCompat;

/**
 * Abstraction over Notification.Builder and NotificationCompat.Builder interfaces.
 */
public interface NotificationWrapperBuilder {
    NotificationWrapperBuilder setAutoCancel(boolean autoCancel);

    @Deprecated
    NotificationWrapperBuilder setContentIntent(PendingIntent contentIntent);

    NotificationWrapperBuilder setContentIntent(PendingIntentProvider contentIntent);

    NotificationWrapperBuilder setContentTitle(CharSequence title);

    NotificationWrapperBuilder setContentText(CharSequence text);

    NotificationWrapperBuilder setSmallIcon(int icon);

    NotificationWrapperBuilder setSmallIcon(Icon icon);

    NotificationWrapperBuilder setColor(int argb);

    NotificationWrapperBuilder setTicker(CharSequence text);

    NotificationWrapperBuilder setLocalOnly(boolean localOnly);

    NotificationWrapperBuilder setGroup(String group);

    NotificationWrapperBuilder setGroupSummary(boolean isGroupSummary);

    NotificationWrapperBuilder addExtras(Bundle extras);

    NotificationWrapperBuilder setOngoing(boolean ongoing);

    NotificationWrapperBuilder setVisibility(int visibility);

    NotificationWrapperBuilder setShowWhen(boolean showWhen);

    @Deprecated
    NotificationWrapperBuilder addAction(int icon, CharSequence title, PendingIntent intent);

    /**
     * @param actionType is for UMA. In Chrome, this is {@link NotificationUmaTracker.ActionType}.
     */
    NotificationWrapperBuilder addAction(
            int icon, CharSequence title, PendingIntentProvider intent, int actionType);

    @Deprecated
    NotificationWrapperBuilder addAction(Notification.Action action);

    /**
     * @param actionType is for UMA. In Chrome, this is {@link NotificationUmaTracker.ActionType}.
     */
    NotificationWrapperBuilder addAction(Notification.Action action, int flags, int actionType);

    @Deprecated
    NotificationWrapperBuilder addAction(NotificationCompat.Action action);

    /**
     * @param actionType is for UMA. In Chrome, this is {@link NotificationUmaTracker.ActionType}.
     */
    NotificationWrapperBuilder addAction(
            NotificationCompat.Action action, int flags, int actionType);

    @Deprecated
    NotificationWrapperBuilder setDeleteIntent(PendingIntent intent);

    NotificationWrapperBuilder setDeleteIntent(PendingIntentProvider intent);

    /**
     * Sets the priority of single notification on Android versions prior to Oreo.
     * (From Oreo onwards, priority is instead determined by channel importance.)
     */
    NotificationWrapperBuilder setPriorityBeforeO(int pri);

    NotificationWrapperBuilder setProgress(int max, int percentage, boolean indeterminate);

    NotificationWrapperBuilder setSubText(CharSequence text);

    NotificationWrapperBuilder setContentInfo(String info);

    NotificationWrapperBuilder setWhen(long time);

    NotificationWrapperBuilder setLargeIcon(Bitmap icon);

    NotificationWrapperBuilder setVibrate(long[] vibratePattern);

    NotificationWrapperBuilder setSound(Uri sound);

    NotificationWrapperBuilder setSilent(boolean silent);

    NotificationWrapperBuilder setDefaults(int defaults);

    NotificationWrapperBuilder setOnlyAlertOnce(boolean onlyAlertOnce);

    NotificationWrapperBuilder setPublicVersion(Notification publicNotification);

    NotificationWrapperBuilder setContent(RemoteViews views);

    NotificationWrapperBuilder setBigPictureStyle(Bitmap bigPicture, CharSequence summaryText);

    NotificationWrapperBuilder setBigTextStyle(CharSequence bigText);

    NotificationWrapperBuilder setMediaStyle(MediaSessionCompat session, int[] actions);

    NotificationWrapperBuilder setCategory(String category);

    NotificationWrapper buildWithBigContentView(RemoteViews bigView);

    NotificationWrapper buildWithBigTextStyle(String bigText);

    @Deprecated
    Notification build();

    NotificationWrapper buildNotificationWrapper();
}
