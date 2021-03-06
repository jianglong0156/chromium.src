// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "media/base/audio_renderer_mixer_input.h"

#include "base/logging.h"
#include "media/base/audio_renderer_mixer.h"

namespace media {

AudioRendererMixerInput::AudioRendererMixerInput(
    const GetMixerCB& get_mixer_cb, const RemoveMixerCB& remove_mixer_cb)
    : playing_(false),
      initialized_(false),
      started_(false),
      volume_(1.0f),
      get_mixer_cb_(get_mixer_cb),
      remove_mixer_cb_(remove_mixer_cb),
      mixer_(NULL),
      callback_(NULL) {
}

AudioRendererMixerInput::~AudioRendererMixerInput() {
  // Mixer is no longer safe to use after |remove_mixer_cb_| has been called.
  if (initialized_)
    remove_mixer_cb_.Run(params_);
}

void AudioRendererMixerInput::Initialize(
    const AudioParameters& params,
    AudioRendererSink::RenderCallback* callback) {
  DCHECK(!initialized_);
  params_ = params;
  mixer_ = get_mixer_cb_.Run(params_);
  callback_ = callback;
  initialized_ = true;
}

void AudioRendererMixerInput::Start() {
  DCHECK(initialized_);
  DCHECK(!started_);
  mixer_->AddMixerInput(this);
  started_ = true;
}

void AudioRendererMixerInput::Stop() {
  // Stop() may be called at any time, so we can't blindly remove our input.
  if (started_) {
    mixer_->RemoveMixerInput(this);
    started_ = false;
  }
  playing_ = false;
}

void AudioRendererMixerInput::Play() {
  DCHECK(initialized_);
  DCHECK(started_);
  playing_ = true;
}

void AudioRendererMixerInput::Pause(bool /* flush */) {
  DCHECK(initialized_);
  DCHECK(started_);
  // We don't care about flush since Pause() simply indicates we should send
  // silence to AudioRendererMixer.
  playing_ = false;
}

bool AudioRendererMixerInput::SetVolume(double volume) {
  volume_ = volume;
  return true;
}

void AudioRendererMixerInput::GetVolume(double* volume) {
  *volume = volume_;
}

}  // namespace media
