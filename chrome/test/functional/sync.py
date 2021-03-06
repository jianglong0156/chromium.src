#!/usr/bin/env python
# Copyright (c) 2012 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

import pyauto_functional  # Must be imported before pyauto
import pyauto
import test_utils


class SyncTest(pyauto.PyUITest):
  """Tests for sync."""

  def testSignInToSync(self):
    """Sign in to sync."""
    new_timeout = pyauto.PyUITest.ActionTimeoutChanger(self,
                                                       60 * 1000)  # 1 min.
    test_utils.SignInToSyncAndVerifyState(self, 'test_google_account')

  def testDisableAndEnableDatatypes(self):
    """Sign in, disable and then enable sync for multiple sync datatypes."""
    new_timeout = pyauto.PyUITest.ActionTimeoutChanger(self,
                                                       2 * 60 * 1000)  # 2 min.
    test_utils.SignInToSyncAndVerifyState(self, 'test_google_account')
    self.assertTrue(self.DisableSyncForDatatypes(['Apps', 'Autofill',
        'Bookmarks', 'Extensions', 'Preferences', 'Themes']))
    self.assertFalse('Apps' in self.GetSyncInfo()['synced datatypes'])
    self.assertFalse('Autofill' in self.GetSyncInfo()['synced datatypes'])
    self.assertFalse('Bookmarks' in self.GetSyncInfo()['synced datatypes'])
    self.assertFalse('Extensions' in self.GetSyncInfo()['synced datatypes'])
    self.assertFalse('Preferences' in self.GetSyncInfo()['synced datatypes'])
    self.assertFalse('Themes' in self.GetSyncInfo()['synced datatypes'])
    self.assertTrue(self.EnableSyncForDatatypes(['Apps', 'Autofill',
        'Bookmarks', 'Extensions', 'Preferences','Themes']))
    self.assertTrue(self.DisableSyncForDatatypes(['Passwords']))
    self.assertTrue('Apps' in self.GetSyncInfo()['synced datatypes'])
    self.assertTrue('Autofill' in self.GetSyncInfo()['synced datatypes'])
    self.assertTrue('Bookmarks' in self.GetSyncInfo()['synced datatypes'])
    self.assertTrue('Extensions' in self.GetSyncInfo()['synced datatypes'])
    self.assertTrue('Preferences' in self.GetSyncInfo()['synced datatypes'])
    self.assertTrue('Themes' in self.GetSyncInfo()['synced datatypes'])
    self.assertFalse('Passwords' in self.GetSyncInfo()['synced datatypes'])

  def testRestartBrowser(self):
    """Sign in to sync and restart the browser."""
    new_timeout = pyauto.PyUITest.ActionTimeoutChanger(self,
                                                       2 * 60 * 1000)  # 2 min.
    test_utils.SignInToSyncAndVerifyState(self, 'test_google_account')
    self.RestartBrowser(clear_profile=False)
    self.assertTrue(self.AwaitSyncRestart())
    self.assertTrue(self.GetSyncInfo()['last synced'] == 'Just now')
    self.assertTrue(self.GetSyncInfo()['updates received'] == 0)

  def testPersonalStuffSyncSection(self):
    """Verify the Sync section in Preferences before and after sync."""
    creds = self.GetPrivateInfo()['test_google_account']
    username = creds['username']
    password = creds['password']
    default_text = 'Keep everything synced or choose what data to sync'
    set_up_button = 'Set Up Sync'
    customize_button = 'Customize'
    stop_button = 'Stop Sync'
    signed_in_text = 'Google Dashboard'
    chrome_settings_url = 'chrome://settings-frame'
    new_timeout = pyauto.PyUITest.ActionTimeoutChanger(self,
                                                       2 * 60 * 1000)  # 2 min.
    self.AppendTab(pyauto.GURL(chrome_settings_url))
    self.assertTrue(self.WaitUntil(
        lambda: self.FindInPage(default_text, tab_index=1)['match_count'],
                expect_retval=1),
        'No default sync text.')
    self.assertTrue(self.WaitUntil(
        lambda: self.FindInPage(set_up_button, tab_index=1)['match_count'],
                expect_retval=1),
        'No set up sync button.')

    self.assertTrue(self.SignInToSync(username, password))
    self.ReloadTab(1)
    self.assertTrue(self.WaitUntil(
        lambda: self.FindInPage(username, tab_index=1)['match_count'],
                expect_retval=1),
        'No sync user account information.')
    self.assertTrue(self.WaitUntil(
        lambda: self.FindInPage(signed_in_text, tab_index=1)['match_count'],
                expect_retval=1),
        'No Google Dashboard information after signing in.')
    self.assertTrue(self.WaitUntil(
        lambda: self.FindInPage(stop_button, tab_index=1)['match_count'],
                expect_retval=1),
        'No stop sync button.')
    self.assertTrue(self.WaitUntil(
        lambda: self.FindInPage(customize_button, tab_index=1)['match_count'],
                expect_retval=1),
        'No customize sync button.')


class SyncIntegrationTest(pyauto.PyUITest):
  """Test integration between sync and other components."""

  def ExtraChromeFlags(self):
    """Prepares the browser to launch with the specified extra Chrome flags.

    |ChromeFlagsForTestServer()| is invoked to create the flags list.
    """
    return pyauto.PyUITest.ExtraChromeFlags(self) + \
        self.ChromeFlagsForSyncTestServer(**self._sync_server.ports)

  def setUp(self):
    # LaunchPythonSyncServer() executes before pyauto.PyUITest.setUp() because
    # the latter invokes ExtraChromeFlags() which requires the server's ports.
    self._sync_server = self.StartSyncServer()
    pyauto.PyUITest.setUp(self)

  def tearDown(self):
    pyauto.PyUITest.tearDown(self)
    self.StopSyncServer(self._sync_server)

  def testAddBookmarkAndVerifySync(self):
    """Verify a bookmark syncs between two browsers.

    Integration tests between the bookmarks and sync features. A bookmark is
    added to one instance of the browser, the bookmark is synced to the account,
    a new instance of the browser is launched, the account is synced and the
    bookmark info is synced on the new browser.
    """
    # Launch a new instance of the browser with a clean profile (Browser 2)
    browser2 = pyauto.ExtraBrowser(
        self.ChromeFlagsForSyncTestServer(**self._sync_server.ports))

    account_key = 'test_sync_account'
    test_utils.SignInToSyncAndVerifyState(self, account_key)
    self.AwaitSyncCycleCompletion()

    # Add a bookmark.
    bookmarks = self.GetBookmarkModel()
    bar_id = bookmarks.BookmarkBar()['id']
    name = 'Column test'
    url = self.GetHttpURLForDataPath('columns.html')
    self.NavigateToURL(url)
    self.AddBookmarkURL(bar_id, 0, name, url)

    # Refresh the bookmarks in the first browser.
    bookmarks = self.GetBookmarkModel()

    # Log into the account and sync the browser to the account.
    test_utils.SignInToSyncAndVerifyState(browser2, account_key)
    browser2.AwaitSyncCycleCompletion()

    # Verify browser 2 contains the bookmark.
    browser2_bookmarks = browser2.GetBookmarkModel()
    self.assertEqual(browser2_bookmarks.NodeCount(), bookmarks.NodeCount())
    bar_child = browser2_bookmarks.BookmarkBar()['children'][0]
    self.assertEqual(bar_child['type'], 'url')
    self.assertEqual(bar_child['name'], name)
    self.assertTrue(url in bar_child['url'])

if __name__ == '__main__':
  pyauto_functional.Main()
