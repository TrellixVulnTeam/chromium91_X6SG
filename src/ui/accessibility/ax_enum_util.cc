// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/accessibility/ax_enum_util.h"

#include "ui/accessibility/ax_enums.mojom.h"

#include "ui/base/l10n/l10n_util.h"
#include "ui/strings/grit/ui_strings.h"

namespace ui {

const char* ToString(ax::mojom::Event event) {
  switch (event) {
    case ax::mojom::Event::kNone:
      return "none";
    case ax::mojom::Event::kActiveDescendantChanged:
      return "activedescendantchanged";
    case ax::mojom::Event::kAlert:
      return "alert";
    case ax::mojom::Event::kAriaAttributeChanged:
      return "ariaAttributeChanged";
    case ax::mojom::Event::kAutocorrectionOccured:
      return "autocorrectionOccured";
    case ax::mojom::Event::kBlur:
      return "blur";
    case ax::mojom::Event::kCheckedStateChanged:
      return "checkedStateChanged";
    case ax::mojom::Event::kChildrenChanged:
      return "childrenChanged";
    case ax::mojom::Event::kClicked:
      return "clicked";
    case ax::mojom::Event::kControlsChanged:
      return "controlsChanged";
    case ax::mojom::Event::kDocumentSelectionChanged:
      return "documentSelectionChanged";
    case ax::mojom::Event::kDocumentTitleChanged:
      return "documentTitleChanged";
    case ax::mojom::Event::kEndOfTest:
      return "endOfTest";
    case ax::mojom::Event::kExpandedChanged:
      return "expandedChanged";
    case ax::mojom::Event::kFocus:
      return "focus";
    case ax::mojom::Event::kFocusAfterMenuClose:
      return "focusAfterMenuClose";
    case ax::mojom::Event::kFocusContext:
      return "focusContext";
    case ax::mojom::Event::kHide:
      return "hide";
    case ax::mojom::Event::kHitTestResult:
      return "hitTestResult";
    case ax::mojom::Event::kHover:
      return "hover";
    case ax::mojom::Event::kImageFrameUpdated:
      return "imageFrameUpdated";
    case ax::mojom::Event::kLayoutComplete:
      return "layoutComplete";
    case ax::mojom::Event::kLiveRegionCreated:
      return "liveRegionCreated";
    case ax::mojom::Event::kLiveRegionChanged:
      return "liveRegionChanged";
    case ax::mojom::Event::kLoadComplete:
      return "loadComplete";
    case ax::mojom::Event::kLoadStart:
      return "loadStart";
    case ax::mojom::Event::kLocationChanged:
      return "locationChanged";
    case ax::mojom::Event::kMediaStartedPlaying:
      return "mediaStartedPlaying";
    case ax::mojom::Event::kMediaStoppedPlaying:
      return "mediaStoppedPlaying";
    case ax::mojom::Event::kMenuEnd:
      return "menuEnd";
    case ax::mojom::Event::kMenuListValueChanged:
      return "menuListValueChanged";
    case ax::mojom::Event::kMenuPopupEnd:
      return "menuPopupEnd";
    case ax::mojom::Event::kMenuPopupStart:
      return "menuPopupStart";
    case ax::mojom::Event::kMenuStart:
      return "menuStart";
    case ax::mojom::Event::kMouseCanceled:
      return "mouseCanceled";
    case ax::mojom::Event::kMouseDragged:
      return "mouseDragged";
    case ax::mojom::Event::kMouseMoved:
      return "mouseMoved";
    case ax::mojom::Event::kMousePressed:
      return "mousePressed";
    case ax::mojom::Event::kMouseReleased:
      return "mouseReleased";
    case ax::mojom::Event::kRowCollapsed:
      return "rowCollapsed";
    case ax::mojom::Event::kRowCountChanged:
      return "rowCountChanged";
    case ax::mojom::Event::kRowExpanded:
      return "rowExpanded";
    case ax::mojom::Event::kScrollPositionChanged:
      return "scrollPositionChanged";
    case ax::mojom::Event::kScrolledToAnchor:
      return "scrolledToAnchor";
    case ax::mojom::Event::kSelectedChildrenChanged:
      return "selectedChildrenChanged";
    case ax::mojom::Event::kSelection:
      return "selection";
    case ax::mojom::Event::kSelectionAdd:
      return "selectionAdd";
    case ax::mojom::Event::kSelectionRemove:
      return "selectionRemove";
    case ax::mojom::Event::kShow:
      return "show";
    case ax::mojom::Event::kStateChanged:
      return "stateChanged";
    case ax::mojom::Event::kTextChanged:
      return "textChanged";
    case ax::mojom::Event::kTextSelectionChanged:
      return "textSelectionChanged";
    case ax::mojom::Event::kTooltipClosed:
      return "tooltipClosed";
    case ax::mojom::Event::kTooltipOpened:
      return "tooltipOpened";
    case ax::mojom::Event::kTreeChanged:
      return "treeChanged";
    case ax::mojom::Event::kValueChanged:
      return "valueChanged";
    case ax::mojom::Event::kWindowActivated:
      return "windowActivated";
    case ax::mojom::Event::kWindowDeactivated:
      return "windowDeactivated";
    case ax::mojom::Event::kWindowVisibilityChanged:
      return "windowVisibilityChanged";
  }

  return "";
}

const char* ToString(ax::mojom::Role role) {
  switch (role) {
    case ax::mojom::Role::kNone:
      return "none";
    case ax::mojom::Role::kAbbr:
      return "abbr";
    case ax::mojom::Role::kAlertDialog:
      return "alertDialog";
    case ax::mojom::Role::kAlert:
      return "alert";
    case ax::mojom::Role::kAnchor:
      return "anchor";
    case ax::mojom::Role::kApplication:
      return "application";
    case ax::mojom::Role::kArticle:
      return "article";
    case ax::mojom::Role::kAudio:
      return "audio";
    case ax::mojom::Role::kBanner:
      return "banner";
    case ax::mojom::Role::kBlockquote:
      return "blockquote";
    case ax::mojom::Role::kButton:
      return "button";
    case ax::mojom::Role::kCanvas:
      return "canvas";
    case ax::mojom::Role::kCaption:
      return "caption";
    case ax::mojom::Role::kCaret:
      return "caret";
    case ax::mojom::Role::kCell:
      return "cell";
    case ax::mojom::Role::kCheckBox:
      return "checkBox";
    case ax::mojom::Role::kClient:
      return "client";
    case ax::mojom::Role::kCode:
      return "code";
    case ax::mojom::Role::kColorWell:
      return "colorWell";
    case ax::mojom::Role::kColumnHeader:
      return "columnHeader";
    case ax::mojom::Role::kColumn:
      return "column";
    case ax::mojom::Role::kComboBoxGrouping:
      return "comboBoxGrouping";
    case ax::mojom::Role::kComboBoxMenuButton:
      return "comboBoxMenuButton";
    case ax::mojom::Role::kComment:
      return "comment";
    case ax::mojom::Role::kComplementary:
      return "complementary";
    case ax::mojom::Role::kContentDeletion:
      return "contentDeletion";
    case ax::mojom::Role::kContentInsertion:
      return "contentInsertion";
    case ax::mojom::Role::kContentInfo:
      return "contentInfo";
    case ax::mojom::Role::kDate:
      return "date";
    case ax::mojom::Role::kDateTime:
      return "dateTime";
    case ax::mojom::Role::kDefinition:
      return "definition";
    case ax::mojom::Role::kDescriptionListDetail:
      return "descriptionListDetail";
    case ax::mojom::Role::kDescriptionList:
      return "descriptionList";
    case ax::mojom::Role::kDescriptionListTerm:
      return "descriptionListTerm";
    case ax::mojom::Role::kDesktop:
      return "desktop";
    case ax::mojom::Role::kDetails:
      return "details";
    case ax::mojom::Role::kDialog:
      return "dialog";
    case ax::mojom::Role::kDirectory:
      return "directory";
    case ax::mojom::Role::kDisclosureTriangle:
      return "disclosureTriangle";
    case ax::mojom::Role::kDocAbstract:
      return "docAbstract";
    case ax::mojom::Role::kDocAcknowledgments:
      return "docAcknowledgments";
    case ax::mojom::Role::kDocAfterword:
      return "docAfterword";
    case ax::mojom::Role::kDocAppendix:
      return "docAppendix";
    case ax::mojom::Role::kDocBackLink:
      return "docBackLink";
    case ax::mojom::Role::kDocBiblioEntry:
      return "docBiblioEntry";
    case ax::mojom::Role::kDocBibliography:
      return "docBibliography";
    case ax::mojom::Role::kDocBiblioRef:
      return "docBiblioRef";
    case ax::mojom::Role::kDocChapter:
      return "docChapter";
    case ax::mojom::Role::kDocColophon:
      return "docColophon";
    case ax::mojom::Role::kDocConclusion:
      return "docConclusion";
    case ax::mojom::Role::kDocCover:
      return "docCover";
    case ax::mojom::Role::kDocCredit:
      return "docCredit";
    case ax::mojom::Role::kDocCredits:
      return "docCredits";
    case ax::mojom::Role::kDocDedication:
      return "docDedication";
    case ax::mojom::Role::kDocEndnote:
      return "docEndnote";
    case ax::mojom::Role::kDocEndnotes:
      return "docEndnotes";
    case ax::mojom::Role::kDocEpigraph:
      return "docEpigraph";
    case ax::mojom::Role::kDocEpilogue:
      return "docEpilogue";
    case ax::mojom::Role::kDocErrata:
      return "docErrata";
    case ax::mojom::Role::kDocExample:
      return "docExample";
    case ax::mojom::Role::kDocFootnote:
      return "docFootnote";
    case ax::mojom::Role::kDocForeword:
      return "docForeword";
    case ax::mojom::Role::kDocGlossary:
      return "docGlossary";
    case ax::mojom::Role::kDocGlossRef:
      return "docGlossref";
    case ax::mojom::Role::kDocIndex:
      return "docIndex";
    case ax::mojom::Role::kDocIntroduction:
      return "docIntroduction";
    case ax::mojom::Role::kDocNoteRef:
      return "docNoteRef";
    case ax::mojom::Role::kDocNotice:
      return "docNotice";
    case ax::mojom::Role::kDocPageBreak:
      return "docPageBreak";
    case ax::mojom::Role::kDocPageFooter:
      return "docPageFooter";
    case ax::mojom::Role::kDocPageHeader:
      return "docPageHeader";
    case ax::mojom::Role::kDocPageList:
      return "docPageList";
    case ax::mojom::Role::kDocPart:
      return "docPart";
    case ax::mojom::Role::kDocPreface:
      return "docPreface";
    case ax::mojom::Role::kDocPrologue:
      return "docPrologue";
    case ax::mojom::Role::kDocPullquote:
      return "docPullquote";
    case ax::mojom::Role::kDocQna:
      return "docQna";
    case ax::mojom::Role::kDocSubtitle:
      return "docSubtitle";
    case ax::mojom::Role::kDocTip:
      return "docTip";
    case ax::mojom::Role::kDocToc:
      return "docToc";
    case ax::mojom::Role::kDocument:
      return "document";
    case ax::mojom::Role::kEmbeddedObject:
      return "embeddedObject";
    case ax::mojom::Role::kEmphasis:
      return "emphasis";
    case ax::mojom::Role::kFeed:
      return "feed";
    case ax::mojom::Role::kFigcaption:
      return "figcaption";
    case ax::mojom::Role::kFigure:
      return "figure";
    case ax::mojom::Role::kFooter:
      return "footer";
    case ax::mojom::Role::kFooterAsNonLandmark:
      return "footerAsNonLandmark";
    case ax::mojom::Role::kForm:
      return "form";
    case ax::mojom::Role::kGenericContainer:
      return "genericContainer";
    case ax::mojom::Role::kGraphicsDocument:
      return "graphicsDocument";
    case ax::mojom::Role::kGraphicsObject:
      return "graphicsObject";
    case ax::mojom::Role::kGraphicsSymbol:
      return "graphicsSymbol";
    case ax::mojom::Role::kGrid:
      return "grid";
    case ax::mojom::Role::kGroup:
      return "group";
    case ax::mojom::Role::kHeader:
      return "header";
    case ax::mojom::Role::kHeaderAsNonLandmark:
      return "headerAsNonLandmark";
    case ax::mojom::Role::kHeading:
      return "heading";
    case ax::mojom::Role::kIframe:
      return "iframe";
    case ax::mojom::Role::kIframePresentational:
      return "iframePresentational";
    case ax::mojom::Role::kIgnored:
      return "ignored";
    case ax::mojom::Role::kImage:
      return "image";
    case ax::mojom::Role::kImeCandidate:
      return "imeCandidate";
    case ax::mojom::Role::kInlineTextBox:
      return "inlineTextBox";
    case ax::mojom::Role::kInputTime:
      return "inputTime";
    case ax::mojom::Role::kKeyboard:
      return "keyboard";
    case ax::mojom::Role::kLabelText:
      return "labelText";
    case ax::mojom::Role::kLayoutTable:
      return "layoutTable";
    case ax::mojom::Role::kLayoutTableCell:
      return "layoutTableCell";
    case ax::mojom::Role::kLayoutTableRow:
      return "layoutTableRow";
    case ax::mojom::Role::kLegend:
      return "legend";
    case ax::mojom::Role::kLineBreak:
      return "lineBreak";
    case ax::mojom::Role::kLink:
      return "link";
    case ax::mojom::Role::kList:
      return "list";
    case ax::mojom::Role::kListBoxOption:
      return "listBoxOption";
    case ax::mojom::Role::kListBox:
      return "listBox";
    case ax::mojom::Role::kListGrid:
      return "listGrid";
    case ax::mojom::Role::kListItem:
      return "listItem";
    case ax::mojom::Role::kListMarker:
      return "listMarker";
    case ax::mojom::Role::kLog:
      return "log";
    case ax::mojom::Role::kMain:
      return "main";
    case ax::mojom::Role::kMark:
      return "mark";
    case ax::mojom::Role::kMarquee:
      return "marquee";
    case ax::mojom::Role::kMath:
      return "math";
    case ax::mojom::Role::kMenu:
      return "menu";
    case ax::mojom::Role::kMenuBar:
      return "menuBar";
    case ax::mojom::Role::kMenuItem:
      return "menuItem";
    case ax::mojom::Role::kMenuItemCheckBox:
      return "menuItemCheckBox";
    case ax::mojom::Role::kMenuItemRadio:
      return "menuItemRadio";
    case ax::mojom::Role::kMenuListOption:
      return "menuListOption";
    case ax::mojom::Role::kMenuListPopup:
      return "menuListPopup";
    case ax::mojom::Role::kMeter:
      return "meter";
    case ax::mojom::Role::kNavigation:
      return "navigation";
    case ax::mojom::Role::kNote:
      return "note";
    case ax::mojom::Role::kPane:
      return "pane";
    case ax::mojom::Role::kParagraph:
      return "paragraph";
    case ax::mojom::Role::kPdfActionableHighlight:
      return "pdfActionableHighlight";
    case ax::mojom::Role::kPdfRoot:
      return "pdfRoot";
    case ax::mojom::Role::kPluginObject:
      return "pluginObject";
    case ax::mojom::Role::kPopUpButton:
      return "popUpButton";
    case ax::mojom::Role::kPortal:
      return "portal";
    case ax::mojom::Role::kPre:
      return "pre";
    case ax::mojom::Role::kPresentational:
      return "presentational";
    case ax::mojom::Role::kProgressIndicator:
      return "progressIndicator";
    case ax::mojom::Role::kRadioButton:
      return "radioButton";
    case ax::mojom::Role::kRadioGroup:
      return "radioGroup";
    case ax::mojom::Role::kRegion:
      return "region";
    case ax::mojom::Role::kRootWebArea:
      return "rootWebArea";
    case ax::mojom::Role::kRow:
      return "row";
    case ax::mojom::Role::kRowGroup:
      return "rowGroup";
    case ax::mojom::Role::kRowHeader:
      return "rowHeader";
    case ax::mojom::Role::kRuby:
      return "ruby";
    case ax::mojom::Role::kRubyAnnotation:
      return "rubyAnnotation";
    case ax::mojom::Role::kSection:
      return "section";
    case ax::mojom::Role::kStrong:
      return "strong";
    case ax::mojom::Role::kSuggestion:
      return "suggestion";
    case ax::mojom::Role::kSvgRoot:
      return "svgRoot";
    case ax::mojom::Role::kScrollBar:
      return "scrollBar";
    case ax::mojom::Role::kScrollView:
      return "scrollView";
    case ax::mojom::Role::kSearch:
      return "search";
    case ax::mojom::Role::kSearchBox:
      return "searchBox";
    case ax::mojom::Role::kSlider:
      return "slider";
    case ax::mojom::Role::kSpinButton:
      return "spinButton";
    case ax::mojom::Role::kSplitter:
      return "splitter";
    case ax::mojom::Role::kStaticText:
      return "staticText";
    case ax::mojom::Role::kStatus:
      return "status";
    case ax::mojom::Role::kSwitch:
      return "switch";
    case ax::mojom::Role::kTabList:
      return "tabList";
    case ax::mojom::Role::kTabPanel:
      return "tabPanel";
    case ax::mojom::Role::kTab:
      return "tab";
    case ax::mojom::Role::kTable:
      return "table";
    case ax::mojom::Role::kTableHeaderContainer:
      return "tableHeaderContainer";
    case ax::mojom::Role::kTerm:
      return "term";
    case ax::mojom::Role::kTextField:
      return "textField";
    case ax::mojom::Role::kTextFieldWithComboBox:
      return "textFieldWithComboBox";
    case ax::mojom::Role::kTime:
      return "time";
    case ax::mojom::Role::kTimer:
      return "timer";
    case ax::mojom::Role::kTitleBar:
      return "titleBar";
    case ax::mojom::Role::kToggleButton:
      return "toggleButton";
    case ax::mojom::Role::kToolbar:
      return "toolbar";
    case ax::mojom::Role::kTreeGrid:
      return "treeGrid";
    case ax::mojom::Role::kTreeItem:
      return "treeItem";
    case ax::mojom::Role::kTree:
      return "tree";
    case ax::mojom::Role::kUnknown:
      return "unknown";
    case ax::mojom::Role::kTooltip:
      return "tooltip";
    case ax::mojom::Role::kVideo:
      return "video";
    case ax::mojom::Role::kWebView:
      return "webView";
    case ax::mojom::Role::kWindow:
      return "window";
  }

  return "";
}

const char* ToString(ax::mojom::State state) {
  switch (state) {
    case ax::mojom::State::kNone:
      return "none";
    case ax::mojom::State::kAutofillAvailable:
      return "autofillAvailable";
    case ax::mojom::State::kCollapsed:
      return "collapsed";
    case ax::mojom::State::kDefault:
      return "default";
    case ax::mojom::State::kEditable:
      return "editable";
    case ax::mojom::State::kExpanded:
      return "expanded";
    case ax::mojom::State::kFocusable:
      return "focusable";
    case ax::mojom::State::kHorizontal:
      return "horizontal";
    case ax::mojom::State::kHovered:
      return "hovered";
    case ax::mojom::State::kIgnored:
      return "ignored";
    case ax::mojom::State::kInvisible:
      return "invisible";
    case ax::mojom::State::kLinked:
      return "linked";
    case ax::mojom::State::kMultiline:
      return "multiline";
    case ax::mojom::State::kMultiselectable:
      return "multiselectable";
    case ax::mojom::State::kProtected:
      return "protected";
    case ax::mojom::State::kRequired:
      return "required";
    case ax::mojom::State::kRichlyEditable:
      return "richlyEditable";
    case ax::mojom::State::kVertical:
      return "vertical";
    case ax::mojom::State::kVisited:
      return "visited";
  }

  return "";
}

const char* ToString(ax::mojom::Action action) {
  switch (action) {
    case ax::mojom::Action::kNone:
      return "none";
    case ax::mojom::Action::kBlur:
      return "blur";
    case ax::mojom::Action::kClearAccessibilityFocus:
      return "clearAccessibilityFocus";
    case ax::mojom::Action::kCollapse:
      return "collapse";
    case ax::mojom::Action::kCustomAction:
      return "customAction";
    case ax::mojom::Action::kDecrement:
      return "decrement";
    case ax::mojom::Action::kDoDefault:
      return "doDefault";
    case ax::mojom::Action::kExpand:
      return "expand";
    case ax::mojom::Action::kFocus:
      return "focus";
    case ax::mojom::Action::kGetImageData:
      return "getImageData";
    case ax::mojom::Action::kHitTest:
      return "hitTest";
    case ax::mojom::Action::kIncrement:
      return "increment";
    case ax::mojom::Action::kLoadInlineTextBoxes:
      return "loadInlineTextBoxes";
    case ax::mojom::Action::kReplaceSelectedText:
      return "replaceSelectedText";
    case ax::mojom::Action::kScrollBackward:
      return "scrollBackward";
    case ax::mojom::Action::kScrollForward:
      return "scrollForward";
    case ax::mojom::Action::kScrollUp:
      return "scrollUp";
    case ax::mojom::Action::kScrollDown:
      return "scrollDown";
    case ax::mojom::Action::kScrollLeft:
      return "scrollLeft";
    case ax::mojom::Action::kScrollRight:
      return "scrollRight";
    case ax::mojom::Action::kScrollToMakeVisible:
      return "scrollToMakeVisible";
    case ax::mojom::Action::kScrollToPoint:
      return "scrollToPoint";
    case ax::mojom::Action::kSetAccessibilityFocus:
      return "setAccessibilityFocus";
    case ax::mojom::Action::kSetScrollOffset:
      return "setScrollOffset";
    case ax::mojom::Action::kSetSelection:
      return "setSelection";
    case ax::mojom::Action::kSetSequentialFocusNavigationStartingPoint:
      return "setSequentialFocusNavigationStartingPoint";
    case ax::mojom::Action::kSetValue:
      return "setValue";
    case ax::mojom::Action::kShowContextMenu:
      return "showContextMenu";
    case ax::mojom::Action::kGetTextLocation:
      return "getTextLocation";
    case ax::mojom::Action::kAnnotatePageImages:
      return "annotatePageImages";
    case ax::mojom::Action::kSignalEndOfTest:
      return "signalEndOfTest";
    case ax::mojom::Action::kShowTooltip:
      return "showTooltip";
    case ax::mojom::Action::kHideTooltip:
      return "hideTooltip";
    case ax::mojom::Action::kInternalInvalidateTree:
      return "internalInvalidateTree";
  }

  return "";
}

const char* ToString(ax::mojom::ActionFlags action_flags) {
  switch (action_flags) {
    case ax::mojom::ActionFlags::kNone:
      return "none";
    case ax::mojom::ActionFlags::kRequestImages:
      return "requestImages";
    case ax::mojom::ActionFlags::kRequestInlineTextBoxes:
      return "requestInlineTextBoxes";
  }

  return "";
}

const char* ToString(ax::mojom::ScrollAlignment scroll_alignment) {
  switch (scroll_alignment) {
    case ax::mojom::ScrollAlignment::kNone:
      return "none";
    case ax::mojom::ScrollAlignment::kScrollAlignmentCenter:
      return "scrollAlignmentCenter";
    case ax::mojom::ScrollAlignment::kScrollAlignmentTop:
      return "scrollAlignmentTop";
    case ax::mojom::ScrollAlignment::kScrollAlignmentBottom:
      return "scrollAlignmentBottom";
    case ax::mojom::ScrollAlignment::kScrollAlignmentLeft:
      return "scrollAlignmentLeft";
    case ax::mojom::ScrollAlignment::kScrollAlignmentRight:
      return "scrollAlignmentRight";
    case ax::mojom::ScrollAlignment::kScrollAlignmentClosestEdge:
      return "scrollAlignmentClosestEdge";
  }
}

const char* ToString(ax::mojom::DefaultActionVerb default_action_verb) {
  switch (default_action_verb) {
    case ax::mojom::DefaultActionVerb::kNone:
      return "none";
    case ax::mojom::DefaultActionVerb::kActivate:
      return "activate";
    case ax::mojom::DefaultActionVerb::kCheck:
      return "check";
    case ax::mojom::DefaultActionVerb::kClick:
      return "click";
    case ax::mojom::DefaultActionVerb::kClickAncestor:
      // Some screen readers, such as Jaws, expect the following spelling of
      // this verb.
      return "clickAncestor";
    case ax::mojom::DefaultActionVerb::kJump:
      return "jump";
    case ax::mojom::DefaultActionVerb::kOpen:
      return "open";
    case ax::mojom::DefaultActionVerb::kPress:
      return "press";
    case ax::mojom::DefaultActionVerb::kSelect:
      return "select";
    case ax::mojom::DefaultActionVerb::kUncheck:
      return "uncheck";
  }

  return "";
}

std::string ToLocalizedString(ax::mojom::DefaultActionVerb action_verb) {
  switch (action_verb) {
    case ax::mojom::DefaultActionVerb::kNone:
      return "";
    case ax::mojom::DefaultActionVerb::kActivate:
      return l10n_util::GetStringUTF8(IDS_AX_ACTIVATE_ACTION_VERB);
    case ax::mojom::DefaultActionVerb::kCheck:
      return l10n_util::GetStringUTF8(IDS_AX_CHECK_ACTION_VERB);
    case ax::mojom::DefaultActionVerb::kClick:
      return l10n_util::GetStringUTF8(IDS_AX_CLICK_ACTION_VERB);
    case ax::mojom::DefaultActionVerb::kClickAncestor:
      return l10n_util::GetStringUTF8(IDS_AX_CLICK_ANCESTOR_ACTION_VERB);
    case ax::mojom::DefaultActionVerb::kJump:
      return l10n_util::GetStringUTF8(IDS_AX_JUMP_ACTION_VERB);
    case ax::mojom::DefaultActionVerb::kOpen:
      return l10n_util::GetStringUTF8(IDS_AX_OPEN_ACTION_VERB);
    case ax::mojom::DefaultActionVerb::kPress:
      return l10n_util::GetStringUTF8(IDS_AX_PRESS_ACTION_VERB);
    case ax::mojom::DefaultActionVerb::kSelect:
      return l10n_util::GetStringUTF8(IDS_AX_SELECT_ACTION_VERB);
    case ax::mojom::DefaultActionVerb::kUncheck:
      return l10n_util::GetStringUTF8(IDS_AX_UNCHECK_ACTION_VERB);
  }

  return "";
}

const char* ToString(ax::mojom::Mutation mutation) {
  switch (mutation) {
    case ax::mojom::Mutation::kNone:
      return "none";
    case ax::mojom::Mutation::kNodeCreated:
      return "nodeCreated";
    case ax::mojom::Mutation::kSubtreeCreated:
      return "subtreeCreated";
    case ax::mojom::Mutation::kNodeChanged:
      return "nodeChanged";
    case ax::mojom::Mutation::kNodeRemoved:
      return "nodeRemoved";
  }

  return "";
}

const char* ToString(ax::mojom::StringAttribute string_attribute) {
  switch (string_attribute) {
    case ax::mojom::StringAttribute::kNone:
      return "none";
    case ax::mojom::StringAttribute::kAccessKey:
      return "accessKey";
    case ax::mojom::StringAttribute::kAriaInvalidValue:
      return "ariaInvalidValue";
    case ax::mojom::StringAttribute::kAutoComplete:
      return "autoComplete";
    case ax::mojom::StringAttribute::kCheckedStateDescription:
      return "checkedStateDescription";
    case ax::mojom::StringAttribute::kChildTreeId:
      return "childTreeId";
    case ax::mojom::StringAttribute::kChildTreeNodeAppId:
      return "childTreeNodeAppId";
    case ax::mojom::StringAttribute::kClassName:
      return "className";
    case ax::mojom::StringAttribute::kContainerLiveRelevant:
      return "containerLiveRelevant";
    case ax::mojom::StringAttribute::kContainerLiveStatus:
      return "containerLiveStatus";
    case ax::mojom::StringAttribute::kDescription:
      return "description";
    case ax::mojom::StringAttribute::kDisplay:
      return "display";
    case ax::mojom::StringAttribute::kFontFamily:
      return "fontFamily";
    case ax::mojom::StringAttribute::kHtmlTag:
      return "htmlTag";
    case ax::mojom::StringAttribute::kImageAnnotation:
      return "imageAnnotation";
    case ax::mojom::StringAttribute::kImageDataUrl:
      return "imageDataUrl";
    case ax::mojom::StringAttribute::kInnerHtml:
      return "innerHtml";
    case ax::mojom::StringAttribute::kInputType:
      return "inputType";
    case ax::mojom::StringAttribute::kKeyShortcuts:
      return "keyShortcuts";
    case ax::mojom::StringAttribute::kLanguage:
      return "language";
    case ax::mojom::StringAttribute::kName:
      return "name";
    case ax::mojom::StringAttribute::kLiveRelevant:
      return "liveRelevant";
    case ax::mojom::StringAttribute::kLiveStatus:
      return "liveStatus";
    case ax::mojom::StringAttribute::kParentTreeNodeAppId:
      return "parentTreeNodeAppId";
    case ax::mojom::StringAttribute::kPlaceholder:
      return "placeholder";
    case ax::mojom::StringAttribute::kRole:
      return "role";
    case ax::mojom::StringAttribute::kRoleDescription:
      return "roleDescription";
    case ax::mojom::StringAttribute::kTooltip:
      return "tooltip";
    case ax::mojom::StringAttribute::kUrl:
      return "url";
    case ax::mojom::StringAttribute::kValue:
      return "value";
    case ax::mojom::StringAttribute::kVirtualContent:
      return "virtualContent";
  }

  return "";
}

const char* ToString(ax::mojom::IntAttribute int_attribute) {
  switch (int_attribute) {
    case ax::mojom::IntAttribute::kNone:
      return "none";
    case ax::mojom::IntAttribute::kDefaultActionVerb:
      return "defaultActionVerb";
    case ax::mojom::IntAttribute::kDropeffect:
      return "dropeffect";
    case ax::mojom::IntAttribute::kScrollX:
      return "scrollX";
    case ax::mojom::IntAttribute::kScrollXMin:
      return "scrollXMin";
    case ax::mojom::IntAttribute::kScrollXMax:
      return "scrollXMax";
    case ax::mojom::IntAttribute::kScrollY:
      return "scrollY";
    case ax::mojom::IntAttribute::kScrollYMin:
      return "scrollYMin";
    case ax::mojom::IntAttribute::kScrollYMax:
      return "scrollYMax";
    case ax::mojom::IntAttribute::kTextSelStart:
      return "textSelStart";
    case ax::mojom::IntAttribute::kTextSelEnd:
      return "textSelEnd";
    case ax::mojom::IntAttribute::kAriaColumnCount:
      return "ariaColumnCount";
    case ax::mojom::IntAttribute::kAriaCellColumnIndex:
      return "ariaCellColumnIndex";
    case ax::mojom::IntAttribute::kAriaCellColumnSpan:
      return "ariaCellColumnSpan";
    case ax::mojom::IntAttribute::kAriaRowCount:
      return "ariaRowCount";
    case ax::mojom::IntAttribute::kAriaCellRowIndex:
      return "ariaCellRowIndex";
    case ax::mojom::IntAttribute::kAriaCellRowSpan:
      return "ariaCellRowSpan";
    case ax::mojom::IntAttribute::kTableRowCount:
      return "tableRowCount";
    case ax::mojom::IntAttribute::kTableColumnCount:
      return "tableColumnCount";
    case ax::mojom::IntAttribute::kTableHeaderId:
      return "tableHeaderId";
    case ax::mojom::IntAttribute::kTableRowIndex:
      return "tableRowIndex";
    case ax::mojom::IntAttribute::kTableRowHeaderId:
      return "tableRowHeaderId";
    case ax::mojom::IntAttribute::kTableColumnIndex:
      return "tableColumnIndex";
    case ax::mojom::IntAttribute::kTableColumnHeaderId:
      return "tableColumnHeaderId";
    case ax::mojom::IntAttribute::kTableCellColumnIndex:
      return "tableCellColumnIndex";
    case ax::mojom::IntAttribute::kTableCellColumnSpan:
      return "tableCellColumnSpan";
    case ax::mojom::IntAttribute::kTableCellRowIndex:
      return "tableCellRowIndex";
    case ax::mojom::IntAttribute::kTableCellRowSpan:
      return "tableCellRowSpan";
    case ax::mojom::IntAttribute::kSortDirection:
      return "sortDirection";
    case ax::mojom::IntAttribute::kHierarchicalLevel:
      return "hierarchicalLevel";
    case ax::mojom::IntAttribute::kNameFrom:
      return "nameFrom";
    case ax::mojom::IntAttribute::kDescriptionFrom:
      return "descriptionFrom";
    case ax::mojom::IntAttribute::kActivedescendantId:
      return "activedescendantId";
    case ax::mojom::IntAttribute::kErrormessageId:
      return "errormessageId";
    case ax::mojom::IntAttribute::kInPageLinkTargetId:
      return "inPageLinkTargetId";
    case ax::mojom::IntAttribute::kMemberOfId:
      return "memberOfId";
    case ax::mojom::IntAttribute::kNextOnLineId:
      return "nextOnLineId";
    case ax::mojom::IntAttribute::kPopupForId:
      return "popupForId";
    case ax::mojom::IntAttribute::kPreviousOnLineId:
      return "previousOnLineId";
    case ax::mojom::IntAttribute::kRestriction:
      return "restriction";
    case ax::mojom::IntAttribute::kSetSize:
      return "setSize";
    case ax::mojom::IntAttribute::kPosInSet:
      return "posInSet";
    case ax::mojom::IntAttribute::kColorValue:
      return "colorValue";
    case ax::mojom::IntAttribute::kAriaCurrentState:
      return "ariaCurrentState";
    case ax::mojom::IntAttribute::kBackgroundColor:
      return "backgroundColor";
    case ax::mojom::IntAttribute::kColor:
      return "color";
    case ax::mojom::IntAttribute::kHasPopup:
      return "haspopup";
    case ax::mojom::IntAttribute::kInvalidState:
      return "invalidState";
    case ax::mojom::IntAttribute::kCheckedState:
      return "checkedState";
    case ax::mojom::IntAttribute::kListStyle:
      return "listStyle";
    case ax::mojom::IntAttribute::kTextAlign:
      return "text-align";
    case ax::mojom::IntAttribute::kTextDirection:
      return "textDirection";
    case ax::mojom::IntAttribute::kTextPosition:
      return "textPosition";
    case ax::mojom::IntAttribute::kTextStyle:
      return "textStyle";
    case ax::mojom::IntAttribute::kTextOverlineStyle:
      return "textOverlineStyle";
    case ax::mojom::IntAttribute::kTextStrikethroughStyle:
      return "textStrikethroughStyle";
    case ax::mojom::IntAttribute::kTextUnderlineStyle:
      return "textUnderlineStyle";
    case ax::mojom::IntAttribute::kPreviousFocusId:
      return "previousFocusId";
    case ax::mojom::IntAttribute::kNextFocusId:
      return "nextFocusId";
    case ax::mojom::IntAttribute::kImageAnnotationStatus:
      return "imageAnnotationStatus";
    case ax::mojom::IntAttribute::kDOMNodeId:
      return "domNodeId";
  }

  return "";
}

const char* ToString(ax::mojom::FloatAttribute float_attribute) {
  switch (float_attribute) {
    case ax::mojom::FloatAttribute::kNone:
      return "none";
    case ax::mojom::FloatAttribute::kValueForRange:
      return "valueForRange";
    case ax::mojom::FloatAttribute::kMinValueForRange:
      return "minValueForRange";
    case ax::mojom::FloatAttribute::kMaxValueForRange:
      return "maxValueForRange";
    case ax::mojom::FloatAttribute::kStepValueForRange:
      return "stepValueForRange";
    case ax::mojom::FloatAttribute::kFontSize:
      return "fontSize";
    case ax::mojom::FloatAttribute::kFontWeight:
      return "fontWeight";
    case ax::mojom::FloatAttribute::kTextIndent:
      return "textIndent";
  }

  return "";
}

const char* ToString(ax::mojom::BoolAttribute bool_attribute) {
  switch (bool_attribute) {
    case ax::mojom::BoolAttribute::kNone:
      return "none";
    case ax::mojom::BoolAttribute::kBusy:
      return "busy";
    case ax::mojom::BoolAttribute::kEditableRoot:
      return "editableRoot";
    case ax::mojom::BoolAttribute::kContainerLiveAtomic:
      return "containerLiveAtomic";
    case ax::mojom::BoolAttribute::kContainerLiveBusy:
      return "containerLiveBusy";
    case ax::mojom::BoolAttribute::kGrabbed:
      return "grabbed";
    case ax::mojom::BoolAttribute::kLiveAtomic:
      return "liveAtomic";
    case ax::mojom::BoolAttribute::kModal:
      return "modal";
    case ax::mojom::BoolAttribute::kUpdateLocationOnly:
      return "updateLocationOnly";
    case ax::mojom::BoolAttribute::kCanvasHasFallback:
      return "canvasHasFallback";
    case ax::mojom::BoolAttribute::kScrollable:
      return "scrollable";
    case ax::mojom::BoolAttribute::kClickable:
      return "clickable";
    case ax::mojom::BoolAttribute::kClipsChildren:
      return "clipsChildren";
    case ax::mojom::BoolAttribute::kNotUserSelectableStyle:
      return "notUserSelectableStyle";
    case ax::mojom::BoolAttribute::kSelected:
      return "selected";
    case ax::mojom::BoolAttribute::kSelectedFromFocus:
      return "selectedFromFocus";
    case ax::mojom::BoolAttribute::kSupportsTextLocation:
      return "supportsTextLocation";
    case ax::mojom::BoolAttribute::kIsLineBreakingObject:
      return "isLineBreakingObject";
    case ax::mojom::BoolAttribute::kIsPageBreakingObject:
      return "isPageBreakingObject";
    case ax::mojom::BoolAttribute::kHasAriaAttribute:
      return "hasAriaAttribute";
    case ax::mojom::BoolAttribute::kTouchPassthrough:
      return "touchPassthrough";
  }

  return "";
}

const char* ToString(ax::mojom::IntListAttribute int_list_attribute) {
  switch (int_list_attribute) {
    case ax::mojom::IntListAttribute::kNone:
      return "none";
    case ax::mojom::IntListAttribute::kIndirectChildIds:
      return "indirectChildIds";
    case ax::mojom::IntListAttribute::kControlsIds:
      return "controlsIds";
    case ax::mojom::IntListAttribute::kDetailsIds:
      return "detailsIds";
    case ax::mojom::IntListAttribute::kDescribedbyIds:
      return "describedbyIds";
    case ax::mojom::IntListAttribute::kFlowtoIds:
      return "flowtoIds";
    case ax::mojom::IntListAttribute::kLabelledbyIds:
      return "labelledbyIds";
    case ax::mojom::IntListAttribute::kRadioGroupIds:
      return "radioGroupIds";
    case ax::mojom::IntListAttribute::kMarkerTypes:
      return "markerTypes";
    case ax::mojom::IntListAttribute::kMarkerStarts:
      return "markerStarts";
    case ax::mojom::IntListAttribute::kMarkerEnds:
      return "markerEnds";
    case ax::mojom::IntListAttribute::kCharacterOffsets:
      return "characterOffsets";
    case ax::mojom::IntListAttribute::kCachedLineStarts:
      return "cachedLineStarts";
    case ax::mojom::IntListAttribute::kWordStarts:
      return "wordStarts";
    case ax::mojom::IntListAttribute::kWordEnds:
      return "wordEnds";
    case ax::mojom::IntListAttribute::kCustomActionIds:
      return "customActionIds";
  }

  return "";
}

const char* ToString(ax::mojom::StringListAttribute string_list_attribute) {
  switch (string_list_attribute) {
    case ax::mojom::StringListAttribute::kNone:
      return "none";
    case ax::mojom::StringListAttribute::kCustomActionDescriptions:
      return "customActionDescriptions";
  }

  return "";
}

const char* ToString(ax::mojom::ListStyle list_style) {
  switch (list_style) {
    case ax::mojom::ListStyle::kNone:
      return "none";
    case ax::mojom::ListStyle::kCircle:
      return "circle";
    case ax::mojom::ListStyle::kDisc:
      return "disc";
    case ax::mojom::ListStyle::kImage:
      return "image";
    case ax::mojom::ListStyle::kNumeric:
      return "numeric";
    case ax::mojom::ListStyle::kOther:
      return "other";
    case ax::mojom::ListStyle::kSquare:
      return "square";
  }

  return "";
}

const char* ToString(ax::mojom::MarkerType marker_type) {
  switch (marker_type) {
    case ax::mojom::MarkerType::kNone:
      return "none";
    case ax::mojom::MarkerType::kSpelling:
      return "spelling";
    case ax::mojom::MarkerType::kGrammar:
      return "grammar";
    case ax::mojom::MarkerType::kTextMatch:
      return "textMatch";
    case ax::mojom::MarkerType::kActiveSuggestion:
      return "activeSuggestion";
    case ax::mojom::MarkerType::kSuggestion:
      return "suggestion";
  }

  return "";
}

const char* ToString(ax::mojom::MoveDirection move_direction) {
  switch (move_direction) {
    case ax::mojom::MoveDirection::kNone:
      return "none";
    case ax::mojom::MoveDirection::kBackward:
      return "backward";
    case ax::mojom::MoveDirection::kForward:
      return "forward";
  }

  return "";
}

const char* ToString(ax::mojom::Command command) {
  switch (command) {
    case ax::mojom::Command::kNone:
      return "none";
    case ax::mojom::Command::kClearSelection:
      return "clearSelection";
    case ax::mojom::Command::kDelete:
      return "delete";
    case ax::mojom::Command::kDictate:
      return "dictate";
    case ax::mojom::Command::kExtendSelection:
      return "extendSelection";
    case ax::mojom::Command::kFormat:
      return "format";
    case ax::mojom::Command::kHistory:
      return "history";
    case ax::mojom::Command::kInsert:
      return "insert";
    case ax::mojom::Command::kMarker:
      return "marker";
    case ax::mojom::Command::kMoveSelection:
      return "moveSelection";
    case ax::mojom::Command::kSetSelection:
      return "setSelection";
  }

  return "";
}

const char* ToString(ax::mojom::InputEventType input_event_type) {
  switch (input_event_type) {
    case ax::mojom::InputEventType::kNone:
      return "none";
    case ax::mojom::InputEventType::kInsertText:
      return "insertText";
    case ax::mojom::InputEventType::kInsertLineBreak:
      return "insertLineBreak";
    case ax::mojom::InputEventType::kInsertParagraph:
      return "insertParagraph";
    case ax::mojom::InputEventType::kInsertOrderedList:
      return "insertOrderedList";
    case ax::mojom::InputEventType::kInsertUnorderedList:
      return "insertUnorderedList";
    case ax::mojom::InputEventType::kInsertHorizontalRule:
      return "insertHorizontalRule";
    case ax::mojom::InputEventType::kInsertFromPaste:
      return "insertFromPaste";
    case ax::mojom::InputEventType::kInsertFromDrop:
      return "insertFromDrop";
    case ax::mojom::InputEventType::kInsertFromYank:
      return "insertFromYank";
    case ax::mojom::InputEventType::kInsertTranspose:
      return "insertTranspose";
    case ax::mojom::InputEventType::kInsertReplacementText:
      return "insertReplacementText";
    case ax::mojom::InputEventType::kInsertCompositionText:
      return "insertCompositionText";
    case ax::mojom::InputEventType::kDeleteWordBackward:
      return "deleteWordBackward";
    case ax::mojom::InputEventType::kDeleteWordForward:
      return "deleteWordForward";
    case ax::mojom::InputEventType::kDeleteSoftLineBackward:
      return "deleteSoftLineBackward";
    case ax::mojom::InputEventType::kDeleteSoftLineForward:
      return "deleteSoftLineForward";
    case ax::mojom::InputEventType::kDeleteHardLineBackward:
      return "deleteHardLineBackward";
    case ax::mojom::InputEventType::kDeleteHardLineForward:
      return "deleteHardLineForward";
    case ax::mojom::InputEventType::kDeleteContentBackward:
      return "deleteContentBackward";
    case ax::mojom::InputEventType::kDeleteContentForward:
      return "deleteContentForward";
    case ax::mojom::InputEventType::kDeleteByCut:
      return "deleteByCut";
    case ax::mojom::InputEventType::kDeleteByDrag:
      return "deleteByDrag";
    case ax::mojom::InputEventType::kHistoryUndo:
      return "historyUndo";
    case ax::mojom::InputEventType::kHistoryRedo:
      return "historyRedo";
    case ax::mojom::InputEventType::kFormatBold:
      return "formatBold";
    case ax::mojom::InputEventType::kFormatItalic:
      return "formatItalic";
    case ax::mojom::InputEventType::kFormatUnderline:
      return "formatUnderline";
    case ax::mojom::InputEventType::kFormatStrikeThrough:
      return "formatStrikeThrough";
    case ax::mojom::InputEventType::kFormatSuperscript:
      return "formatSuperscript";
    case ax::mojom::InputEventType::kFormatSubscript:
      return "formatSubscript";
    case ax::mojom::InputEventType::kFormatJustifyCenter:
      return "formatJustifyCenter";
    case ax::mojom::InputEventType::kFormatJustifyFull:
      return "formatJustifyFull";
    case ax::mojom::InputEventType::kFormatJustifyRight:
      return "formatJustifyRight";
    case ax::mojom::InputEventType::kFormatJustifyLeft:
      return "formatJustifyLeft";
    case ax::mojom::InputEventType::kFormatIndent:
      return "formatIndent";
    case ax::mojom::InputEventType::kFormatOutdent:
      return "formatOutdent";
    case ax::mojom::InputEventType::kFormatRemove:
      return "formatRemove";
    case ax::mojom::InputEventType::kFormatSetBlockTextDirection:
      return "formatSetBlockTextDirection";
  }

  return "";
}

const char* ToString(ax::mojom::TextBoundary text_boundary) {
  switch (text_boundary) {
    case ax::mojom::TextBoundary::kNone:
      return "none";
    case ax::mojom::TextBoundary::kCharacter:
      return "character";
    case ax::mojom::TextBoundary::kFormat:
      return "format";
    case ax::mojom::TextBoundary::kLineEnd:
      return "lineEnd";
    case ax::mojom::TextBoundary::kLineStart:
      return "lineStart";
    case ax::mojom::TextBoundary::kLineStartOrEnd:
      return "lineStartOrEnd";
    case ax::mojom::TextBoundary::kObject:
      return "object";
    case ax::mojom::TextBoundary::kPageEnd:
      return "pageEnd";
    case ax::mojom::TextBoundary::kPageStart:
      return "pageStart";
    case ax::mojom::TextBoundary::kPageStartOrEnd:
      return "pageStartOrEnd";
    case ax::mojom::TextBoundary::kParagraphEnd:
      return "paragraphEnd";
    case ax::mojom::TextBoundary::kParagraphStart:
      return "paragraphStart";
    case ax::mojom::TextBoundary::kParagraphStartOrEnd:
      return "paragraphStartOrEnd";
    case ax::mojom::TextBoundary::kSentenceEnd:
      return "sentenceEnd";
    case ax::mojom::TextBoundary::kSentenceStart:
      return "sentenceStart";
    case ax::mojom::TextBoundary::kSentenceStartOrEnd:
      return "sentenceStartOrEnd";
    case ax::mojom::TextBoundary::kWebPage:
      return "webPage";
    case ax::mojom::TextBoundary::kWordEnd:
      return "wordEnd";
    case ax::mojom::TextBoundary::kWordStart:
      return "wordStart";
    case ax::mojom::TextBoundary::kWordStartOrEnd:
      return "wordStartOrEnd";
  }

  return "";
}

const char* ToString(ax::mojom::TextAlign text_align) {
  switch (text_align) {
    case ax::mojom::TextAlign::kNone:
      return "none";
    case ax::mojom::TextAlign::kLeft:
      return "left";
    case ax::mojom::TextAlign::kRight:
      return "right";
    case ax::mojom::TextAlign::kCenter:
      return "center";
    case ax::mojom::TextAlign::kJustify:
      return "justify";
  }

  return "";
}

const char* ToString(ax::mojom::WritingDirection writing_direction) {
  switch (writing_direction) {
    case ax::mojom::WritingDirection::kNone:
      return "none";
    case ax::mojom::WritingDirection::kLtr:
      return "ltr";
    case ax::mojom::WritingDirection::kRtl:
      return "rtl";
    case ax::mojom::WritingDirection::kTtb:
      return "ttb";
    case ax::mojom::WritingDirection::kBtt:
      return "btt";
  }

  return "";
}

const char* ToString(ax::mojom::TextPosition text_position) {
  switch (text_position) {
    case ax::mojom::TextPosition::kNone:
      return "none";
    case ax::mojom::TextPosition::kSubscript:
      return "subscript";
    case ax::mojom::TextPosition::kSuperscript:
      return "superscript";
  }

  return "";
}

const char* ToString(ax::mojom::TextStyle text_style) {
  switch (text_style) {
    case ax::mojom::TextStyle::kNone:
      return "none";
    case ax::mojom::TextStyle::kBold:
      return "bold";
    case ax::mojom::TextStyle::kItalic:
      return "italic";
    case ax::mojom::TextStyle::kUnderline:
      return "underline";
    case ax::mojom::TextStyle::kLineThrough:
      return "lineThrough";
    case ax::mojom::TextStyle::kOverline:
      return "overline";
  }

  return "";
}

const char* ToString(ax::mojom::TextDecorationStyle text_decoration_style) {
  switch (text_decoration_style) {
    case ax::mojom::TextDecorationStyle::kNone:
      return "none";
    case ax::mojom::TextDecorationStyle::kSolid:
      return "solid";
    case ax::mojom::TextDecorationStyle::kDashed:
      return "dashed";
    case ax::mojom::TextDecorationStyle::kDotted:
      return "dotted";
    case ax::mojom::TextDecorationStyle::kDouble:
      return "double";
    case ax::mojom::TextDecorationStyle::kWavy:
      return "wavy";
  }

  return "";
}

const char* ToString(ax::mojom::AriaCurrentState aria_current_state) {
  switch (aria_current_state) {
    case ax::mojom::AriaCurrentState::kNone:
      return "none";
    case ax::mojom::AriaCurrentState::kFalse:
      return "false";
    case ax::mojom::AriaCurrentState::kTrue:
      return "true";
    case ax::mojom::AriaCurrentState::kPage:
      return "page";
    case ax::mojom::AriaCurrentState::kStep:
      return "step";
    case ax::mojom::AriaCurrentState::kLocation:
      return "location";
    case ax::mojom::AriaCurrentState::kDate:
      return "date";
    case ax::mojom::AriaCurrentState::kTime:
      return "time";
  }

  return "";
}

const char* ToString(ax::mojom::HasPopup has_popup) {
  switch (has_popup) {
    case ax::mojom::HasPopup::kFalse:
      return "";
    case ax::mojom::HasPopup::kTrue:
      return "true";
    case ax::mojom::HasPopup::kMenu:
      return "menu";
    case ax::mojom::HasPopup::kListbox:
      return "listbox";
    case ax::mojom::HasPopup::kTree:
      return "tree";
    case ax::mojom::HasPopup::kGrid:
      return "grid";
    case ax::mojom::HasPopup::kDialog:
      return "dialog";
  }

  return "";
}

const char* ToString(ax::mojom::InvalidState invalid_state) {
  switch (invalid_state) {
    case ax::mojom::InvalidState::kNone:
      return "none";
    case ax::mojom::InvalidState::kFalse:
      return "false";
    case ax::mojom::InvalidState::kTrue:
      return "true";
    case ax::mojom::InvalidState::kOther:
      return "other";
  }

  return "";
}

const char* ToString(ax::mojom::Restriction restriction) {
  switch (restriction) {
    case ax::mojom::Restriction::kNone:
      return "none";
    case ax::mojom::Restriction::kReadOnly:
      return "readOnly";
    case ax::mojom::Restriction::kDisabled:
      return "disabled";
  }

  return "";
}

const char* ToString(ax::mojom::CheckedState checked_state) {
  switch (checked_state) {
    case ax::mojom::CheckedState::kNone:
      return "none";
    case ax::mojom::CheckedState::kFalse:
      return "false";
    case ax::mojom::CheckedState::kTrue:
      return "true";
    case ax::mojom::CheckedState::kMixed:
      return "mixed";
  }

  return "";
}

const char* ToString(ax::mojom::SortDirection sort_direction) {
  switch (sort_direction) {
    case ax::mojom::SortDirection::kNone:
      return "none";
    case ax::mojom::SortDirection::kUnsorted:
      return "unsorted";
    case ax::mojom::SortDirection::kAscending:
      return "ascending";
    case ax::mojom::SortDirection::kDescending:
      return "descending";
    case ax::mojom::SortDirection::kOther:
      return "other";
  }

  return "";
}

const char* ToString(ax::mojom::NameFrom name_from) {
  switch (name_from) {
    case ax::mojom::NameFrom::kNone:
      return "none";
    case ax::mojom::NameFrom::kUninitialized:
      return "uninitialized";
    case ax::mojom::NameFrom::kAttribute:
      return "attribute";
    case ax::mojom::NameFrom::kAttributeExplicitlyEmpty:
      return "attributeExplicitlyEmpty";
    case ax::mojom::NameFrom::kCaption:
      return "caption";
    case ax::mojom::NameFrom::kContents:
      return "contents";
    case ax::mojom::NameFrom::kPlaceholder:
      return "placeholder";
    case ax::mojom::NameFrom::kRelatedElement:
      return "relatedElement";
    case ax::mojom::NameFrom::kTitle:
      return "title";
    case ax::mojom::NameFrom::kValue:
      return "value";
  }

  return "";
}

const char* ToString(ax::mojom::DescriptionFrom description_from) {
  switch (description_from) {
    case ax::mojom::DescriptionFrom::kNone:
      return "none";
    case ax::mojom::DescriptionFrom::kUninitialized:
      return "uninitialized";
    case ax::mojom::DescriptionFrom::kAttribute:
      return "attribute";
    case ax::mojom::DescriptionFrom::kContents:
      return "contents";
    case ax::mojom::DescriptionFrom::kRelatedElement:
      return "relatedElement";
    case ax::mojom::DescriptionFrom::kTitle:
      return "title";
  }

  return "";
}

const char* ToString(ax::mojom::EventFrom event_from) {
  switch (event_from) {
    case ax::mojom::EventFrom::kNone:
      return "none";
    case ax::mojom::EventFrom::kUser:
      return "user";
    case ax::mojom::EventFrom::kPage:
      return "page";
    case ax::mojom::EventFrom::kAction:
      return "action";
  }

  return "";
}

const char* ToString(ax::mojom::Gesture gesture) {
  switch (gesture) {
    case ax::mojom::Gesture::kNone:
      return "none";
    case ax::mojom::Gesture::kClick:
      return "click";
    case ax::mojom::Gesture::kSwipeLeft1:
      return "swipeLeft1";
    case ax::mojom::Gesture::kSwipeUp1:
      return "swipeUp1";
    case ax::mojom::Gesture::kSwipeRight1:
      return "swipeRight1";
    case ax::mojom::Gesture::kSwipeDown1:
      return "swipeDown1";
    case ax::mojom::Gesture::kSwipeLeft2:
      return "swipeLeft2";
    case ax::mojom::Gesture::kSwipeUp2:
      return "swipeUp2";
    case ax::mojom::Gesture::kSwipeRight2:
      return "swipeRight2";
    case ax::mojom::Gesture::kSwipeDown2:
      return "swipeDown2";
    case ax::mojom::Gesture::kSwipeLeft3:
      return "swipeLeft3";
    case ax::mojom::Gesture::kSwipeUp3:
      return "swipeUp3";
    case ax::mojom::Gesture::kSwipeRight3:
      return "swipeRight3";
    case ax::mojom::Gesture::kSwipeDown3:
      return "swipeDown3";
    case ax::mojom::Gesture::kSwipeLeft4:
      return "swipeLeft4";
    case ax::mojom::Gesture::kSwipeUp4:
      return "swipeUp4";
    case ax::mojom::Gesture::kSwipeRight4:
      return "swipeRight4";
    case ax::mojom::Gesture::kSwipeDown4:
      return "swipeDown4";
    case ax::mojom::Gesture::kTap2:
      return "tap2";
    case ax::mojom::Gesture::kTap3:
      return "tap3";
    case ax::mojom::Gesture::kTap4:
      return "tap4";
    case ax::mojom::Gesture::kTouchExplore:
      return "touchExplore";
  }

  return "";
}

const char* ToString(ax::mojom::TextAffinity text_affinity) {
  switch (text_affinity) {
    case ax::mojom::TextAffinity::kNone:
      return "none";
    case ax::mojom::TextAffinity::kDownstream:
      return "downstream";
    case ax::mojom::TextAffinity::kUpstream:
      return "upstream";
  }

  return "";
}

const char* ToString(ax::mojom::TreeOrder tree_order) {
  switch (tree_order) {
    case ax::mojom::TreeOrder::kNone:
      return "none";
    case ax::mojom::TreeOrder::kUndefined:
      return "undefined";
    case ax::mojom::TreeOrder::kBefore:
      return "before";
    case ax::mojom::TreeOrder::kEqual:
      return "equal";
    case ax::mojom::TreeOrder::kAfter:
      return "after";
  }

  return "";
}

const char* ToString(ax::mojom::ImageAnnotationStatus status) {
  switch (status) {
    case ax::mojom::ImageAnnotationStatus::kNone:
      return "none";
    case ax::mojom::ImageAnnotationStatus::kWillNotAnnotateDueToScheme:
      return "kWillNotAnnotateDueToScheme";
    case ax::mojom::ImageAnnotationStatus::kIneligibleForAnnotation:
      return "ineligibleForAnnotation";
    case ax::mojom::ImageAnnotationStatus::kEligibleForAnnotation:
      return "eligibleForAnnotation";
    case ax::mojom::ImageAnnotationStatus::kSilentlyEligibleForAnnotation:
      return "silentlyEligibleForAnnotation";
    case ax::mojom::ImageAnnotationStatus::kAnnotationPending:
      return "annotationPending";
    case ax::mojom::ImageAnnotationStatus::kAnnotationSucceeded:
      return "annotationSucceeded";
    case ax::mojom::ImageAnnotationStatus::kAnnotationEmpty:
      return "annotationEmpty";
    case ax::mojom::ImageAnnotationStatus::kAnnotationAdult:
      return "annotationAdult";
    case ax::mojom::ImageAnnotationStatus::kAnnotationProcessFailed:
      return "annotationProcessFailed";
  }

  return "";
}

const char* ToString(ax::mojom::Dropeffect dropeffect) {
  switch (dropeffect) {
    case ax::mojom::Dropeffect::kCopy:
      return "copy";
    case ax::mojom::Dropeffect::kExecute:
      return "execute";
    case ax::mojom::Dropeffect::kLink:
      return "link";
    case ax::mojom::Dropeffect::kMove:
      return "move";
    case ax::mojom::Dropeffect::kPopup:
      return "popup";
    case ax::mojom::Dropeffect::kNone:
      return "none";
  }

  return "";
}

}  // namespace ui
