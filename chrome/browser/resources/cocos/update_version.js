cr.define('update_version', function() {
  'use strict';
 
  /**
   * Be polite and insert translated hello world strings for the user on loading.
   */
  function initialize() {
    $('update-version-message').textContent = loadTimeData.getStringF('updateVersionMessage',
        loadTimeData.getString('userName'));
    chrome.send('addNumbers', [2, 2]);
  }


  function addResult(result) {
    alert('The result of our C++ arithmetic: 2 + 2 = ' + result);
  }
 
  // Return an object with all of the exports.
  return {
    addResult: addResult,
    initialize: initialize,
  };

});
 
document.addEventListener('DOMContentLoaded', update_version.initialize);
