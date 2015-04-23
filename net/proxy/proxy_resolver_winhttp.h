// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef NET_PROXY_PROXY_RESOLVER_WINHTTP_H_
#define NET_PROXY_PROXY_RESOLVER_WINHTTP_H_

#include "base/compiler_specific.h"
#include "net/proxy/proxy_resolver.h"
#include "url/gurl.h"

typedef void* HINTERNET;  // From winhttp.h

namespace net {

// An implementation of ProxyResolver that uses WinHTTP and the system
// proxy settings.
class NET_EXPORT_PRIVATE ProxyResolverWinHttp : public ProxyResolver {
 public:
  ProxyResolverWinHttp();
  ~ProxyResolverWinHttp() override;

  // ProxyResolver implementation:
  int GetProxyForURL(const GURL& url,
                     ProxyInfo* results,
                     const CompletionCallback& /*callback*/,
                     RequestHandle* /*request*/,
                     const BoundNetLog& /*net_log*/) override;
  void CancelRequest(RequestHandle request) override;

  LoadState GetLoadState(RequestHandle request) const override;

  void CancelSetPacScript() override;

  int SetPacScript(const scoped_refptr<ProxyResolverScriptData>& script_data,
                   const CompletionCallback& /*callback*/) override;

 private:
  bool OpenWinHttpSession();
  void CloseWinHttpSession();

  // Proxy configuration is cached on the session handle.
  HINTERNET session_handle_;

  GURL pac_url_;

  DISALLOW_COPY_AND_ASSIGN(ProxyResolverWinHttp);
};

}  // namespace net

#endif  // NET_PROXY_PROXY_RESOLVER_WINHTTP_H_
