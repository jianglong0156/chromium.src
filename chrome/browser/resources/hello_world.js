cr.define('hello_world', function() {
  'use strict';
 
  /**
   * Be polite and insert translated hello world strings for the user on loading.
   */
  function initialize() {
      //chrome.send('addNumbers', [2, 2]);
      showDialog();
    }
 
    function addResult(result) {
      alert('The result of our C++ arithmetic: 2 + 2 = ' + result);
    }

    function showDialog() {
      var dialog = document.getElementById('dialog');
       dialog.showModal();
       dialog.addEventListener('close', function (event) {
         if (dialog.returnValue == 'yes') {
           alert("升级CDT");
         } else {
           alert("不升级CDT");
         }
       });
    }
 
    return {
      addResult: addResult,
      initialize: initialize,
    };

});
 
document.addEventListener('DOMContentLoaded', hello_world.initialize);
