// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/login/ui/login_user_menu_view.h"
#include "ash/login/ui/non_accessible_view.h"
#include "ash/login/ui/system_label_button.h"
#include "ash/login/ui/views_utils.h"
#include "ash/public/cpp/login_types.h"
#include "ash/public/cpp/shelf_config.h"
#include "ash/strings/grit/ash_strings.h"
#include "base/strings/strcat.h"
#include "base/strings/utf_string_conversions.h"
#include "chromeos/strings/grit/chromeos_strings.h"
#include "ui/accessibility/ax_node_data.h"
#include "ui/base/l10n/l10n_util.h"
#include "ui/display/display.h"
#include "ui/display/screen.h"
#include "ui/views/accessibility/view_accessibility.h"
#include "ui/views/layout/box_layout.h"
#include "ui/views/layout/fill_layout.h"

namespace ash {
namespace {
// Vertical margin between username and mail.
constexpr int kUserMenuVerticalMarginUsernameMailDp = 8;

// Vertical margin between labels.
constexpr int kUserMenuVerticalMarginBetweenLabelsDp = 16;

// Horizontal and vertical padding of login user menu view.
constexpr int kHorizontalPaddingLoginUserMenuViewDp = 8;
constexpr int kVerticalPaddingLoginUserMenuViewDp = 8;

constexpr int kUserMenuRemoveUserButtonIdForTest = 1;

// Font name of the username headline.
constexpr char kUserMenuFontNameUsername[] = "Google Sans";

// Font size of the username headline.
constexpr int kUserMenuFontSizeUsername = 15;

// Line height of the username headline.
constexpr int kUserMenuLineHeightUsername = 22;

// Traps the focus so it does not move from the |trapped_focus| view.
class TrappedFocusSearch : public views::FocusSearch {
 public:
  explicit TrappedFocusSearch(views::View* trapped_focus)
      : FocusSearch(trapped_focus->parent(), true, true),
        trapped_focus_(trapped_focus) {}
  TrappedFocusSearch(const TrappedFocusSearch&) = delete;
  TrappedFocusSearch& operator=(const TrappedFocusSearch&) = delete;
  ~TrappedFocusSearch() override = default;

  // views::FocusSearch:
  views::View* FindNextFocusableView(
      views::View* starting_view,
      views::FocusSearch::SearchDirection search_direction,
      views::FocusSearch::TraversalDirection traversal_direction,
      views::FocusSearch::StartingViewPolicy check_starting_view,
      views::FocusSearch::AnchoredDialogPolicy can_go_into_anchored_dialog,
      views::FocusTraversable** focus_traversable,
      views::View** focus_traversable_view) override {
    return trapped_focus_;
  }

 private:
  views::View* const trapped_focus_;
};

}  // namespace

// A button that holds a child view.
class RemoveUserButton : public SystemLabelButton {
 public:
  RemoveUserButton(PressedCallback callback, LoginUserMenuView* bubble)
      : SystemLabelButton(std::move(callback),
                          l10n_util::GetStringUTF16(
                              IDS_ASH_LOGIN_POD_REMOVE_ACCOUNT_ACCESSIBLE_NAME),
                          SystemLabelButton::DisplayType::DEFAULT,
                          /*multiline*/ true),
        bubble_(bubble) {}

  ~RemoveUserButton() override = default;

 private:
  void OnKeyEvent(ui::KeyEvent* event) override {
    if (event->type() != ui::ET_KEY_PRESSED ||
        event->key_code() == ui::VKEY_PROCESSKEY) {
      return;
    }

    if (event->key_code() == ui::VKEY_ESCAPE) {
      bubble_->Hide();
      // We explicitly move focus back to the dropdown button so the Tab
      // traversal works correctly.
      bubble_->GetBubbleOpener()->RequestFocus();
    }

    if (event->key_code() == ui::VKEY_RETURN)
      views::Button::OnKeyEvent(event);
  }

  LoginUserMenuView* bubble_;

