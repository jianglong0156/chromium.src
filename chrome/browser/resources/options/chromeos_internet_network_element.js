// Copyright (c) 2010 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

cr.define('options.internet', function() {
  /**
   * Creates a new network list div.
   * @param {Object=} opt_propertyBag Optional properties.
   * @constructor
   * @extends {HTMLDivElement}
   */
  var NetworkElement = cr.ui.define('div');

  NetworkElement.prototype = {
    __proto__: HTMLDivElement.prototype,

    /** @inheritDoc */
    decorate: function() {
      this.addEventListener('click', this.handleClick_);
    },

    /**
     * Loads given network list.
     * @param {Array} networks An array of network object.
     */
    load: function(networks) {
      this.textContent = '';

      for (var i = 0; i < networks.length; ++i) {
        this.appendChild(new NetworkItem(networks[i]));
      }
    },

    /**
     * Handles click on network list and triggers actions when clicked on
     * a NetworkListItem button.
     * @private
     * @param {!Event} e The click event object.
     */
    handleClick_: function(e) {
      // We shouldn't respond to click events selecting an input,
      // so return on those.
      if (e.target.tagName == 'INPUT') {
        return;
      }
      // Handle left button click
      if (e.button == 0) {
        var el = e.target;
        // If click is on action buttons of a network item.
        if (el.buttonType && el.networkType && el.servicePath) {
          chrome.send('buttonClickCallback',
              [String(el.networkType), el.servicePath, el.buttonType]);
        } else {
          if (el.className == 'other-network' || el.buttonType) {
            return;
          }
          // If click is on a network item or its label, walk up the DOM tree
          // to find the network item.
          var item = el;
          while (item && !item.data) {
            item = item.parentNode;
          }
          if (item.connecting)
            return;

          if (item) {
            var data = item.data;
            // Don't try to connect to Ethernet.
            if (data && data.networkType == 1)
              return;
            for (var i = 0; i < this.childNodes.length; i++) {
              if (this.childNodes[i] != item)
                this.childNodes[i].hidePassword();
            }
            // If clicked on other networks item.
            if (data && data.servicePath == '?') {
              item.showOtherLogin();
            } else if (data) {
                if (!data.connecting && !data.connected) {
                  chrome.send('buttonClickCallback',
                              [String(data.networkType),
                               data.servicePath,
                               'connect']);
                }
            }
          }
        }
      }
    }
  };

  /**
   * Creates a new network item.
   * @param {Object} network The network this represents.
   * @constructor
   * @extends {HTMLDivElement}
   */
  function NetworkItem(network) {
    var el = cr.doc.createElement('div');
    el.data = {
      servicePath: network[0],
      networkName: network[1],
      networkStatus: network[2],
      networkType: network[3],
      connected: network[4],
      connecting: network[5],
      iconURL: network[6],
      remembered: network[7],
      activation_state: network[8]
    };
    NetworkItem.decorate(el);
    return el;
  }


  /**
   * Minimum length for wireless network password.
   * @type {number}
   */
  NetworkItem.MIN_WIRELESS_PASSWORD_LENGTH = 5;
  NetworkItem.MIN_WIRELESS_SSID_LENGTH = 1;
  // Cellular activation states:
  NetworkItem.ACTIVATION_STATE_UNKNOWN             = 0;
  NetworkItem.ACTIVATION_STATE_ACTIVATED           = 1;
  NetworkItem.ACTIVATION_STATE_ACTIVATING          = 2;
  NetworkItem.ACTIVATION_STATE_NOT_ACTIVATED       = 3;
  NetworkItem.ACTIVATION_STATE_PARTIALLY_ACTIVATED = 4;
  NetworkItem.TYPE_UNKNOWN   = 0;
  NetworkItem.TYPE_ETHERNET  = 1;
  NetworkItem.TYPE_WIFI      = 2;
  NetworkItem.TYPE_WIMAX     = 3;
  NetworkItem.TYPE_BLUETOOTH = 4;
  NetworkItem.TYPE_CELLULAR  = 5;

  /**
   * Decorates an element as a network item.
   * @param {!HTMLElement} el The element to decorate.
   */
  NetworkItem.decorate = function(el) {
    el.__proto__ = NetworkItem.prototype;
    el.decorate();
  };

  NetworkItem.prototype = {
    __proto__: HTMLDivElement.prototype,

    /** @inheritDoc */
    decorate: function() {
      var isOtherNetworksItem = this.data.servicePath == '?';

      this.className = 'network-item';
      this.connected = this.data.connected;
      this.id = this.data.servicePath;
      // textDiv holds icon, name and status text.
      var textDiv = this.ownerDocument.createElement('div');
      textDiv.className = 'network-item-text';
      if (this.data.iconURL) {
        textDiv.style.backgroundImage = url(this.data.iconURL);
      }

      var nameEl = this.ownerDocument.createElement('div');
      nameEl.className = 'network-name-label';
      nameEl.textContent = this.data.networkName;
      textDiv.appendChild(nameEl);

      if (isOtherNetworksItem) {
        // No status and buttons for "Other..."
        this.appendChild(textDiv);
        return;
      }

      // Only show status text for networks other than "remembered".
      if (!this.data.remembered) {
        var statusEl = this.ownerDocument.createElement('div');
        statusEl.className = 'network-status-label';
        statusEl.textContent = this.data.networkStatus;
        textDiv.appendChild(statusEl);
      }

      this.appendChild(textDiv);

      var spacerDiv = this.ownerDocument.createElement('div');
      spacerDiv.className = 'network-item-box-spacer';
      this.appendChild(spacerDiv);

      var buttonsDiv = this.ownerDocument.createElement('div');
      var self = this;
      if (!this.data.remembered) {
        var show_activate =
          this.data.networkType == NetworkItem.TYPE_CELLULAR &&
          this.data.activation_state !=
              NetworkItem.ACTIVATION_STATE_ACTIVATED &&
          this.data.activation_state !=
              NetworkItem.ACTIVATION_STATE_ACTIVATING;

        // Disconnect button if not ethernet and if cellular it should be
        // activated.
        if (this.data.networkType != NetworkItem.TYPE_ETHERNET &&
            !show_activate && this.data.connected) {
          buttonsDiv.appendChild(
              this.createButton_('disconnect_button',
                                  function(e) {
                 chrome.send('buttonClickCallback',
                             [String(self.data.networkType),
                              self.data.servicePath,
                              'disconnect']);
               }));
        }
        // Show [Activate] button for non-activated Cellular network.
        if (show_activate) {
          buttonsDiv.appendChild(
              this.createButton_('activate_button',
                                 function(e) {
                chrome.send('buttonClickCallback',
                            [String(self.data.networkType),
                             self.data.servicePath,
                             'activate']);
              }));
        }
        if (this.data.connected) {
          buttonsDiv.appendChild(
              this.createButton_('options_button',
                                 function(e) {
                chrome.send('buttonClickCallback',
                            [String(self.data.networkType),
                             self.data.servicePath,
                             'options']);
              }));
        }
      } else {
        // Put "Forget this network" button.
        var button = this.createButton_('forget_button',
                                        function(e) {
                       chrome.send('buttonClickCallback',
                                   [String(self.data.networkType),
                                   self.data.servicePath,
                                   'forget']);
                     });
        if (cr.commandLine.options['--bwsi']) {
          // Disable this for guest session(bwsi).
          button.disabled = true;
        }

        buttonsDiv.appendChild(button);
      }
      this.appendChild(buttonsDiv);
    },

    showPassword: function(password) {
      if (this.connecting)
        return;
      var passwordDiv = this.ownerDocument.createElement('div');
      passwordDiv.className = 'network-password';
      var passInput = this.ownerDocument.createElement('input');
      passwordDiv.appendChild(passInput);
      passInput.placeholder = localStrings.getString('inetPassPrompt');
      passInput.type = 'password';
      var buttonEl = this.ownerDocument.createElement('button');
      buttonEl.textContent = localStrings.getString('inetLogin');
      buttonEl.addEventListener('click', this.handleLogin_);
      buttonEl.servicePath = this.data.servicePath;
      buttonEl.style.right = '0';
      buttonEl.style.position = 'absolute';
      buttonEl.style.visibility = 'visible';
      buttonEl.disabled = true;

      var togglePassLabel = this.ownerDocument.createElement('label');
      togglePassLabel.style.display = 'inline';
      var togglePassSpan = this.ownerDocument.createElement('span');
      var togglePassCheckbox = this.ownerDocument.createElement('input');
      togglePassCheckbox.type = 'checkbox';
      togglePassCheckbox.checked = false;
      togglePassCheckbox.target = passInput;
      togglePassCheckbox.addEventListener('change', this.handleShowPass_);
      togglePassSpan.textContent = localStrings.getString('inetShowPass');
      togglePassLabel.appendChild(togglePassCheckbox);
      togglePassLabel.appendChild(togglePassSpan);
      passwordDiv.appendChild(togglePassLabel);

      // Disable login button if there is no password.
      passInput.addEventListener('keyup', function(e) {
        buttonEl.disabled =
          passInput.value.length < NetworkItem.MIN_WIRELESS_PASSWORD_LENGTH;
      });

      passwordDiv.appendChild(buttonEl);
      this.connecting = true;
      this.appendChild(passwordDiv);
    },

    handleShowPass_: function(e) {
      var target = e.target;
      if (target.checked) {
        target.target.type = 'text';
      } else {
        target.target.type = 'password';
      }
    },

    hidePassword: function() {
      this.connecting = false;
      var children = this.childNodes;
      for (var i = 0; i < children.length; i++) {
        if (children[i].className == 'network-password' ||
            children[i].className == 'other-network') {
          this.removeChild(children[i]);
          return;
        }
      }
    },

    showOtherLogin: function() {
      if (this.connecting)
        return;
      var passwordDiv = this.ownerDocument.createElement('div');
      passwordDiv.className = 'other-network';
      var ssidInput = this.ownerDocument.createElement('input');
      ssidInput.placeholder = localStrings.getString('inetSsidPrompt');
      passwordDiv.appendChild(ssidInput);
      var passInput = this.ownerDocument.createElement('input');
      passInput.placeholder = localStrings.getString('inetPassPrompt');
      passwordDiv.appendChild(passInput);
      var buttonEl = this.ownerDocument.createElement('button');
      buttonEl.textContent = localStrings.getString('inetLogin');
      buttonEl.buttonType = true;
      buttonEl.addEventListener('click', this.handleOtherLogin_);
      buttonEl.style.visibility = 'visible';
      passwordDiv.appendChild(buttonEl);
      this.appendChild(passwordDiv);

      ssidInput.addEventListener('keydown', function(e) {
        buttonEl.disabled =
          ssidInput.value.length < NetworkItem.MIN_WIRELESS_SSID_LENGTH;
      });
      this.connecting = true;
    },

    handleLogin_: function(e) {
      var el = e.target;
      var parent = el.parentNode;
      el.disabled = true;
      var input = parent.firstChild;
      input.disabled = true;
      chrome.send('loginToNetwork', [el.servicePath, input.value]);
    },

    handleOtherLogin_: function(e) {
      var el = e.target;
      var parent = el.parentNode;
      el.disabled = true;
      var ssid = parent.childNodes[0];
      var pass = parent.childNodes[1];
      ssid.disabled = true;
      pass.disabled = true;
      chrome.send('loginToNetwork', [ssid.value, pass.value]);
    },

    /**
     * Creates a button for interacting with a network.
     * @param {Object} name The name of the localStrings to use for the text.
     * @param {Object} type The type of button.
     */
    createButton_: function(name, callback) {
      var buttonEl = this.ownerDocument.createElement('button');
      buttonEl.textContent = localStrings.getString(name);
      buttonEl.addEventListener('click', callback);
      return buttonEl;
    }
  };

  /**
   * Whether the underlying network is connected. Only used for display purpose.
   * @type {boolean}
   */
  cr.defineProperty(NetworkItem, 'connected', cr.PropertyKind.BOOL_ATTR);

  /**
   * Whether the underlying network is currently connecting.
   * Only used for display purpose.
   * @type {boolean}
   */
  cr.defineProperty(NetworkItem, 'connecting', cr.PropertyKind.BOOL_ATTR);

  return {
    NetworkElement: NetworkElement
  };
});
