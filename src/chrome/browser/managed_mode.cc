// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/managed_mode.h"

#include "base/command_line.h"
#include "chrome/browser/browser_process.h"
#include "chrome/browser/extensions/extension_service.h"
#include "chrome/browser/extensions/extension_system.h"
#include "chrome/browser/managed_mode_url_filter.h"
#include "chrome/browser/prefs/pref_service.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/browser/ui/browser_list.h"
#include "chrome/browser/ui/browser_window.h"
#include "chrome/common/chrome_notification_types.h"
#include "chrome/common/chrome_switches.h"
#include "chrome/common/pref_names.h"
#include "content/public/browser/browser_thread.h"
#include "content/public/browser/notification_service.h"
#include "grit/generated_resources.h"
#include "ui/base/l10n/l10n_util.h"

using content::BrowserThread;

// A bridge from ManagedMode (which lives on the UI thread) to
// ManagedModeURLFilter (which lives on the IO thread).
class ManagedMode::URLFilterContext {
 public:
  URLFilterContext() {}
  ~URLFilterContext() {}

  const ManagedModeURLFilter* url_filter() const {
    return &url_filter_;
  }

  void SetActive(bool in_managed_mode) {
    DCHECK(BrowserThread::CurrentlyOn(BrowserThread::UI));
    // Because ManagedMode is a singleton, we can pass the pointer to
    // |url_filter_| unretained.
    BrowserThread::PostTask(BrowserThread::IO,
                            FROM_HERE,
                            base::Bind(
                                &ManagedModeURLFilter::SetActive,
                                base::Unretained(&url_filter_),
                                in_managed_mode));
  }

  void ShutdownOnUIThread() {
    DCHECK(BrowserThread::CurrentlyOn(BrowserThread::UI));
    BrowserThread::DeleteSoon(BrowserThread::IO, FROM_HERE, this);
  }

 private:
  ManagedModeURLFilter url_filter_;

  DISALLOW_COPY_AND_ASSIGN(URLFilterContext);
};

// static
ManagedMode* ManagedMode::GetInstance() {
  return Singleton<ManagedMode, LeakySingletonTraits<ManagedMode> >::get();
}

// static
void ManagedMode::RegisterPrefs(PrefService* prefs) {
  prefs->RegisterBooleanPref(prefs::kInManagedMode, false);
}

// static
void ManagedMode::Init(Profile* profile) {
  GetInstance()->InitImpl(profile);
}

void ManagedMode::InitImpl(Profile* profile) {
  DCHECK(g_browser_process);
  DCHECK(g_browser_process->local_state());

  Profile* original_profile = profile->GetOriginalProfile();
  // Set the value directly in the PrefService instead of using
  // CommandLinePrefStore so we can change it at runtime.
  if (CommandLine::ForCurrentProcess()->HasSwitch(switches::kNoManaged)) {
    SetInManagedMode(NULL);
  } else if (IsInManagedModeImpl() ||
      CommandLine::ForCurrentProcess()->HasSwitch(switches::kManaged)) {
    SetInManagedMode(original_profile);
  }
}

// static
bool ManagedMode::IsInManagedMode() {
  return GetInstance()->IsInManagedModeImpl();
}

bool ManagedMode::IsInManagedModeImpl() const {
  // |g_browser_process| can be NULL during startup.
  if (!g_browser_process)
    return false;
  // Local State can be NULL during unit tests.
  if (!g_browser_process->local_state())
    return false;
  return g_browser_process->local_state()->GetBoolean(prefs::kInManagedMode);
}

// static
void ManagedMode::EnterManagedMode(Profile* profile,
                                   const EnterCallback& callback) {
  GetInstance()->EnterManagedModeImpl(profile, callback);
}

