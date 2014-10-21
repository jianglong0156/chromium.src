// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_CHROMEOS_LOGIN_SUPERVISED_SUPERVISED_USER_CREATION_SCREEN_H_
#define CHROME_BROWSER_CHROMEOS_LOGIN_SUPERVISED_SUPERVISED_USER_CREATION_SCREEN_H_

#include <string>

#include "base/basictypes.h"
#include "base/compiler_specific.h"
#include "chrome/browser/chromeos/camera_presence_notifier.h"
#include "chrome/browser/chromeos/login/screens/base_screen.h"
#include "chrome/browser/chromeos/login/supervised/supervised_user_creation_controller.h"
#include "chrome/browser/image_decoder.h"
#include "chrome/browser/supervised_user/supervised_user_sync_service.h"
#include "chrome/browser/ui/webui/chromeos/login/supervised_user_creation_screen_handler.h"
#include "chromeos/network/portal_detector/network_portal_detector.h"
#include "ui/gfx/image/image_skia.h"

class Profile;

namespace chromeos {

class ErrorScreensHistogramHelper;
class NetworkState;
class ScreenManager;

// Class that controls screen showing ui for supervised user creation.
class SupervisedUserCreationScreen
    : public BaseScreen,
      public SupervisedUserCreationScreenHandler::Delegate,
      public SupervisedUserCreationController::StatusConsumer,
      public SupervisedUserSyncServiceObserver,
      public ImageDecoder::Delegate,
      public NetworkPortalDetector::Observer,
      public CameraPresenceNotifier::Observer {
 public:
  SupervisedUserCreationScreen(
      ScreenObserver* observer,
      SupervisedUserCreationScreenHandler* actor);
  virtual ~SupervisedUserCreationScreen();

  static SupervisedUserCreationScreen* Get(ScreenManager* manager);

  // Makes screen to show message about inconsistency in manager login flow
  // (e.g. password change detected, invalid OAuth token, etc).
  // Called when manager user is successfully authenticated, so ui elements
  // should result in forced logout.
  void ShowManagerInconsistentStateErrorScreen();

  // Called when authentication fails for manager with provided password.
  // Displays wrong password message on manager selection screen.
  void OnManagerLoginFailure();

  // Called when manager is successfully authenticated and account is in
  // consistent state.
  void OnManagerFullyAuthenticated(Profile* manager_profile);

  // Called when manager is successfully authenticated against cryptohome, but
  // OAUTH token validation hasn't completed yet.
  // Results in spinner indicating that creation is in process.
  void OnManagerCryptohomeAuthenticated();

  // Shows initial screen where managed user name/password are defined and
  // manager is selected.
  void ShowInitialScreen();

  // CameraPresenceNotifier::Observer implementation:
  virtual void OnCameraPresenceCheckDone(bool is_camera_present) override;

  // SupervisedUserSyncServiceObserver implementation
  virtual void OnSupervisedUserAcknowledged(
      const std::string& supervised_user_id) override {}
  virtual void OnSupervisedUsersSyncingStopped() override {}
  virtual void OnSupervisedUsersChanged() override;

  // BaseScreen implementation:
  virtual void PrepareToShow() override;
  virtual void Show() override;
  virtual void Hide() override;
  virtual std::string GetName() const override;

  // SupervisedUserCreationScreenHandler::Delegate implementation:
  virtual void OnActorDestroyed(SupervisedUserCreationScreenHandler* actor)
      override;
  virtual void CreateSupervisedUser(
      const base::string16& display_name,
      const std::string& supervised_user_password) override;
  virtual void ImportSupervisedUser(const std::string& user_id) override;
  virtual void ImportSupervisedUserWithPassword(
      const std::string& user_id,
      const std::string& password) override;
  virtual void AuthenticateManager(
      const std::string& manager_id,
      const std::string& manager_password) override;
  virtual void AbortFlow() override;
  virtual void FinishFlow() override;
  virtual bool FindUserByDisplayName(const base::string16& display_name,
                                     std::string *out_id) const override;
  virtual void OnPageSelected(const std::string& page) override;

  // SupervisedUserController::StatusConsumer overrides.
  virtual void OnCreationError(SupervisedUserCreationController::ErrorCode code)
      override;
  virtual void OnCreationTimeout() override;
  virtual void OnCreationSuccess() override;
  virtual void OnLongCreationWarning() override;

  // NetworkPortalDetector::Observer implementation:
  virtual void OnPortalDetectionCompleted(
          const NetworkState* network,
          const NetworkPortalDetector::CaptivePortalState& state) override;

  // TODO(antrim) : this is an explicit code duplications with UserImageScreen.
  // It should be removed by issue 251179.

  // SupervisedUserCreationScreenHandler::Delegate (image) implementation:
  virtual void OnPhotoTaken(const std::string& raw_data) override;
  virtual void OnImageSelected(const std::string& image_url,
                               const std::string& image_type) override;
  virtual void OnImageAccepted() override;
  // ImageDecoder::Delegate overrides:
  virtual void OnImageDecoded(const ImageDecoder* decoder,
                              const SkBitmap& decoded_image) override;
  virtual void OnDecodeImageFailed(const ImageDecoder* decoder) override;

 private:
  void ApplyPicture();
  void OnGetSupervisedUsers(const base::DictionaryValue* users);

  SupervisedUserCreationScreenHandler* actor_;

  scoped_ptr<SupervisedUserCreationController> controller_;
  scoped_ptr<base::DictionaryValue> existing_users_;

  bool on_error_screen_;
  bool manager_signin_in_progress_;
  std::string last_page_;

  SupervisedUserSyncService* sync_service_;

  gfx::ImageSkia user_photo_;
  scoped_refptr<ImageDecoder> image_decoder_;
  bool apply_photo_after_decoding_;
  int selected_image_;

  scoped_ptr<ErrorScreensHistogramHelper> histogram_helper_;

  base::WeakPtrFactory<SupervisedUserCreationScreen> weak_factory_;

  DISALLOW_COPY_AND_ASSIGN(SupervisedUserCreationScreen);
};

}  // namespace chromeos

#endif  // CHROME_BROWSER_CHROMEOS_LOGIN_SUPERVISED_SUPERVISED_USER_CREATION_SCREEN_H_

