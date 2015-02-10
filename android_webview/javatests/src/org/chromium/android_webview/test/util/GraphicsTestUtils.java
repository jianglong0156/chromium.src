// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.android_webview.test.util;

import android.graphics.Bitmap;
import android.graphics.Canvas;

import org.chromium.android_webview.AwContents;
import org.chromium.android_webview.test.AwTestBase;
import org.chromium.base.ThreadUtils;

import java.util.concurrent.Callable;

/**
 * Graphics-related test utils.
 */
public class GraphicsTestUtils {
    /**
     * Draws the supplied {@link AwContents} into the returned {@link Bitmap}.
     *
     * @param awContents The contents to draw
     * @param width The width of the bitmap
     * @param height The height of the bitmap
     */
    public static Bitmap drawAwContents(AwContents awContents, int width, int height) {
        return doDrawAwContents(awContents, width, height, null, null);
    }

    /**
     * Draws the supplied {@link AwContents} after applying a translate into the returned
     * {@link Bitmap}.
     *
     * @param awContents The contents to draw
     * @param width The width of the bitmap
     * @param height The height of the bitmap
     * @param dx The distance to translate in X
     * @param dy The distance to translate in Y
     */
    public static Bitmap drawAwContents(
            AwContents awContents, int width, int height, float dx, float dy) {
        return doDrawAwContents(awContents, width, height, dx, dy);
    }

    public static int sampleBackgroundColorOnUiThread(final AwContents awContents)
            throws Exception {
        return ThreadUtils.runOnUiThreadBlocking(new Callable<Integer>() {
            @Override
            public Integer call() throws Exception {
                return drawAwContents(awContents, 10, 10, 0, 0).getPixel(0, 0);
            }
        });
    }

    public static void pollForBackgroundColor(final AwContents awContents, final int c)
            throws Throwable {
        AwTestBase.poll(new Callable<Boolean>() {
            @Override
            public Boolean call() throws Exception {
                return sampleBackgroundColorOnUiThread(awContents) == c;
            }
        });
    }

    private static Bitmap doDrawAwContents(
            AwContents awContents, int width, int height, Float dx, Float dy) {
        Bitmap bitmap = Bitmap.createBitmap(width, height, Bitmap.Config.ARGB_8888);
        Canvas canvas = new Canvas(bitmap);
        if (dx != null && dy != null) {
            canvas.translate(dx, dy);
        }
        awContents.onDraw(canvas);
        return bitmap;
    }
}
