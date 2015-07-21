// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_VIEWS_TOOLBAR_DEVTOOL_BUTTON_H__
#define CHROME_BROWSER_UI_VIEWS_TOOLBAR_DEVTOOL_BUTTON_H__

#include "base/basictypes.h"
#include "base/gtest_prod_util.h"
#include "base/timer/timer.h"
#include "chrome/browser/ui/views/toolbar/toolbar_button.h"
#include "ui/base/models/simple_menu_model.h"
#include "ui/views/controls/button/button.h"

class CommandUpdater;

////////////////////////////////////////////////////////////////////////////////
//
// DevtoolButton
//
// The reload button in the toolbar, which changes to a stop button when a page
// load is in progress. Trickiness comes from the desire to have the 'stop'
// button not change back to 'reload' if the user's mouse is hovering over it
// (to prevent mis-clicks).
//
////////////////////////////////////////////////////////////////////////////////

class DevtoolButton : public ToolbarButton,
                     public views::ButtonListener{
 public:

  // The button's class name.
  static const char kViewClassName[];

  explicit DevtoolButton(CommandUpdater* command_updater);
  ~DevtoolButton() override;

  void LoadImages();

  // ToolbarButton:
  void OnMouseExited(const ui::MouseEvent& event) override;
  bool GetTooltipText(const gfx::Point& p,
                      base::string16* tooltip) const override;
  const char* GetClassName() const override;
  void GetAccessibleState(ui::AXViewState* state) override;
  bool ShouldShowMenu() override;

  // views::ButtonListener:
  void ButtonPressed(views::Button* /* button */,
                     const ui::Event& event) override;

 private:
  friend class DevtoolButtonTest;

  void ExecuteBrowserCommand(int command, int event_flags);

  base::OneShotTimer<DevtoolButton> double_click_timer_;
  base::OneShotTimer<DevtoolButton> stop_to_reload_timer_;

  // This may be NULL when testing.
  CommandUpdater* command_updater_;

  // TESTING ONLY
  // True if we should pretend the button is hovered.
  bool testing_mouse_hovered_;
  // Increments when we would tell the browser to "reload", so
  // test code can tell whether we did so (as there may be no |browser_|).
  int testing_reload_count_;

  DISALLOW_IMPLICIT_CONSTRUCTORS(DevtoolButton);
};

#endif  // CHROME_BROWSER_UI_VIEWS_TOOLBAR_DEVTOOL_BUTTON_H__
