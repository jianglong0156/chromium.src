// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/views/toolbar/devtool_button.h"

#include "base/strings/utf_string_conversions.h"
#include "chrome/app/chrome_command_ids.h"
#include "chrome/browser/command_updater.h"
#include "chrome/browser/search/search.h"
#include "chrome/browser/ui/search/search_model.h"
#include "chrome/grit/generated_resources.h"
#include "grit/theme_resources.h"
#include "ui/base/l10n/l10n_util.h"
#include "ui/base/models/simple_menu_model.h"
#include "ui/base/theme_provider.h"
#include "ui/base/window_open_disposition.h"
#include "ui/views/metrics.h"
#include "ui/views/widget/widget.h"

// DevtoolButton ---------------------------------------------------------------

// static
const char DevtoolButton::kViewClassName[] = "DevtoolButton";

DevtoolButton::DevtoolButton(CommandUpdater* command_updater)
    : ToolbarButton(this, NULL),
      command_updater_(command_updater),
      testing_mouse_hovered_(false),
      testing_reload_count_(0) {
}

DevtoolButton::~DevtoolButton() {
}

void DevtoolButton::LoadImages() {
    ui::ThemeProvider* tp = GetThemeProvider();
    // |tp| can be NULL in unit tests.
    if (tp) {
        SetImage(views::Button::STATE_NORMAL, *(tp->GetImageSkiaNamed(IDR_SHOW_DEVTOOL)));
    }

  SchedulePaint();
  PreferredSizeChanged();
}

void DevtoolButton::OnMouseExited(const ui::MouseEvent& event) {
  ToolbarButton::OnMouseExited(event);
//   if (!IsMenuShowing())
//     ChangeMode(intended_mode_, true);
}

bool DevtoolButton::GetTooltipText(const gfx::Point& p,
                                  base::string16* tooltip) const {
  tooltip->assign(l10n_util::GetStringUTF16(IDS_TOOLTIP_RELOAD));
  return true;
}

const char* DevtoolButton::GetClassName() const {
  return kViewClassName;
}

void DevtoolButton::GetAccessibleState(ui::AXViewState* state) {
    CustomButton::GetAccessibleState(state);
}

bool DevtoolButton::ShouldShowMenu() {
    return false;
}

void DevtoolButton::ButtonPressed(views::Button* /* button */,
                                 const ui::Event& event) {
  ClearPendingMenu();
  ExecuteBrowserCommand(IDC_SHOW_DEVTOOL, event.flags());
  ExecuteBrowserCommand(IDC_RECORD_APP_CLICK_CDT, event.flags());
}

void DevtoolButton::ExecuteBrowserCommand(int command, int event_flags) {
  if (!command_updater_)
    return;
  command_updater_->ExecuteCommandWithDisposition(
      command, ui::DispositionFromEventFlags(event_flags));
}
