// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
//
// This is the main interface for the cast sender.
//
// The AudioFrameInput, VideoFrameInput and PacketReciever interfaces should
// be accessed from the main thread.

#ifndef MEDIA_CAST_CAST_SENDER_H_
#define MEDIA_CAST_CAST_SENDER_H_

#include "base/basictypes.h"
#include "base/callback.h"
#include "base/memory/ref_counted.h"
#include "base/time/tick_clock.h"
#include "base/time/time.h"
#include "media/cast/cast_config.h"
#include "media/cast/cast_environment.h"
#include "media/cast/transport/cast_transport_sender.h"
#include "media/filters/gpu_video_accelerator_factories.h"

namespace media {
class AudioBus;
class GpuVideoAcceleratorFactories;
class VideoFrame;

namespace cast {
class AudioSender;
class VideoSender;

class VideoFrameInput : public base::RefCountedThreadSafe<VideoFrameInput> {
 public:
  // Insert video frames into Cast sender. Frames will be encoded, packetized
  // and sent to the network.
  virtual void InsertRawVideoFrame(
      const scoped_refptr<media::VideoFrame>& video_frame,
      const base::TimeTicks& capture_time) = 0;

 protected:
  virtual ~VideoFrameInput() {}

 private:
  friend class base::RefCountedThreadSafe<VideoFrameInput>;
};

class AudioFrameInput : public base::RefCountedThreadSafe<AudioFrameInput> {
 public:
  // Insert audio frames into Cast sender. Frames will be encoded, packetized
  // and sent to the network.
  // The |audio_bus| must be valid until the |done_callback| is called.
  // The callback is called from the main cast thread as soon as the encoder is
  // done with |audio_bus|; it does not mean that the encoded data has been
  // sent out.
  virtual void InsertAudio(const AudioBus* audio_bus,
                           const base::TimeTicks& recorded_time,
                           const base::Closure& done_callback) = 0;

 protected:
  virtual ~AudioFrameInput() {}

 private:
  friend class base::RefCountedThreadSafe<AudioFrameInput>;
};

// The provided CastTransportSender and the CastSender should be called from the
// main thread.
class CastSender {
 public:
  static scoped_ptr<CastSender> Create(
      scoped_refptr<CastEnvironment> cast_environment,
      transport::CastTransportSender* const transport_sender);

  virtual ~CastSender() {}

  // All video frames for the session should be inserted to this object.
  virtual scoped_refptr<VideoFrameInput> video_frame_input() = 0;

  // All audio frames for the session should be inserted to this object.
  virtual scoped_refptr<AudioFrameInput> audio_frame_input() = 0;

  // All RTCP packets for the session should be inserted to this object.
  // This function and the callback must be called on the main thread.
  virtual transport::PacketReceiverCallback packet_receiver() = 0;

  // Initialize the audio stack. Must be called in order to send audio frames.
  // Status of the initialization will be returned on cast_initialization_cb.
  virtual void InitializeAudio(
      const AudioSenderConfig& audio_config,
      const CastInitializationCallback& cast_initialization_cb) = 0;

  // Initialize the video stack. Must be called in order to send video frames.
  // Status of the initialization will be returned on cast_initialization_cb.
  virtual void InitializeVideo(
      const VideoSenderConfig& video_config,
      const CastInitializationCallback& cast_initialization_cb,
      const scoped_refptr<GpuVideoAcceleratorFactories>& gpu_factories) = 0;
};

}  // namespace cast
}  // namespace media

#endif  // MEDIA_CAST_CAST_SENDER_H_
