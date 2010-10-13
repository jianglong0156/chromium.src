// Copyright (c) 2009 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_COMMON_NET_URL_REQUEST_CONTEXT_GETTER_H_
#define CHROME_COMMON_NET_URL_REQUEST_CONTEXT_GETTER_H_
#pragma once

#include "base/ref_counted.h"
#include "base/task.h"

namespace base {
class MessageLoopProxy;
}

namespace net {
class CookieStore;
}

class URLRequestContext;
struct URLRequestContextGetterTraits;

// Interface for retrieving an URLRequestContext.
class URLRequestContextGetter
    : public base::RefCountedThreadSafe<URLRequestContextGetter,
                                        URLRequestContextGetterTraits> {
 public:
  virtual URLRequestContext* GetURLRequestContext() = 0;

  // Defaults to GetURLRequestContext()->cookie_store(), but
  // implementations can override it.
  virtual net::CookieStore* GetCookieStore();
  // Returns a MessageLoopProxy corresponding to the thread on which the
  // request IO happens (the thread on which the returned URLRequestContext
  // may be used).
  virtual scoped_refptr<base::MessageLoopProxy> GetIOMessageLoopProxy() = 0;

  // Controls whether or not the URLRequestContextGetter considers itself to be
  // the the "main" URLRequestContextGetter.  Note that each Profile will have a
  // "default" URLRequestContextGetter.  Therefore, "is_main" refers to the
  // default URLRequestContextGetter for the "main" Profile.
  // TODO(willchan): Move this code to ChromeURLRequestContextGetter, since this
  // ia a browser process specific concept.
  void set_is_main(bool is_main) { is_main_ = is_main; }

 protected:
  friend class DeleteTask<URLRequestContextGetter>;
  friend struct URLRequestContextGetterTraits;

  URLRequestContextGetter();
  virtual ~URLRequestContextGetter();

  bool is_main() const { return is_main_; }

 private:
  // OnDestruct is meant to ensure deletion on the thread on which the request
  // IO happens.
  void OnDestruct();

  // Indicates whether or not this is the default URLRequestContextGetter for
  // the main Profile.
  bool is_main_;
};

struct URLRequestContextGetterTraits {
  static void Destruct(URLRequestContextGetter* context_getter) {
    context_getter->OnDestruct();
  }
};

#endif  // CHROME_COMMON_NET_URL_REQUEST_CONTEXT_GETTER_H_
