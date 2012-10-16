# Copyright (c) 2012 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

{
  'variables': {
    'webkit_compositor_bindings_sources': [
      'CCThreadImpl.cpp',
      'CCThreadImpl.h',
      'WebAnimationCurveCommon.cpp',
      'WebAnimationCurveCommon.h',
      'WebAnimationImpl.cpp',
      'WebAnimationImpl.h',
      'WebCompositorImpl.cpp',
      'WebCompositorImpl.h',
      'WebContentLayerImpl.cpp',
      'WebContentLayerImpl.h',
      'WebDelegatedRendererLayerImpl.cpp',
      'WebDelegatedRendererLayerImpl.h',
      'WebExternalTextureLayerImpl.cpp',
      'WebExternalTextureLayerImpl.h',
      'WebFloatAnimationCurveImpl.cpp',
      'WebFloatAnimationCurveImpl.h',
      'WebIOSurfaceLayerImpl.cpp',
      'WebIOSurfaceLayerImpl.h',
      'WebImageLayerImpl.cpp',
      'WebImageLayerImpl.h',
      'WebLayerImpl.cpp',
      'WebLayerImpl.h',
      'WebToCCInputHandlerAdapter.cpp',
      'WebToCCInputHandlerAdapter.h',
      'WebLayerTreeViewImpl.cpp',
      'WebLayerTreeViewImpl.h',
      'WebScrollbarLayerImpl.cpp',
      'WebScrollbarLayerImpl.h',
      'WebSolidColorLayerImpl.cpp',
      'WebSolidColorLayerImpl.h',
      'WebVideoLayerImpl.cpp',
      'WebVideoLayerImpl.h',
      'WebTransformAnimationCurveImpl.cpp',
      'WebTransformAnimationCurveImpl.h',
    ],
  },
  'targets': [
    {
      'target_name': 'webkit_compositor_support',
      'type': 'static_library',
      'dependencies': [
        '../../skia/skia.gyp:skia',
        '<(webkit_src_dir)/Source/WTF/WTF.gyp/WTF.gyp:wtf',
        'webkit_compositor_bindings',
      ],
      'sources': [
        'web_compositor_support_impl.cc',
        'web_compositor_support_impl.h',
      ],
      'includes': [
        '../../cc/cc.gypi',
      ],
      'include_dirs': [
        '../..',
        '../../cc',
        '<(SHARED_INTERMEDIATE_DIR)/webkit',
        '<(webkit_src_dir)/Source/Platform/chromium',
        '<@(cc_stubs_dirs)',
      ],
    },
    {
      'target_name': 'webkit_compositor_bindings',
      'type': 'static_library',
      'includes': [
        '../../cc/cc.gypi',
      ],
      'dependencies': [
        '../../base/base.gyp:base',
        '../../cc/cc.gyp:cc',
        '../../skia/skia.gyp:skia',
        # We have to depend on WTF directly to pick up the correct defines for WTF headers - for instance USE_SYSTEM_MALLOC.
        '<(webkit_src_dir)/Source/WTF/WTF.gyp/WTF.gyp:wtf',
      ],
      'include_dirs': [
        '../../cc',
        '<@(cc_stubs_dirs)',
        '<(webkit_src_dir)/Source/Platform/chromium',
      ],
      'sources': [
        '<@(webkit_compositor_bindings_sources)',
        'webcore_convert.cc',
        'webcore_convert.h',
      ],
    },
  ],
}
