// Copyright (c) 2006-2009 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "config.h"

#include "base/compiler_specific.h"

MSVC_PUSH_WARNING_LEVEL(0);
#include "ContextMenu.h"
#include "Document.h"
#include "DocumentLoader.h"
#include "Editor.h"
#include "EventHandler.h"
#include "FrameLoader.h"
#include "FrameView.h"
#include "HitTestResult.h"
#include "HTMLMediaElement.h"
#include "HTMLNames.h"
#include "KURL.h"
#include "Widget.h"
MSVC_POP_WARNING();
#undef LOG

#include "webkit/glue/context_menu_client_impl.h"

#include "base/string_util.h"
#include "webkit/api/public/WebURL.h"
#include "webkit/api/public/WebURLResponse.h"
#include "webkit/glue/context_menu.h"
#include "webkit/glue/glue_util.h"
#include "webkit/glue/webdatasource_impl.h"
#include "webkit/glue/webview_impl.h"

#include "base/word_iterator.h"

using WebKit::WebDataSource;

namespace {

// Helper function to determine whether text is a single word or a sentence.
bool IsASingleWord(const std::wstring& text) {
  WordIterator iter(text, WordIterator::BREAK_WORD);
  int word_count = 0;
  if (!iter.Init()) return false;
  while (iter.Advance()) {
    if (iter.IsWord()) {
      word_count++;
      if (word_count > 1) // More than one word.
        return false;
    }
  }

  // Check for 0 words.
  if (!word_count)
    return false;

  // Has a single word.
  return true;
}

// Helper function to get misspelled word on which context menu
// is to be evolked. This function also sets the word on which context menu
// has been evoked to be the selected word, as required. This function changes
// the selection only when there were no selected characters.
std::wstring GetMisspelledWord(const WebCore::ContextMenu* default_menu,
                               WebCore::Frame* selected_frame) {
  std::wstring misspelled_word_string;

  // First select from selectedText to check for multiple word selection.
  misspelled_word_string = CollapseWhitespace(
      webkit_glue::StringToStdWString(selected_frame->selectedText()),
      false);

  // If some texts were already selected, we don't change the selection.
  if (!misspelled_word_string.empty()) {
    // Don't provide suggestions for multiple words.
    if (!IsASingleWord(misspelled_word_string))
      return L"";
    else
      return misspelled_word_string;
  }

  WebCore::HitTestResult hit_test_result = selected_frame->eventHandler()->
      hitTestResultAtPoint(default_menu->hitTestResult().point(), true);
  WebCore::Node* inner_node = hit_test_result.innerNode();
  WebCore::VisiblePosition pos(inner_node->renderer()->positionForPoint(
      hit_test_result.localPoint()));

  WebCore::VisibleSelection selection;
  if (pos.isNotNull()) {
    selection = WebCore::VisibleSelection(pos);
    selection.expandUsingGranularity(WebCore::WordGranularity);
  }

  if (selection.isRange()) {
    selected_frame->setSelectionGranularity(WebCore::WordGranularity);
  }

  if (selected_frame->shouldChangeSelection(selection))
    selected_frame->selection()->setSelection(selection);

  misspelled_word_string = CollapseWhitespace(
      webkit_glue::StringToStdWString(selected_frame->selectedText()),
                                      false);

  // If misspelled word is empty, then that portion should not be selected.
  // Set the selection to that position only, and do not expand.
  if (misspelled_word_string.empty()) {
    selection = WebCore::VisibleSelection(pos);
    selected_frame->selection()->setSelection(selection);
  }

  return misspelled_word_string;
}

}  // namespace

ContextMenuClientImpl::~ContextMenuClientImpl() {
}

void ContextMenuClientImpl::contextMenuDestroyed() {
  delete this;
}

// Figure out the URL of a page or subframe. Returns |page_type| as the type,
// which indicates page or subframe, or ContextNodeType::NONE if the URL could not
// be determined for some reason.
static ContextNodeType GetTypeAndURLFromFrame(
    WebCore::Frame* frame,
    GURL* url,
    ContextNodeType page_node_type) {
  ContextNodeType node_type;
  if (frame) {
    WebCore::DocumentLoader* dl = frame->loader()->documentLoader();
    if (dl) {
      WebDataSource* ds = WebDataSourceImpl::FromLoader(dl);
      if (ds) {
        node_type = page_node_type;
        *url = ds->hasUnreachableURL() ? ds->unreachableURL()
                                       : ds->request().url();
      }
    }
  }
  return node_type;
}

