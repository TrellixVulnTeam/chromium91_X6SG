// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/accessibility/accessibility_tree_formatter_utils_mac.h"

#include "base/strings/sys_string_conversions.h"
#include "content/browser/accessibility/accessibility_tools_utils_mac.h"
#include "content/browser/accessibility/browser_accessibility_mac.h"
#include "ui/accessibility/platform/inspect/ax_property_node.h"

using ui::AXPropertyNode;

namespace content {
namespace a11y {

namespace {

#define INT_FAIL(property_node, msg)                              \
  LOG(ERROR) << "Failed to parse " << property_node.name_or_value \
             << " to Int: " << msg;                               \
  return nil;

#define INTARRAY_FAIL(property_node, msg)                         \
  LOG(ERROR) << "Failed to parse " << property_node.name_or_value \
             << " to IntArray: " << msg;                          \
  return nil;

#define NSRANGE_FAIL(property_node, msg)                          \
  LOG(ERROR) << "Failed to parse " << property_node.name_or_value \
             << " to NSRange: " << msg;                           \
  return nil;

#define UIELEMENT_FAIL(property_node, msg)                        \
  LOG(ERROR) << "Failed to parse " << property_node.name_or_value \
             << " to UIElement: " << msg;                         \
  return nil;

#define TEXTMARKER_FAIL(property_node, msg)                                    \
  LOG(ERROR) << "Failed to parse " << property_node.name_or_value              \
             << " to AXTextMarker: " << msg                                    \
             << ". Expected format: {anchor, offset, affinity}, where anchor " \
                "is :line_num, offset is integer, affinity is either down, "   \
                "up or none";                                                  \
  return nil;

}  // namespace

// Line indexers

LineIndexer::LineIndexer(const gfx::NativeViewAccessible node) {
  int counter = 0;
  Build(node, &counter);
}

LineIndexer::~LineIndexer() {}

constexpr bool LineIndexer::NodeComparator::operator()(
    const gfx::NativeViewAccessible& lhs,
    const gfx::NativeViewAccessible& rhs) const {
  if (IsAXUIElement(lhs)) {
    DCHECK(IsAXUIElement(rhs));
    return CFHash(lhs) < CFHash(rhs);
  }
  DCHECK(IsBrowserAccessibilityCocoa(lhs));
  DCHECK(IsBrowserAccessibilityCocoa(rhs));
  return lhs < rhs;
}

std::string LineIndexer::IndexBy(const gfx::NativeViewAccessible node) const {
  std::string line_index = ":unknown";
  auto iter = map.find(node);
  if (iter != map.end()) {
    line_index = iter->second.line_index;
  }
  return line_index;
}

gfx::NativeViewAccessible LineIndexer::NodeBy(
    const std::string& identifier) const {
  // Finds a first match either by a line number in :LINE_NUM format or by DOM
  // id.
  for (auto& item : map) {
    if (item.second.line_index == identifier ||
        item.second.DOMid == identifier) {
      return item.first;
    }
  }
  return nil;
}

void LineIndexer::Build(const gfx::NativeViewAccessible node, int* counter) {
  const std::string line_index =
      std::string(1, ':') + base::NumberToString(++(*counter));

  const id domid_value =
      AttributeValueOf(node, base::SysUTF8ToNSString("AXDOMIdentifier"));
  const std::string domid =
      base::SysNSStringToUTF8(static_cast<NSString*>(domid_value));

  map.insert({node, {line_index, domid}});
  NSArray* children = ChildrenOf(node);
  for (gfx::NativeViewAccessible child in children) {
    Build(child, counter);
  }
}

// OptionalNSObject

std::string OptionalNSObject::ToString() const {
  if (IsNotApplicable()) {
    return "<n/a>";
  } else if (IsError()) {
    return "<error>";
  } else if (value == nil) {
    return "<nil>";
  } else {
    return base::SysNSStringToUTF8([NSString stringWithFormat:@"%@", value]);
  }
}

// AttributeInvoker

AttributeInvoker::AttributeInvoker(const LineIndexer* line_indexer)
    : node(nullptr), line_indexer(line_indexer) {}

AttributeInvoker::AttributeInvoker(const id node,
                                   const LineIndexer* line_indexer)
    : node(node), line_indexer(line_indexer) {
}

OptionalNSObject AttributeInvoker::Invoke(
    const AXPropertyNode& property_node) const {
  id target = TargetOf(property_node);
  if (!target) {
    // TODO(alexs): failing the tests when filters are incorrect is a good idea,
    // however crashing ax_dump tools on wrong input might be not. Figure out
    // a working solution that works nicely in both cases.
    LOG(ERROR) << "No target to invoke attribute";
    return OptionalNSObject::Error();
  }

  // Attributes
  for (NSString* attribute : AttributeNamesOf(target)) {
    if (property_node.IsMatching(base::SysNSStringToUTF8(attribute))) {
      return OptionalNSObject::NotNullOrNotApplicable(
          AttributeValueOf(target, attribute));
    }
  }

  // Parameterized attributes
  for (NSString* attribute : ParameterizedAttributeNamesOf(target)) {
    if (property_node.IsMatching(base::SysNSStringToUTF8(attribute))) {
      OptionalNSObject param = ParamByPropertyNode(property_node);
      if (param.IsNotNil()) {
        return OptionalNSObject(
            ParameterizedAttributeValueOf(target, attribute, *param));
      }
      return param;
    }
  }

  return OptionalNSObject::NotApplicable();
}

OptionalNSObject AttributeInvoker::GetValue(
    const std::string& property_name,
    const OptionalNSObject& param) const {
  NSString* attribute = base::SysUTF8ToNSString(property_name);
  NSArray* attributes = ParameterizedAttributeNamesOf(node);
  if ([attributes containsObject:attribute]) {
    if (param.IsNotNil()) {
      return OptionalNSObject(
          ParameterizedAttributeValueOf(node, attribute, *param));
    } else {
      return param;
    }
  }
  return OptionalNSObject::NotApplicable();
}

OptionalNSObject AttributeInvoker::GetValue(
    const std::string& property_name) const {
  NSString* attribute = base::SysUTF8ToNSString(property_name);
  NSArray* parameterized_attributes = AttributeNamesOf(node);
  if ([parameterized_attributes containsObject:attribute]) {
    return OptionalNSObject::NotNullOrNotApplicable(
        AttributeValueOf(node, attribute));
  }
  return OptionalNSObject::NotApplicable();
}

void AttributeInvoker::SetValue(const std::string& property_name,
                                const OptionalNSObject& value) const {
  NSString* attribute = base::SysUTF8ToNSString(property_name);
  NSArray* attributes = AttributeNamesOf(node);
  if ([attributes containsObject:attribute] &&
      IsAttributeSettable(node, attribute)) {
    SetAttributeValueOf(node, attribute, *value);
  }
}

id AttributeInvoker::TargetOf(const AXPropertyNode& property_node) const {
  return property_node.target.empty()
             ? node
             : line_indexer->NodeBy(property_node.target);
}

OptionalNSObject AttributeInvoker::ParamByPropertyNode(
    const AXPropertyNode& property_node) const {
  // NSAccessibility attributes always take a single parameter.
  if (property_node.parameters.size() != 1) {
    LOG(ERROR) << "Failed to parse " << property_node.original_property
               << ": single parameter is expected";
    return OptionalNSObject::Error();
  }

  // Nested attribute case: attempt to invoke an attribute for an argument node.
  const AXPropertyNode& arg_node = property_node.parameters[0];
  OptionalNSObject subvalue = Invoke(arg_node);
  if (!subvalue.IsNotApplicable()) {
    return subvalue;
  }

  // Otherwise parse argument node value.
  const std::string& property_name = property_node.name_or_value;
  if (property_name == "AXLineForIndex" ||
      property_name == "AXTextMarkerForIndex") {  // Int
    return OptionalNSObject::NotNilOrError(PropertyNodeToInt(arg_node));
  }
  if (property_name == "AXCellForColumnAndRow") {  // IntArray
    return OptionalNSObject::NotNilOrError(PropertyNodeToIntArray(arg_node));
  }
  if (property_name == "AXStringForRange") {  // NSRange
    return OptionalNSObject::NotNilOrError(PropertyNodeToRange(arg_node));
  }
  if (property_name == "AXIndexForChildUIElement") {  // UIElement
    return OptionalNSObject::NotNilOrError(PropertyNodeToUIElement(arg_node));
  }
  if (property_name == "AXIndexForTextMarker") {  // TextMarker
    return OptionalNSObject::NotNilOrError(PropertyNodeToTextMarker(arg_node));
  }
  if (property_name == "AXStringForTextMarkerRange") {  // TextMarkerRange
    return OptionalNSObject::NotNilOrError(
        PropertyNodeToTextMarkerRange(arg_node));
  }

  return OptionalNSObject::NotApplicable();
}

// NSNumber. Format: integer.
NSNumber* AttributeInvoker::PropertyNodeToInt(
    const AXPropertyNode& intnode) const {
  base::Optional<int> param = intnode.AsInt();
  if (!param) {
    INT_FAIL(intnode, "not a number")
  }
  return [NSNumber numberWithInt:*param];
}

// NSArray of two NSNumber. Format: [integer, integer].
NSArray* AttributeInvoker::PropertyNodeToIntArray(
    const AXPropertyNode& arraynode) const {
  if (arraynode.name_or_value != "[]") {
    INTARRAY_FAIL(arraynode, "not array")
  }

  NSMutableArray* array =
      [[NSMutableArray alloc] initWithCapacity:arraynode.parameters.size()];
  for (const auto& paramnode : arraynode.parameters) {
    base::Optional<int> param = paramnode.AsInt();
    if (!param) {
      INTARRAY_FAIL(arraynode, paramnode.name_or_value + " is not a number")
    }
    [array addObject:@(*param)];
  }
  return array;
}

// NSRange. Format: {loc: integer, len: integer}.
NSValue* AttributeInvoker::PropertyNodeToRange(
    const AXPropertyNode& dictnode) const {
  if (!dictnode.IsDict()) {
    NSRANGE_FAIL(dictnode, "dictionary is expected")
  }

  base::Optional<int> loc = dictnode.FindIntKey("loc");
  if (!loc) {
    NSRANGE_FAIL(dictnode, "no loc or loc is not a number")
  }

  base::Optional<int> len = dictnode.FindIntKey("len");
  if (!len) {
    NSRANGE_FAIL(dictnode, "no len or len is not a number")
  }

  return [NSValue valueWithRange:NSMakeRange(*loc, *len)];
}

// UIElement. Format: :line_num.
gfx::NativeViewAccessible AttributeInvoker::PropertyNodeToUIElement(
    const AXPropertyNode& uielement_node) const {
  gfx::NativeViewAccessible uielement =
      line_indexer->NodeBy(uielement_node.name_or_value);
  if (!uielement) {
    UIELEMENT_FAIL(uielement_node,
                   "no corresponding UIElement was found in the tree")
  }
  return uielement;
}

id AttributeInvoker::DictNodeToTextMarker(
    const AXPropertyNode& dictnode) const {
  if (!dictnode.IsDict()) {
    TEXTMARKER_FAIL(dictnode, "dictionary is expected")
  }
  if (dictnode.parameters.size() != 3) {
    TEXTMARKER_FAIL(dictnode, "wrong number of dictionary elements")
  }

  BrowserAccessibilityCocoa* anchor_cocoa =
      line_indexer->NodeBy(dictnode.parameters[0].name_or_value);
  if (!anchor_cocoa) {
    TEXTMARKER_FAIL(dictnode, "1st argument: wrong anchor")
  }

  base::Optional<int> offset = dictnode.parameters[1].AsInt();
  if (!offset) {
    TEXTMARKER_FAIL(dictnode, "2nd argument: wrong offset")
  }

  ax::mojom::TextAffinity affinity;
  const std::string& affinity_str = dictnode.parameters[2].name_or_value;
  if (affinity_str == "none") {
    affinity = ax::mojom::TextAffinity::kNone;
  } else if (affinity_str == "down") {
    affinity = ax::mojom::TextAffinity::kDownstream;
  } else if (affinity_str == "up") {
    affinity = ax::mojom::TextAffinity::kUpstream;
  } else {
    TEXTMARKER_FAIL(dictnode, "3rd argument: wrong affinity")
  }

  return content::AXTextMarkerFrom(anchor_cocoa, *offset, affinity);
}

id AttributeInvoker::PropertyNodeToTextMarker(
    const AXPropertyNode& dictnode) const {
  return DictNodeToTextMarker(dictnode);
}

id AttributeInvoker::PropertyNodeToTextMarkerRange(
    const AXPropertyNode& rangenode) const {
  if (!rangenode.IsDict()) {
    TEXTMARKER_FAIL(rangenode, "dictionary is expected")
  }

  const AXPropertyNode* anchornode = rangenode.FindKey("anchor");
  if (!anchornode) {
    TEXTMARKER_FAIL(rangenode, "no anchor")
  }

  id anchor_textmarker = DictNodeToTextMarker(*anchornode);
  if (!anchor_textmarker) {
    TEXTMARKER_FAIL(rangenode, "failed to parse anchor")
  }

  const AXPropertyNode* focusnode = rangenode.FindKey("focus");
  if (!focusnode) {
    TEXTMARKER_FAIL(rangenode, "no focus")
  }

  id focus_textmarker = DictNodeToTextMarker(*focusnode);
  if (!focus_textmarker) {
    TEXTMARKER_FAIL(rangenode, "failed to parse focus")
  }

  return content::AXTextMarkerRangeFrom(anchor_textmarker, focus_textmarker);
}

OptionalNSObject TextMarkerRangeGetStartMarker(const OptionalNSObject& obj) {
  if (!IsAXTextMarkerRange(*obj))
    return OptionalNSObject::NotApplicable();

  const BrowserAccessibility::AXRange range = AXTextMarkerRangeToAXRange(*obj);
  if (range.IsNull())
    return OptionalNSObject::Error();

  auto* manager =
      BrowserAccessibilityManager::FromID(range.anchor()->tree_id());
  DCHECK(manager) << "A non-null range should have an associated AX tree.";
  const BrowserAccessibility* node =
      manager->GetFromID(range.anchor()->anchor_id());
  DCHECK(node) << "A non-null range should have a non-null anchor node.";
  const BrowserAccessibilityCocoa* cocoa_node =
      ToBrowserAccessibilityCocoa(node);
  return OptionalNSObject::NotNilOrError(content::AXTextMarkerFrom(
      cocoa_node, range.anchor()->text_offset(), range.anchor()->affinity()));
}

OptionalNSObject TextMarkerRangeGetEndMarker(const OptionalNSObject& obj) {
  if (!IsAXTextMarkerRange(*obj))
    return OptionalNSObject::NotApplicable();

  const BrowserAccessibility::AXRange range = AXTextMarkerRangeToAXRange(*obj);
  if (range.IsNull())
    return OptionalNSObject::Error();

  auto* manager = BrowserAccessibilityManager::FromID(range.focus()->tree_id());
  DCHECK(manager) << "A non-null range should have an associated AX tree.";
  const BrowserAccessibility* node =
      manager->GetFromID(range.focus()->anchor_id());
  DCHECK(node) << "A non-null range should have a non-null focus node.";
  const BrowserAccessibilityCocoa* cocoa_node =
      ToBrowserAccessibilityCocoa(node);
  return OptionalNSObject::NotNilOrError(content::AXTextMarkerFrom(
      cocoa_node, range.focus()->text_offset(), range.focus()->affinity()));
}

OptionalNSObject MakePairArray(const OptionalNSObject& obj1,
                               const OptionalNSObject& obj2) {
  if (!obj1.IsNotNil() || !obj2.IsNotNil())
    return OptionalNSObject::Error();
  return OptionalNSObject::NotNilOrError(
      [NSArray arrayWithObjects:*obj1, *obj2, nil]);
}

}  // namespace a11y
}  // namespace content
