// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
//
// This file contains the implementation of GenericV4L2Device used on
// platforms, which provide generic V4L2 video codec devices.

#ifndef CONTENT_COMMON_GPU_MEDIA_GENERIC_V4L2_VIDEO_DEVICE_H_
#define CONTENT_COMMON_GPU_MEDIA_GENERIC_V4L2_VIDEO_DEVICE_H_

#include "content/common/gpu/media/v4l2_video_device.h"

namespace content {

class GenericV4L2Device : public V4L2Device {
 public:
  explicit GenericV4L2Device(Type type);
  virtual ~GenericV4L2Device();

  // V4L2Device implementation.
  int Ioctl(int request, void* arg) override;
  bool Poll(bool poll_device, bool* event_pending) override;
  bool SetDevicePollInterrupt() override;
  bool ClearDevicePollInterrupt() override;
  void* Mmap(void* addr,
             unsigned int len,
             int prot,
             int flags,
             unsigned int offset) override;
  void Munmap(void* addr, unsigned int len) override;
  bool Initialize() override;
  bool CanCreateEGLImageFrom(uint32_t v4l2_pixfmt) override;
  EGLImageKHR CreateEGLImage(EGLDisplay egl_display,
                             EGLContext egl_context,
                             GLuint texture_id,
                             gfx::Size frame_buffer_size,
                             unsigned int buffer_index,
                             uint32_t v4l2_pixfmt,
                             size_t num_v4l2_planes);
  EGLBoolean DestroyEGLImage(EGLDisplay egl_display,
                             EGLImageKHR egl_image) override;
  GLenum GetTextureTarget() override;
  uint32 PreferredInputFormat() override;

 private:
  const Type type_;

  // The actual device fd.
  int device_fd_;

  // eventfd fd to signal device poll thread when its poll() should be
  // interrupted.
  int device_poll_interrupt_fd_;

  DISALLOW_COPY_AND_ASSIGN(GenericV4L2Device);
};
}  //  namespace content

#endif  // CONTENT_COMMON_GPU_MEDIA_GENERIC_V4L2_VIDEO_DEVICE_H_
