cr.define('hello_world', function() {
  'use strict';
 
    /**
     * Be polite and insert translated hello world strings for the user on loading.
     */
    function initialize() {
      chrome.send('showUpdateDialog');
    }
 
    function showUpdateDialog(title, content, yesStr, noStr) {
       var dialog = document.getElementById('dialog');
       var dialogTitle = document.getElementById('dialogTitle');
       var dialogYes = document.getElementById('dialogYes');
       var dialogNo = document.getElementById('dialogNo');

       dialogTitle

       dialog.showModal();
       dialog.addEventListener('close', function (event) {
         if (dialog.returnValue == 'yes') {
            windows.open("localhost:3000/Release20150520.zip"):
           //alert("升级CDT");
         } else {
           alert("不升级CDT");
         }
       });
    }

    return {
      showUpdateDialog: showUpdateDialog,
      initialize: initialize,
    };

});
 
document.addEventListener('DOMContentLoaded', hello_world.initialize);
