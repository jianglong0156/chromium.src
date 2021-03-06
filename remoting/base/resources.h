// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef REMOTING_HOST_BASE_RESOURCES_H_
#define REMOTING_HOST_BASE_RESOURCES_H_

#include <string>

namespace remoting {

// Loads chromoting resources. Returns false in case of a failure.
bool LoadResources(const std::string& locale);

}  // namespace remoting

#endif  // REMOTING_HOST_BASE_RESOURCES_H_
