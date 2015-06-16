cr.define('hello_world', function() {
  'use strict';
 
  /**
   * Be polite and insert translated hello world strings for the user on loading.
   */
  function initialize() {
      chrome.send('addNumbers', [2, 2]);
    }
 
    function addResult(result) {
      alert('The result of our C++ arithmetic: 2 + 2 = ' + result);
    }
 
    return {
      addResult: addResult,
      initialize: initialize,
    };

});
 
document.addEventListener('DOMContentLoaded', hello_world.initialize);
