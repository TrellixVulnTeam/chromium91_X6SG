// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef UI_ACCESSIBILITY_PLATFORM_INSPECT_AX_INSPECT_SCENARIO_H_
#define UI_ACCESSIBILITY_PLATFORM_INSPECT_AX_INSPECT_SCENARIO_H_

#include <string>
#include <vector>

#include "ui/accessibility/ax_export.h"

namespace ui {

struct AXPropertyFilter;
struct AXNodeFilter;

// Describes the test execution flow, which is parsed from a sequence
// of testing directives (instructions). The testing directives are typically
// found in a testing file in the comment section. For example, such section
// in a dump_tree HTML test file will instruct to wait for 'bananas' text in
// a document and then dump an accessible tree which includes aria-live property
// on all platforms:
// <!--
// @WAIT-FOR:bananas
// @MAC-ALLOW:AXARIALive
// @WIN-ALLOW:live*
// @UIA-WIN-ALLOW:LiveSetting*
// @BLINK-ALLOW:live*
// @BLINK-ALLOW:container*
// @AURALINUX-ALLOW:live*
// -->
class AX_EXPORT AXInspectScenario {
 public:
  explicit AXInspectScenario(
      const std::vector<AXPropertyFilter>& default_filters = {});
  AXInspectScenario(AXInspectScenario&&);
  ~AXInspectScenario();

  AXInspectScenario& operator=(AXInspectScenario&&);

  // Parses a given testing scenario.
  // @directive_prefix  platform dependent directive prefix, for example,
  //                    @MAC- is used for filter directives on Mac
  // @lines             lines containing directives as a text
  // @default_filters   set of default filters, a special type of directives,
  //                    defining which property gets (or not) into the output,
  //                    useful to not make each test to specify common filters
  //                    all over
  static AXInspectScenario From(
      const std::string& directive_prefix,
      const std::vector<std::string>& lines,
      const std::vector<AXPropertyFilter>& default_filters = {});

  // A list of URLs of resources that are never expected to load. For example,
  // a broken image url, which otherwise would make a test failing.
  std::vector<std::string> no_load_expected;

  // A list of strings must be present in the formatted tree before the test
  // starts
  std::vector<std::string> wait_for;

  // A list of string indicating an element the default accessible action
  // should be performed at before the test starts.
  std::vector<std::string> default_action_on;

  // A list of JavaScripts functions to be executed consequently. Function
  // may return a value, which has to be present in a formatter tree before
  // the next function evaluated.
  std::vector<std::string> execute;

  // A list of strings indicating that event recording should be terminated
  // when one of them is present in a formatted tree.
  std::vector<std::string> run_until;

  // A list of property filters which defines generated output of a formatted
  // tree.
  std::vector<AXPropertyFilter> property_filters;

  // The node filters indicating subtrees that should be not included into
  // a formatted tree.
  std::vector<AXNodeFilter> node_filters;

 private:
  enum Directive {
    // No directive.
    kNone,

    // Instructs to not wait for document load for url defined by the
    // directive.
    kNoLoadExpected,

    // Delays a test unitl a string defined by the directive is present
    // in the dump.
    kWaitFor,

    // Delays a test until a string returned by a script defined by the
    // directive is present in the dump.
    kExecuteAndWaitFor,

    // Indicates event recording should continue at least until a specific
    // event has been received.
    kRunUntil,

    // Invokes default action on an accessible object defined by the
    // directive.
    kDefaultActionOn,

    // Property filter directives, see AXPropertyFilter.
    kPropertyFilterAllow,
    kPropertyFilterAllowEmpty,
    kPropertyFilterDeny,

    // Scripting instruction.
    kScript,

    // Node filter directives, see AXNodeFilter.
    kNodeFilter,
  };

  // Parses directives from the given line.
  static Directive ParseDirective(const std::string& directive_prefix,
                                  const std::string& directive);

  // Adds a given directive into a scenario.
  void ProcessDirective(Directive directive, const std::string& value);
};

}  // namespace ui

#endif  // UI_ACCESSIBILITY_PLATFORM_INSPECT_AX_INSPECT_SCENARIO_H_
