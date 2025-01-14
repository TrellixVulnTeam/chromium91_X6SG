// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

const {assert} = chai;

import * as SDK from '../../../../front_end/core/sdk/sdk.js';
import type * as BrowserSDKModule from '../../../../front_end/browser_sdk/browser_sdk.js';

import {describeWithEnvironment} from '../helpers/EnvironmentHelpers.js';
import {StubIssue, ThirdPartyStubIssue} from '../sdk/StubIssue.js';
import {MockIssuesModel} from '../sdk/MockIssuesModel.js';

describeWithEnvironment('IssuesManager', () => {
  let BrowserSDK: typeof BrowserSDKModule;
  before(async () => {
    BrowserSDK = await import('../../../../front_end/browser_sdk/browser_sdk.js');
  });

  it('collects issues from an issues model', () => {
    const issue1 = new StubIssue('StubIssue1', ['id1', 'id2'], []);
    const issue2 = new StubIssue('StubIssue2', ['id1', 'id2'], []);
    const issue2b = new StubIssue('StubIssue2', ['id1', 'id2'], ['id3']);

    const mockModel = new MockIssuesModel([issue1]) as unknown as SDK.IssuesModel.IssuesModel;
    const issuesManager = new BrowserSDK.IssuesManager.IssuesManager();
    issuesManager.modelAdded(mockModel);

    const dispatchedIssues: SDK.Issue.Issue[] = [];
    issuesManager.addEventListener(
        BrowserSDK.IssuesManager.Events.IssueAdded, event => dispatchedIssues.push(event.data.issue));

    mockModel.dispatchEventToListeners(SDK.IssuesModel.Events.IssueAdded, {issuesModel: mockModel, issue: issue2});
    mockModel.dispatchEventToListeners(SDK.IssuesModel.Events.IssueAdded, {issuesModel: mockModel, issue: issue2b});

    assert.deepStrictEqual(dispatchedIssues.map(i => i.code()), ['StubIssue2', 'StubIssue2']);

    // The `issue1` should not be present, as it was present before the IssuesManager
    // was instantiated.
    const issueCodes = Array.from(issuesManager.issues()).map(r => r.code());
    assert.deepStrictEqual(issueCodes, ['StubIssue2', 'StubIssue2']);
  });

  it('filters third-party issues when the third-party issues setting is false, includes them otherwise', () => {
    const issues = [
      new ThirdPartyStubIssue('AllowedStubIssue1', false),
      new ThirdPartyStubIssue('StubIssue2', true),
      new ThirdPartyStubIssue('AllowedStubIssue3', false),
      new ThirdPartyStubIssue('StubIssue4', true),
    ];

    SDK.Issue.getShowThirdPartyIssuesSetting().set(false);

    const issuesManager = new BrowserSDK.IssuesManager.IssuesManager();
    const mockModel = new MockIssuesModel([]) as unknown as SDK.IssuesModel.IssuesModel;
    issuesManager.modelAdded(mockModel);

    const firedIssueAddedEventCodes: string[] = [];
    issuesManager.addEventListener(
        BrowserSDK.IssuesManager.Events.IssueAdded, event => firedIssueAddedEventCodes.push(event.data.issue.code()));

    for (const issue of issues) {
      mockModel.dispatchEventToListeners(SDK.IssuesModel.Events.IssueAdded, {issuesModel: mockModel, issue: issue});
    }

    let issueCodes = Array.from(issuesManager.issues()).map(i => i.code());
    assert.deepStrictEqual(issueCodes, ['AllowedStubIssue1', 'AllowedStubIssue3']);
    assert.deepStrictEqual(firedIssueAddedEventCodes, ['AllowedStubIssue1', 'AllowedStubIssue3']);

    SDK.Issue.getShowThirdPartyIssuesSetting().set(true);

    issueCodes = Array.from(issuesManager.issues()).map(i => i.code());
    assert.deepStrictEqual(issueCodes, ['AllowedStubIssue1', 'StubIssue2', 'AllowedStubIssue3', 'StubIssue4']);
  });

  it('reports issue counts by kind', () => {
    const issue1 = new StubIssue('StubIssue1', ['id1'], [], SDK.Issue.IssueKind.Improvement);
    const issue2 = new StubIssue('StubIssue1', ['id2'], [], SDK.Issue.IssueKind.Improvement);
    const issue3 = new StubIssue('StubIssue1', ['id3'], [], SDK.Issue.IssueKind.BreakingChange);

    const mockModel = new MockIssuesModel([issue1]) as unknown as SDK.IssuesModel.IssuesModel;
    const issuesManager = new BrowserSDK.IssuesManager.IssuesManager();
    issuesManager.modelAdded(mockModel);

    mockModel.dispatchEventToListeners(SDK.IssuesModel.Events.IssueAdded, {issuesModel: mockModel, issue: issue1});
    mockModel.dispatchEventToListeners(SDK.IssuesModel.Events.IssueAdded, {issuesModel: mockModel, issue: issue2});
    mockModel.dispatchEventToListeners(SDK.IssuesModel.Events.IssueAdded, {issuesModel: mockModel, issue: issue3});

    assert.deepStrictEqual(issuesManager.numberOfIssues(), 3);
    assert.deepStrictEqual(issuesManager.numberOfIssues(SDK.Issue.IssueKind.Improvement), 2);
    assert.deepStrictEqual(issuesManager.numberOfIssues(SDK.Issue.IssueKind.BreakingChange), 1);
    assert.deepStrictEqual(issuesManager.numberOfIssues(SDK.Issue.IssueKind.PageError), 0);
  });
});
