// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_VIEWS_BOOKMARKS_BOOKMARK_BAR_INSTRUCTIONS_VIEW_H_
#define CHROME_BROWSER_UI_VIEWS_BOOKMARKS_BOOKMARK_BAR_INSTRUCTIONS_VIEW_H_

#include "base/basictypes.h"
#include "base/compiler_specific.h"
#include "ui/base/accessibility/accessible_view_state.h"
#include "ui/views/controls/link_listener.h"
#include "ui/views/view.h"

namespace chrome {
class BookmarkBarInstructionsDelegate;
namespace search {
struct Mode;
}
}

namespace views {
class Label;
class Link;
}

// BookmarkBarInstructionsView is a child of the bookmark bar that is visible
// when the user has no bookmarks on the bookmark bar.
// BookmarkBarInstructionsView shows a description of the bookmarks bar along
// with a link to import bookmarks. Clicking the link results in notifying the
// delegate.
class BookmarkBarInstructionsView : public views::View,
                                    public views::LinkListener {
 public:
  explicit BookmarkBarInstructionsView(
      chrome::BookmarkBarInstructionsDelegate* delegate);

  // Updates background color according to |search_mode|.
  // In NTP mode, background of bookmark bar is transparent, so the instruction
  // text should not use subpixel rendering; |views::Label| uses the alpha value
  // of background color to determine the text rendering flags, so specifically
  // setting the backround color to transparent white will force disabling of
  // subpixel rendering.
  // Note that |views::Label| doesn't paint this background color; it's only
  // used to determine color rendering flags of text.
  void UpdateBackgroundColor(const chrome::search::Mode& search_mode);

  // views::View overrides.
  virtual gfx::Size GetPreferredSize() OVERRIDE;
  virtual void Layout() OVERRIDE;
  virtual void OnThemeChanged() OVERRIDE;
  virtual void ViewHierarchyChanged(bool is_add,
                                    views::View* parent,
                                    views::View* child) OVERRIDE;
  virtual void GetAccessibleState(ui::AccessibleViewState* state) OVERRIDE;

  // views::LinkListener overrides.
  virtual void LinkClicked(views::Link* source, int event_flags) OVERRIDE;

 private:
  void UpdateColors();

  chrome::BookmarkBarInstructionsDelegate* delegate_;

  views::Label* instructions_;
  views::Link* import_link_;

  // The baseline of the child views. This is -1 if none of the views support a
  // baseline.
  int baseline_;

  // Have the colors of the child views been updated? This is initially false
  // and set to true once we have a valid ThemeProvider.
  bool updated_colors_;

  DISALLOW_COPY_AND_ASSIGN(BookmarkBarInstructionsView);
};

#endif  // CHROME_BROWSER_UI_VIEWS_BOOKMARKS_BOOKMARK_BAR_INSTRUCTIONS_VIEW_H_
