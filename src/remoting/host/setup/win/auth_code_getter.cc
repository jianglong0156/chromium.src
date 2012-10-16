// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "remoting/host/setup/win/auth_code_getter.h"

#include "base/time.h"
#include "base/utf_string_conversions.h"
#include "base/win/scoped_bstr.h"
#include "base/win/scoped_variant.h"
#include "remoting/host/setup/oauth_helper.h"

namespace {
const int kUrlPollIntervalMs = 100;
}  // namespace remoting

namespace remoting {

AuthCodeGetter::AuthCodeGetter() :
    browser_(NULL),
    browser_running_(false),
    timer_interval_(base::TimeDelta::FromMilliseconds(kUrlPollIntervalMs)) {
}

AuthCodeGetter::~AuthCodeGetter() {
  KillBrowser();
}

void AuthCodeGetter::GetAuthCode(
    base::Callback<void(const std::string&)> on_auth_code) {
  if (browser_running_) {
    on_auth_code.Run("");
    return;
  }
  on_auth_code_ = on_auth_code;
  HRESULT hr = browser_.CreateInstance(CLSID_InternetExplorer, NULL,
                                       CLSCTX_LOCAL_SERVER);
  if (FAILED(hr)) {
    on_auth_code_.Run("");
    return;
  }
  browser_running_ = true;
  base::win::ScopedBstr url(UTF8ToWide(GetOauthStartUrl()).c_str());
  base::win::ScopedVariant empty_variant;
  hr = browser_->Navigate(url, empty_variant.AsInput(), empty_variant.AsInput(),
                          empty_variant.AsInput(), empty_variant.AsInput());
  if (FAILED(hr)) {
    KillBrowser();
    on_auth_code_.Run("");
    return;
  }
  browser_->put_Visible(VARIANT_TRUE);
  StartTimer();
}

void AuthCodeGetter::StartTimer() {
  timer_.Start(FROM_HERE, timer_interval_, this, &AuthCodeGetter::OnTimer);
}

void AuthCodeGetter::OnTimer() {
  std::string auth_code;
  if (TestBrowserUrl(&auth_code)) {
    on_auth_code_.Run(auth_code);
  } else {
    StartTimer();
  }
}

bool AuthCodeGetter::TestBrowserUrl(std::string* auth_code) {
  *auth_code = "";
  if (!browser_running_) {
    return true;
  }
  base::win::ScopedBstr url;
  HRESULT hr = browser_->get_LocationName(url.Receive());
  if (!SUCCEEDED(hr)) {
    KillBrowser();
    return true;
  }
  *auth_code = GetOauthCodeInUrl(WideToUTF8(static_cast<BSTR>(url)));
  if (!auth_code->empty()) {
    KillBrowser();
    return true;
  }
  return false;
}

void AuthCodeGetter::KillBrowser() {
  if (browser_running_) {
    browser_->Quit();
    browser_running_ = false;
  }
}

}  // namespace remoting
