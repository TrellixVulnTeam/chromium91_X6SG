/* Copyright 2017 The Chromium Authors. All rights reserved.
 * Use of this source code is governed by a BSD-style license that can be
 * found in the LICENSE file. */

import {addWebUIListener, sendWithPromise} from 'chrome://resources/js/cr.m.js';
import {decorate} from 'chrome://resources/js/cr/ui.m.js';
import {TabBox} from 'chrome://resources/js/cr/ui/tabs.m.js';
import {$} from 'chrome://resources/js/util.m.js';

decorate('tabbox', TabBox);

/**
 * Asks the C++ SafeBrowsingUIHandler to get the lists of Safe Browsing
 * ongoing experiments and preferences.
 * The SafeBrowsingUIHandler should reply to addExperiment() and
 * addPreferences() (below).
 */
function initialize() {
  sendWithPromise('getExperiments', [])
      .then((experiments) => addExperiments(experiments));
  sendWithPromise('getPrefs', []).then((prefs) => addPrefs(prefs));
  sendWithPromise('getCookie', []).then((cookie) => addCookie(cookie));
  sendWithPromise('getSavedPasswords', [])
      .then((passwords) => addSavedPasswords(passwords));
  sendWithPromise('getDatabaseManagerInfo', []).then(function(databaseState) {
    const fullHashCacheState = databaseState.splice(-1, 1);
    addDatabaseManagerInfo(databaseState);
    addFullHashCacheInfo(fullHashCacheState);
  });

  sendWithPromise('getSentClientDownloadRequests', [])
      .then((sentClientDownloadRequests) => {
        sentClientDownloadRequests.forEach(function(cdr) {
          addSentClientDownloadRequestsInfo(cdr);
        });
      });
  addWebUIListener('sent-client-download-requests-update', function(result) {
    addSentClientDownloadRequestsInfo(result);
  });

  sendWithPromise('getReceivedClientDownloadResponses', [])
      .then((receivedClientDownloadResponses) => {
        receivedClientDownloadResponses.forEach(function(cdr) {
          addReceivedClientDownloadResponseInfo(cdr);
        });
      });
  addWebUIListener(
      'received-client-download-responses-update', function(result) {
        addReceivedClientDownloadResponseInfo(result);
      });

  sendWithPromise('getSentClientPhishingRequests', [])
      .then((sentClientPhishingRequests) => {
        sentClientPhishingRequests.forEach(function(cpr) {
          addSentClientPhishingRequestsInfo(cpr);
        });
      });
  addWebUIListener('sent-client-phishing-requests-update', function(result) {
    addSentClientPhishingRequestsInfo(result);
  });

  sendWithPromise('getReceivedClientPhishingResponses', [])
      .then((receivedClientPhishingResponses) => {
        receivedClientPhishingResponses.forEach(function(cpr) {
          addReceivedClientPhishingResponseInfo(cpr);
        });
      });
  addWebUIListener(
      'received-client-phishing-responses-update', function(result) {
        addReceivedClientPhishingResponseInfo(result);
      });

  sendWithPromise('getSentCSBRRs', []).then((sentCSBRRs) => {
    sentCSBRRs.forEach(function(csbrr) {
      addSentCSBRRsInfo(csbrr);
    });
  });
  addWebUIListener('sent-csbrr-update', function(result) {
    addSentCSBRRsInfo(result);
  });

  sendWithPromise('getPGEvents', []).then((pgEvents) => {
    pgEvents.forEach(function(pgEvent) {
      addPGEvent(pgEvent);
    });
  });
  addWebUIListener('sent-pg-event', function(result) {
    addPGEvent(result);
  });

  sendWithPromise('getSecurityEvents', []).then((securityEvents) => {
    securityEvents.forEach(function(securityEvent) {
      addSecurityEvent(securityEvent);
    });
  });
  addWebUIListener('sent-security-event', function(result) {
    addSecurityEvent(result);
  });

  sendWithPromise('getPGPings', []).then((pgPings) => {
    pgPings.forEach(function(pgPing) {
      addPGPing(pgPing);
    });
  });
  addWebUIListener('pg-pings-update', function(result) {
    addPGPing(result);
  });

  sendWithPromise('getPGResponses', []).then((pgResponses) => {
    pgResponses.forEach(function(pgResponse) {
      addPGResponse(pgResponse);
    });
  });
  addWebUIListener('pg-responses-update', function(result) {
    addPGResponse(result);
  });

  sendWithPromise('getRTLookupPings', []).then((rtLookupPings) => {
    rtLookupPings.forEach(function(rtLookupPing) {
      addRTLookupPing(rtLookupPing);
    });
  });
  addWebUIListener('rt-lookup-pings-update', function(result) {
    addRTLookupPing(result);
  });

  sendWithPromise('getRTLookupResponses', []).then((rtLookupResponses) => {
    rtLookupResponses.forEach(function(rtLookupResponse) {
      addRTLookupResponse(rtLookupResponse);
    });
  });
  addWebUIListener('rt-lookup-responses-update', function(result) {
    addRTLookupResponse(result);
  });

  sendWithPromise('getLogMessages', []).then((logMessages) => {
    logMessages.forEach(function(message) {
      addLogMessage(message);
    });
  });
  addWebUIListener('log-messages-update', function(message) {
    addLogMessage(message);
  });

  sendWithPromise('getReportingEvents', []).then((reportingEvents) => {
    reportingEvents.forEach(function(reportingEvent) {
      addReportingEvent(reportingEvent);
    });
  });
  addWebUIListener('reporting-events-update', function(reportingEvent) {
    addReportingEvent(reportingEvent);
  });

  sendWithPromise('getDeepScans', []).then((requests) => {
    requests.forEach(function(request) {
      addDeepScan(request);
    });
  });
  addWebUIListener('deep-scan-request-update', function(result) {
    addDeepScan(result);
  });

  $('get-referrer-chain-form').addEventListener('submit', addReferrerChain);

  // Allow tabs to be navigated to by fragment. The fragment with be of the
  // format "#tab-<tab id>"
  showTab(window.location.hash.substr(5));
  window.onhashchange = function() {
    showTab(window.location.hash.substr(5));
  };

  // When the tab updates, update the anchor
  $('tabbox').addEventListener('selectedChange', function() {
    const tabbox = $('tabbox');
    const tabs = tabbox.querySelector('tabs').children;
    const selectedTab = tabs[tabbox.selectedIndex];
    window.location.hash = 'tab-' + selectedTab.id;
  }, true);
}