WebCore::PlatformMenuDescription
    ContextMenuClientImpl::getCustomMenuFromDefaultItems(
        WebCore::ContextMenu* default_menu) {
  // Displaying the context menu in this function is a big hack as we don't
  // have context, i.e. whether this is being invoked via a script or in
  // response to user input (Mouse event WM_RBUTTONDOWN,
  // Keyboard events KeyVK_APPS, Shift+F10). Check if this is being invoked
  // in response to the above input events before popping up the context menu.
  if (!webview_->context_menu_allowed())
    return NULL;

  WebCore::HitTestResult r = default_menu->hitTestResult();
  WebCore::Frame* selected_frame = r.innerNonSharedNode()->document()->frame();

  WebCore::IntPoint menu_point =
      selected_frame->view()->contentsToWindow(r.point());

  ContextNodeType node_type;

  // Links, Images, Media tags, and Image/Media-Links take preference over
  // all else.
  WebCore::KURL link_url = r.absoluteLinkURL();
  if (!link_url.isEmpty()) {
    node_type.type |= ContextNodeType::LINK;
  }

  WebCore::KURL src_url;

  ContextMenuMediaParams media_params;

  if (!r.absoluteImageURL().isEmpty()) {
    src_url = r.absoluteImageURL();
    node_type.type |= ContextNodeType::IMAGE;
  } else if (!r.absoluteMediaURL().isEmpty()) {
    src_url = r.absoluteMediaURL();
          
    // We know that if absoluteMediaURL() is not empty, then this is a media
    // element.
    WebCore::HTMLMediaElement* media_element =
        static_cast<WebCore::HTMLMediaElement*>(r.innerNonSharedNode());
    if (media_element->hasTagName(WebCore::HTMLNames::videoTag)) {
      node_type.type |= ContextNodeType::VIDEO;
    } else if (media_element->hasTagName(WebCore::HTMLNames::audioTag)) {
      node_type.type |= ContextNodeType::AUDIO;
    }

    media_params.playback_rate = media_element->playbackRate();

    if (media_element->paused()) {
      media_params.player_state |= ContextMenuMediaParams::PAUSED;
    }
    if (media_element->muted()) {
      media_params.player_state |= ContextMenuMediaParams::MUTED;
    }
    if (media_element->loop()) {
      media_params.player_state |= ContextMenuMediaParams::LOOP;
    }
    if (media_element->supportsSave()) {
      media_params.player_state |= ContextMenuMediaParams::CAN_SAVE;
    }
    // TODO(ajwong): Report error states in the media player.
  }

  // If it's not a link, an image, a media element, or an image/media link,
  // show a selection menu or a more generic page menu.
  std::wstring selection_text_string;
  std::wstring misspelled_word_string;
  GURL frame_url;
  GURL page_url;
  std::string security_info;

  std::string frame_charset = WideToASCII(
      webkit_glue::StringToStdWString(selected_frame->loader()->encoding()));
  // Send the frame and page URLs in any case.
  ContextNodeType frame_node = ContextNodeType(ContextNodeType::NONE);
  ContextNodeType page_node =
      GetTypeAndURLFromFrame(webview_->main_frame()->frame(),
                             &page_url,
                             ContextNodeType(ContextNodeType::PAGE));
  if (selected_frame != webview_->main_frame()->frame()) {
    frame_node =
      GetTypeAndURLFromFrame(selected_frame,
          &frame_url,
          ContextNodeType(ContextNodeType::FRAME));
  }

  if (r.isSelected()) {
    node_type.type |= ContextNodeType::SELECTION;
    selection_text_string = CollapseWhitespace(
      webkit_glue::StringToStdWString(selected_frame->selectedText()),
      false);
  }

  if (r.isContentEditable()) {
    node_type.type |= ContextNodeType::EDITABLE;
    if (webview_->GetFocusedWebCoreFrame()->editor()->
        isContinuousSpellCheckingEnabled()) {
      misspelled_word_string = GetMisspelledWord(default_menu,
                                                 selected_frame);
    }
  }

  if (node_type.type == ContextNodeType::NONE) {
    if (selected_frame != webview_->main_frame()->frame()) {
      node_type = frame_node;
    } else {
      node_type = page_node;
    }
  }

  // Now retrieve the security info.
  WebCore::DocumentLoader* dl = selected_frame->loader()->documentLoader();
  WebDataSource* ds = WebDataSourceImpl::FromLoader(dl);
  if (ds)
    security_info = ds->response().securityInfo();

  int edit_flags = ContextNodeType::CAN_DO_NONE;
  if (webview_->GetFocusedWebCoreFrame()->editor()->canUndo())
    edit_flags |= ContextNodeType::CAN_UNDO;
  if (webview_->GetFocusedWebCoreFrame()->editor()->canRedo())
    edit_flags |= ContextNodeType::CAN_REDO;
  if (webview_->GetFocusedWebCoreFrame()->editor()->canCut())
    edit_flags |= ContextNodeType::CAN_CUT;
  if (webview_->GetFocusedWebCoreFrame()->editor()->canCopy())
    edit_flags |= ContextNodeType::CAN_COPY;
  if (webview_->GetFocusedWebCoreFrame()->editor()->canPaste())
    edit_flags |= ContextNodeType::CAN_PASTE;
  if (webview_->GetFocusedWebCoreFrame()->editor()->canDelete())
    edit_flags |= ContextNodeType::CAN_DELETE;
  // We can always select all...
  edit_flags |= ContextNodeType::CAN_SELECT_ALL;

  WebViewDelegate* d = webview_->delegate();
  if (d) {
    d->ShowContextMenu(webview_,
                       node_type,
                       menu_point.x(),
                       menu_point.y(),
                       webkit_glue::KURLToGURL(link_url),
                       webkit_glue::KURLToGURL(src_url),
                       page_url,
                       frame_url,
                       media_params,
                       selection_text_string,
                       misspelled_word_string,
                       edit_flags,
                       security_info,
                       frame_charset);
  }
  return NULL;
}

void ContextMenuClientImpl::contextMenuItemSelected(
    WebCore::ContextMenuItem*, const WebCore::ContextMenu*) {
}

void ContextMenuClientImpl::downloadURL(const WebCore::KURL&) {
}

void ContextMenuClientImpl::copyImageToClipboard(const WebCore::HitTestResult&) {
}

void ContextMenuClientImpl::searchWithGoogle(const WebCore::Frame*) {
}

void ContextMenuClientImpl::lookUpInDictionary(WebCore::Frame*) {
}

void ContextMenuClientImpl::speak(const WebCore::String&) {
}

bool ContextMenuClientImpl::isSpeaking() {
  return false;
}

void ContextMenuClientImpl::stopSpeaking() {
}

bool ContextMenuClientImpl::shouldIncludeInspectElementItem() {
    return false;  // TODO(jackson): Eventually include the inspector context menu item
}

#if defined(OS_MACOSX)
void ContextMenuClientImpl::searchWithSpotlight() {
  // TODO(pinkerton): write this
}
#endif