void ManagedMode::EnterManagedModeImpl(Profile* profile,
                                       const EnterCallback& callback) {
  Profile* original_profile = profile->GetOriginalProfile();
  if (IsInManagedModeImpl()) {
    callback.Run(original_profile == managed_profile_);
    return;
  }
  if (!callbacks_.empty()) {
    // We are already in the process of entering managed mode, waiting for
    // browsers to close. Don't allow entering managed mode again for a
    // different profile, and queue the callback for the same profile.
    if (original_profile != managed_profile_)
      callback.Run(false);
    else
      callbacks_.push_back(callback);
    return;
  }

  if (!PlatformConfirmEnter()) {
    callback.Run(false);
    return;
  }
  // Close all other profiles.
  // At this point, we shouldn't be waiting for other browsers to close (yet).
  DCHECK_EQ(0u, browsers_to_close_.size());
  for (BrowserList::const_iterator i = BrowserList::begin();
       i != BrowserList::end(); ++i) {
    if ((*i)->profile()->GetOriginalProfile() != original_profile)
      browsers_to_close_.insert(*i);
  }

  if (browsers_to_close_.empty()) {
    SetInManagedMode(original_profile);
    callback.Run(true);
    return;
  }
  // Remember the profile we're trying to manage while we wait for other
  // browsers to close.
  managed_profile_ = original_profile;
  callbacks_.push_back(callback);
  registrar_.Add(this, chrome::NOTIFICATION_CLOSE_ALL_BROWSERS_REQUEST,
                 content::NotificationService::AllSources());
  registrar_.Add(this, chrome::NOTIFICATION_BROWSER_CLOSE_CANCELLED,
                 content::NotificationService::AllSources());
  for (std::set<Browser*>::const_iterator i = browsers_to_close_.begin();
       i != browsers_to_close_.end(); ++i) {
    (*i)->window()->Close();
  }
}

// static
void ManagedMode::LeaveManagedMode() {
  GetInstance()->LeaveManagedModeImpl();
}

void ManagedMode::LeaveManagedModeImpl() {
  bool confirmed = PlatformConfirmLeave();
  if (confirmed)
    SetInManagedMode(NULL);
}

// static
const ManagedModeURLFilter* ManagedMode::GetURLFilter() {
  return GetInstance()->GetURLFilterImpl();
}

const ManagedModeURLFilter* ManagedMode::GetURLFilterImpl() {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::IO));
  return url_filter_context_->url_filter();
}

std::string ManagedMode::GetDebugPolicyProviderName() const {
  // Save the string space in official builds.
#ifdef NDEBUG
  NOTREACHED();
  return std::string();
#else
  return "Managed Mode";
#endif
}

bool ManagedMode::UserMayLoad(const extensions::Extension* extension,
                              string16* error) const {
  return ExtensionManagementPolicyImpl(error);
}

bool ManagedMode::UserMayModifySettings(const extensions::Extension* extension,
                                        string16* error) const {
  return ExtensionManagementPolicyImpl(error);
}

bool ManagedMode::ExtensionManagementPolicyImpl(string16* error) const {
  if (!IsInManagedModeImpl())
    return true;

  if (error)
    *error = l10n_util::GetStringUTF16(IDS_EXTENSIONS_LOCKED_MANAGED_MODE);
  return false;
}

void ManagedMode::OnBrowserAdded(Browser* browser) {
  // Return early if we don't have any queued callbacks.
  if (callbacks_.empty())
    return;

  DCHECK(managed_profile_);
  if (browser->profile()->GetOriginalProfile() != managed_profile_)
    FinalizeEnter(false);
}

void ManagedMode::OnBrowserRemoved(Browser* browser) {
  // Return early if we don't have any queued callbacks.
  if (callbacks_.empty())
    return;

  DCHECK(managed_profile_);
  if (browser->profile()->GetOriginalProfile() == managed_profile_) {
    // Ignore closing browser windows that are in managed mode.
    return;
  }
  size_t count = browsers_to_close_.erase(browser);
  DCHECK_EQ(1u, count);
  if (browsers_to_close_.empty())
    FinalizeEnter(true);
}

ManagedMode::ManagedMode() : managed_profile_(NULL),
                             url_filter_context_(new URLFilterContext) {
  BrowserList::AddObserver(this);
}

