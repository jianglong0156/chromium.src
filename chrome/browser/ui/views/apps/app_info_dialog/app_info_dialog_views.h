// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_VIEWS_APPS_APP_INFO_DIALOG_APP_INFO_DIALOG_VIEWS_H_
#define CHROME_BROWSER_UI_VIEWS_APPS_APP_INFO_DIALOG_APP_INFO_DIALOG_VIEWS_H_

#include "extensions/browser/extension_registry_observer.h"
#include "ui/gfx/native_widget_types.h"
#include "ui/views/view.h"

class Profile;

namespace extensions {
class Extension;
class ExtensionRegistry;
}

namespace views {
class ScrollView;
}

// View the information about a particular chrome application.

class AppInfoDialog : public views::View,
                      public extensions::ExtensionRegistryObserver {
 public:
  AppInfoDialog(gfx::NativeWindow parent_window,
                Profile* profile,
                const extensions::Extension* app);
  virtual ~AppInfoDialog();

 private:
  // Closes the dialog.
  void Close();

  void StartObservingExtensionRegistry();
  void StopObservingExtensionRegistry();

  // Overridden from extensions::ExtensionRegistryObserver:
  virtual void OnExtensionUninstalled(
      content::BrowserContext* browser_context,
      const extensions::Extension* extension,
      extensions::UninstallReason reason) OVERRIDE;
  virtual void OnShutdown(extensions::ExtensionRegistry* registry) OVERRIDE;

  // UI elements of the dialog.
  views::View* dialog_header_;
  views::ScrollView* dialog_body_;
  views::View* dialog_footer_;

  Profile* profile_;
  std::string app_id_;
  extensions::ExtensionRegistry* extension_registry_;

  DISALLOW_COPY_AND_ASSIGN(AppInfoDialog);
};

#endif  // CHROME_BROWSER_UI_VIEWS_APPS_APP_INFO_DIALOG_APP_INFO_DIALOG_VIEWS_H_