  DISALLOW_COPY_AND_ASSIGN(RemoveUserButton);
};

LoginUserMenuView::TestApi::TestApi(LoginUserMenuView* bubble)
    : bubble_(bubble) {}

views::View* LoginUserMenuView::TestApi::remove_user_button() {
  return bubble_->remove_user_button_;
}

views::View* LoginUserMenuView::TestApi::remove_user_confirm_data() {
  return bubble_->remove_user_confirm_data_;
}

views::View* LoginUserMenuView::TestApi::managed_user_data() {
  return bubble_->managed_user_data_;
}

views::Label* LoginUserMenuView::TestApi::username_label() {
  return bubble_->username_label_;
}

views::Label* LoginUserMenuView::TestApi::management_disclosure_label() {
  return bubble_->management_disclosure_label_;
}

LoginUserMenuView::LoginUserMenuView(
    const LoginUserInfo& user,
    views::View* anchor_view,
    LoginButton* bubble_opener,
    base::RepeatingClosure on_remove_user_warning_shown,
    base::RepeatingClosure on_remove_user_requested)
    : LoginBaseBubbleView(anchor_view),
      bubble_opener_(bubble_opener),
      on_remove_user_warning_shown_(on_remove_user_warning_shown),
      on_remove_user_requested_(on_remove_user_requested) {
  const std::u16string& email =
      base::UTF8ToUTF16(user.basic_user_info.display_email);
  bool is_owner = user.is_device_owner;

  // User information.
  {
    const std::u16string& username =
        base::UTF8ToUTF16(user.basic_user_info.display_name);
    std::u16string display_username =
        is_owner
            ? l10n_util::GetStringFUTF16(IDS_ASH_LOGIN_POD_OWNER_USER, username)
            : username;

    views::View* container =
        new NonAccessibleView("UsernameLabel MarginContainer");
    container->SetLayoutManager(std::make_unique<views::BoxLayout>(
        views::BoxLayout::Orientation::kVertical, gfx::Insets(),
        kUserMenuVerticalMarginUsernameMailDp));
    AddChildView(container);
    // Colors should be updated in OnThemeChanged.
    username_label_ =
        container->AddChildView(login_views_utils::CreateBubbleLabel(
            display_username, nullptr, SK_ColorGREEN,
            gfx::FontList({kUserMenuFontNameUsername},
                          gfx::Font::FontStyle::NORMAL,
                          kUserMenuFontSizeUsername, gfx::Font::Weight::MEDIUM),
            kUserMenuLineHeightUsername));
    email_label_ =
        container->AddChildView(login_views_utils::CreateBubbleLabel(email));
  }

  // User is managed.
  if (user.user_account_manager) {
    managed_user_data_ = new views::View();
    managed_user_data_->SetLayoutManager(std::make_unique<views::BoxLayout>(
        views::BoxLayout::Orientation::kVertical));
    std::u16string managed_text = l10n_util::GetStringFUTF16(
        IDS_ASH_LOGIN_MANAGED_SESSION_MONITORING_USER_WARNING,
        base::UTF8ToUTF16(user.user_account_manager.value()));
    management_disclosure_label_ =
        login_views_utils::CreateBubbleLabel(managed_text, this);
    managed_user_data_->AddChildView(management_disclosure_label_);
    AddChildView(managed_user_data_);
  }

  // If we can remove the user, the focus will be trapped by the bubble, and
  // button. If we can't, and there is no button, we set this so that the bubble
  // accessible data is displayed by accessibility tools.
  set_notify_alert_on_show(!user.can_remove);

  // Remove user.
  if (user.can_remove) {
    DCHECK(!is_owner);
    user_manager::UserType type = user.basic_user_info.type;
    std::u16string part1 = l10n_util::GetStringUTF16(
        IDS_ASH_LOGIN_POD_NON_OWNER_USER_REMOVE_WARNING_PART_1);
    std::u16string part2 = l10n_util::GetStringFUTF16(
        type == user_manager::UserType::USER_TYPE_CHILD
            ? IDS_ASH_LOGIN_POD_NON_OWNER_USER_REMOVE_WARNING_PART_2_SUPERVISED_USER
            : IDS_ASH_LOGIN_POD_NON_OWNER_USER_REMOVE_WARNING_PART_2,
        email);
    warning_message_ = base::StrCat({part1, u" ", part2});

    remove_user_confirm_data_ = AddChildView(std::make_unique<views::View>());
    remove_user_confirm_data_->SetLayoutManager(
        std::make_unique<views::BoxLayout>(
            views::BoxLayout::Orientation::kVertical, gfx::Insets(),
            kUserMenuVerticalMarginBetweenLabelsDp));
    remove_user_confirm_data_->SetVisible(false);

    remove_user_confirm_data_->AddChildView(
        login_views_utils::CreateBubbleLabel(part1, this));

    remove_user_confirm_data_->AddChildView(
        login_views_utils::CreateBubbleLabel(part2, this));

    remove_user_button_ = new RemoveUserButton(
        base::BindRepeating(&LoginUserMenuView::RemoveUserButtonPressed,
                            base::Unretained(this)),
        this);
    remove_user_button_->SetID(kUserMenuRemoveUserButtonIdForTest);
    AddChildView(remove_user_button_);

    // Traps the focus on the remove user button.
    focus_search_ = std::make_unique<TrappedFocusSearch>(remove_user_button_);
  }

  set_positioning_strategy(PositioningStrategy::kTryAfterThenBefore);
  SetPadding(kHorizontalPaddingLoginUserMenuViewDp,
             kVerticalPaddingLoginUserMenuViewDp);
}

LoginUserMenuView::~LoginUserMenuView() = default;

void LoginUserMenuView::ResetState() {
  if (managed_user_data_)
    managed_user_data_->SetVisible(true);
  if (remove_user_confirm_data_) {
    remove_user_confirm_data_->SetVisible(false);
    remove_user_button_->SetDisplayType(
        SystemLabelButton::DisplayType::DEFAULT);
    // Reset button's description to none.
    remove_user_button_->GetViewAccessibility().OverrideDescription(
        std::u16string());
  }
}

LoginButton* LoginUserMenuView::GetBubbleOpener() const {
  return bubble_opener_;
}

void LoginUserMenuView::RequestFocus() {
  // This view has no actual interesting contents to focus, so immediately
  // forward to the button.
  if (remove_user_button_)
    remove_user_button_->RequestFocus();
}

bool LoginUserMenuView::HasFocus() const {
  return remove_user_button_ && remove_user_button_->HasFocus();
}

const char* LoginUserMenuView::GetClassName() const {
  return "LoginUserMenuView";
}

void LoginUserMenuView::GetAccessibleNodeData(ui::AXNodeData* node_data) {
  if (remove_user_button_) {
    node_data->SetName(l10n_util::GetStringUTF16(
        IDS_ASH_LOGIN_POD_REMOVE_ACCOUNT_ACCESSIBLE_NAME));
    node_data->SetDescription(l10n_util::GetStringUTF16(
        IDS_ASH_LOGIN_POD_REMOVE_ACCOUNT_DIALOG_ACCESSIBLE_DESCRIPTION));
  } else {
    node_data->SetName(username_label_->GetText());
    if (management_disclosure_label_) {
      node_data->SetDescription(
          base::StrCat({email_label_->GetText(), u" ",
                        management_disclosure_label_->GetText()}));
    } else {
      node_data->SetDescription(email_label_->GetText());
    }
  }
  node_data->role = ax::mojom::Role::kDialog;
  node_data->AddBoolAttribute(ax::mojom::BoolAttribute::kModal, true);
}

void LoginUserMenuView::OnThemeChanged() {
  views::View::OnThemeChanged();
  username_label_->SetEnabledColor(
      AshColorProvider::Get()->GetContentLayerColor(
          AshColorProvider::ContentLayerType::kTextColorPrimary));
  email_label_->SetEnabledColor(AshColorProvider::Get()->GetContentLayerColor(
      AshColorProvider::ContentLayerType::kTextColorSecondary));
  if (remove_user_confirm_data_) {
    DCHECK_EQ(2u, remove_user_confirm_data_->children().size());
    for (views::View* label : remove_user_confirm_data_->children()) {
      static_cast<views::Label*>(label)->SetEnabledColor(
          AshColorProvider::Get()->GetContentLayerColor(
              AshColorProvider::ContentLayerType::kTextColorPrimary));
    }
  }
}

views::FocusTraversable* LoginUserMenuView::GetPaneFocusTraversable() {
  return this;
}

views::FocusSearch* LoginUserMenuView::GetFocusSearch() {
  return focus_search_.get();
}

views::FocusTraversable* LoginUserMenuView::GetFocusTraversableParent() {
  return nullptr;
}

views::View* LoginUserMenuView::GetFocusTraversableParentView() {
  return nullptr;
}

void LoginUserMenuView::RemoveUserButtonPressed() {
  // Show confirmation warning. The user has to click the button again before
  // we actually allow the exit.
  if (!remove_user_confirm_data_->GetVisible()) {
    remove_user_confirm_data_->SetVisible(true);
    if (managed_user_data_)
      managed_user_data_->SetVisible(false);
    remove_user_button_->SetDisplayType(
        SystemLabelButton::DisplayType::ALERT_NO_ICON);

    Layout();

    // Change the node's description to force assistive technologies, like
    // ChromeVox, to report the updated description.
    remove_user_button_->GetViewAccessibility().OverrideDescription(
        warning_message_);
    if (on_remove_user_warning_shown_)
      std::move(on_remove_user_warning_shown_).Run();
    return;
  }

  // Immediately hide the bubble with no animation before running the remove
  // user callback. If an animation is triggered while the the views hierarchy
  // for this bubble is being torn down, we can get a crash.
  SetVisible(false);

  if (on_remove_user_requested_)
    std::move(on_remove_user_requested_).Run();
}

}  // namespace ash
