// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/icon_loader.h"

#import <AppKit/AppKit.h>

#include "base/bind.h"
#include "base/message_loop.h"
#include "base/threading/thread.h"
#include "base/sys_string_conversions.h"
#include "ui/gfx/image/image_skia.h"
#include "ui/gfx/image/image_skia_util_mac.h"

void IconLoader::ReadIcon() {
  NSString* group = base::SysUTF8ToNSString(group_);
  NSWorkspace* workspace = [NSWorkspace sharedWorkspace];
  NSImage* icon = [workspace iconForFileType:group];

  if (icon_size_ == ALL) {
    // The NSImage already has all sizes.
    image_.reset(new gfx::Image([icon retain]));
  } else {
    NSSize size = NSZeroSize;
    switch (icon_size_) {
      case IconLoader::SMALL:
        size = NSMakeSize(16, 16);
        break;
      case IconLoader::NORMAL:
        size = NSMakeSize(32, 32);
        break;
      default:
        NOTREACHED();
    }
    gfx::ImageSkia image_skia(gfx::ImageSkiaFromResizedNSImage(icon, size));
    image_skia.MakeThreadSafe();
    image_.reset(new gfx::Image(image_skia));
  }

  target_message_loop_->PostTask(FROM_HERE,
      base::Bind(&IconLoader::NotifyDelegate, this));
}