ManagedMode::~ManagedMode() {
  // This class usually is a leaky singleton, so this destructor shouldn't be
  // called. We still do some cleanup, in case we're owned by a unit test.
  BrowserList::RemoveObserver(this);
  DCHECK_EQ(0u, callbacks_.size());
  DCHECK_EQ(0u, browsers_to_close_.size());
  url_filter_context_.release()->ShutdownOnUIThread();
}

void ManagedMode::Observe(int type,
                          const content::NotificationSource& source,
                          const content::NotificationDetails& details) {
  // Return early if we don't have any queued callbacks.
  if (callbacks_.empty())
    return;

  switch (type) {
    case chrome::NOTIFICATION_CLOSE_ALL_BROWSERS_REQUEST: {
      FinalizeEnter(false);
      return;
    }
    case chrome::NOTIFICATION_BROWSER_CLOSE_CANCELLED: {
      Browser* browser = content::Source<Browser>(source).ptr();
      if (browsers_to_close_.find(browser) != browsers_to_close_.end())
        FinalizeEnter(false);
      return;
    }
    case chrome::NOTIFICATION_EXTENSION_LOADED:
    case chrome::NOTIFICATION_EXTENSION_UNLOADED: {
      if (managed_profile_)
        UpdateWhitelist();
      break;
    }
    default:
      NOTREACHED();
  }
}

void ManagedMode::FinalizeEnter(bool result) {
  if (result)
    SetInManagedMode(managed_profile_);
  for (std::vector<EnterCallback>::iterator it = callbacks_.begin();
       it != callbacks_.end(); ++it) {
    it->Run(result);
  }
  callbacks_.clear();
  browsers_to_close_.clear();
  registrar_.RemoveAll();
}

bool ManagedMode::PlatformConfirmEnter() {
  // TODO(bauerb): Show platform-specific confirmation dialog.
  return true;
}

bool ManagedMode::PlatformConfirmLeave() {
  // TODO(bauerb): Show platform-specific confirmation dialog.
  return true;
}

void ManagedMode::SetInManagedMode(Profile* newly_managed_profile) {
  // Register the ManagementPolicy::Provider before changing the pref when
  // setting it, and unregister it after changing the pref when clearing it,
  // so pref observers see the correct ManagedMode state.
  bool in_managed_mode = !!newly_managed_profile;
  if (in_managed_mode) {
    DCHECK(!managed_profile_ || managed_profile_ == newly_managed_profile);
    extensions::ExtensionSystem::Get(
        newly_managed_profile)->management_policy()->RegisterProvider(this);
    registrar_.Add(this, chrome::NOTIFICATION_EXTENSION_LOADED,
                   content::Source<Profile>(newly_managed_profile));
    registrar_.Add(this, chrome::NOTIFICATION_EXTENSION_UNLOADED,
                   content::Source<Profile>(newly_managed_profile));
  } else {
    extensions::ExtensionSystem::Get(
        managed_profile_)->management_policy()->UnregisterProvider(this);
    registrar_.Remove(this, chrome::NOTIFICATION_EXTENSION_LOADED,
                      content::Source<Profile>(managed_profile_));
    registrar_.Remove(this, chrome::NOTIFICATION_EXTENSION_UNLOADED,
                      content::Source<Profile>(managed_profile_));
  }

  managed_profile_ = newly_managed_profile;
  url_filter_context_->SetActive(in_managed_mode);
  g_browser_process->local_state()->SetBoolean(prefs::kInManagedMode,
                                               in_managed_mode);
  if (in_managed_mode)
    UpdateWhitelist();

  // This causes the avatar and the profile menu to get updated.
  content::NotificationService::current()->Notify(
      chrome::NOTIFICATION_PROFILE_CACHED_INFO_CHANGED,
      content::NotificationService::AllBrowserContextsAndSources(),
      content::NotificationService::NoDetails());
}

void ManagedMode::UpdateWhitelist() {
  DCHECK(managed_profile_);
  // TODO(bauerb): Update URL filter with whitelist.
}
