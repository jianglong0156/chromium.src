# Copyright (c) 2012 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

def web_socket_do_extra_handshake(_request):
  pass  # Always accept.


def web_socket_transfer_data(request):
  while True:
    line = request.ws_stream.receive_message()
    if line is None:
      return
    if isinstance(line, unicode):
      request.ws_stream.send_message(line, binary=False)
    else:
      request.ws_stream.send_message(line, binary=True)