function addExperiments(result) {
  const resLength = result.length;

  for (let i = 0; i < resLength; i += 2) {
    const experimentsListFormatted =
        $('result-template').content.cloneNode(true);
    experimentsListFormatted.querySelectorAll('span')[0].textContent =
        result[i + 1] + ': ';
    experimentsListFormatted.querySelectorAll('span')[1].textContent =
        result[i];
    $('experiments-list').appendChild(experimentsListFormatted);
  }
}

function addPrefs(result) {
  const resLength = result.length;

  for (let i = 0; i < resLength; i += 2) {
    const preferencesListFormatted =
        $('result-template').content.cloneNode(true);
    preferencesListFormatted.querySelectorAll('span')[0].textContent =
        result[i + 1] + ': ';
    preferencesListFormatted.querySelectorAll('span')[1].textContent =
        result[i];
    $('preferences-list').appendChild(preferencesListFormatted);
  }
}

function addCookie(result) {
  const cookieFormatted = $('cookie-template').content.cloneNode(true);
  cookieFormatted.querySelectorAll('.result')[0].textContent = result[0];
  cookieFormatted.querySelectorAll('.result')[1].textContent =
      (new Date(result[1])).toLocaleString();
  $('cookie-panel').appendChild(cookieFormatted);
}

function addSavedPasswords(result) {
  const resLength = result.length;

  for (let i = 0; i < resLength; i += 2) {
    const savedPasswordFormatted = document.createElement('div');
    const suffix = result[i + 1] ? 'GAIA password' : 'Enterprise password';
    savedPasswordFormatted.textContent = `${result[i]} (${suffix})`;
    $('saved-passwords').appendChild(savedPasswordFormatted);
  }
}

function addDatabaseManagerInfo(result) {
  const resLength = result.length;

  for (let i = 0; i < resLength; i += 2) {
    const preferencesListFormatted =
        $('result-template').content.cloneNode(true);
    preferencesListFormatted.querySelectorAll('span')[0].textContent =
        result[i] + ': ';
    const value = result[i + 1];
    if (Array.isArray(value)) {
      const blockQuote = document.createElement('blockquote');
      value.forEach(item => {
        const div = document.createElement('div');
        div.textContent = item;
        blockQuote.appendChild(div);
      });
      preferencesListFormatted.querySelectorAll('span')[1].appendChild(
          blockQuote);
    } else {
      preferencesListFormatted.querySelectorAll('span')[1].textContent = value;
    }
    $('database-info-list').appendChild(preferencesListFormatted);
  }
}

function addFullHashCacheInfo(result) {
  $('full-hash-cache-info').textContent = result;
}

function addSentClientDownloadRequestsInfo(result) {
  const logDiv = $('sent-client-download-requests-list');
  appendChildWithInnerText(logDiv, result);
}

function addReceivedClientDownloadResponseInfo(result) {
  const logDiv = $('received-client-download-response-list');
  appendChildWithInnerText(logDiv, result);
}

function addSentClientPhishingRequestsInfo(result) {
  const logDiv = $('sent-client-phishing-requests-list');
  appendChildWithInnerText(logDiv, result);
}

function addReceivedClientPhishingResponseInfo(result) {
  const logDiv = $('received-client-phishing-response-list');
  appendChildWithInnerText(logDiv, result);
}

function addSentCSBRRsInfo(result) {
  const logDiv = $('sent-csbrrs-list');
  appendChildWithInnerText(logDiv, result);
}

function addPGEvent(result) {
  const logDiv = $('pg-event-log');
  const eventFormatted = '[' + (new Date(result['time'])).toLocaleString() +
      '] ' + result['message'];
  appendChildWithInnerText(logDiv, eventFormatted);
}

function addSecurityEvent(result) {
  const logDiv = $('security-event-log');
  const eventFormatted = '[' + (new Date(result['time'])).toLocaleString() +
      '] ' + result['message'];
  appendChildWithInnerText(logDiv, eventFormatted);
}

function insertTokenToTable(tableId, token) {
  const row = $(tableId).insertRow();
  row.className = 'content';
  row.id = tableId + '-' + token;
  row.insertCell().className = 'content';
  row.insertCell().className = 'content';
}

function addResultToTable(tableId, token, result, position) {
  if ($(tableId + '-' + token) === null) {
    insertTokenToTable(tableId, token);
  }

  const cell = $(tableId + '-' + token).cells[position];
  cell.innerText = result;
}

function addPGPing(result) {
  addResultToTable('pg-ping-list', result[0], result[1], 0);
}

function addPGResponse(result) {
  addResultToTable('pg-ping-list', result[0], result[1], 1);
}

function addRTLookupPing(result) {
  addResultToTable('rt-lookup-ping-list', result[0], result[1], 0);
}

function addRTLookupResponse(result) {
  addResultToTable('rt-lookup-ping-list', result[0], result[1], 1);
}

function addDeepScan(result) {
  if (result['request_time'] != null) {
    const requestFormatted = '[' +
        (new Date(result['request_time'])).toLocaleString() + ']\n' +
        result['request'];
    addResultToTable('deep-scan-list', result['token'], requestFormatted, 0);
  }

  if (result['response_time'] != null) {
    if (result['response_status'] == 'SUCCESS') {
      // Display the response instead
      const resultFormatted = '[' +
          (new Date(result['response_time'])).toLocaleString() + ']\n' +
          result['response'];
      addResultToTable('deep-scan-list', result['token'], resultFormatted, 1);
    } else {
      // Display the error
      const resultFormatted = '[' +
          (new Date(result['response_time'])).toLocaleString() + ']\n' +
          result['response_status'];
      addResultToTable('deep-scan-list', result['token'], resultFormatted, 1);
    }
  }
}

function addLogMessage(result) {
  const logDiv = $('log-messages');
  const eventFormatted = '[' + (new Date(result['time'])).toLocaleString() +
      '] ' + result['message'];
  appendChildWithInnerText(logDiv, eventFormatted);
}

function addReportingEvent(result) {
  const logDiv = $('reporting-events');
  const eventFormatted = result['message'];
  appendChildWithInnerText(logDiv, eventFormatted);
}

function appendChildWithInnerText(logDiv, text) {
  if (!logDiv) {
    return;
  }
  const textDiv = document.createElement('div');
  textDiv.innerText = text;
  logDiv.appendChild(textDiv);
}

function addReferrerChain(ev) {
  // Don't navigate
  ev.preventDefault();

  sendWithPromise('getReferrerChain', $('referrer-chain-url').value)
      .then((response) => {
        $('referrer-chain-content').innerHTML = trustedTypes.emptyHTML;
        $('referrer-chain-content').textContent = response;
      });
}

function showTab(tabId) {
  if ($(tabId)) {
    $(tabId).selected = 'selected';
  }
}

document.addEventListener('DOMContentLoaded', initialize);
