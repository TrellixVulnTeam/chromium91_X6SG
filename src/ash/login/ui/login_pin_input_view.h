// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ASH_LOGIN_UI_LOGIN_PIN_INPUT_VIEW_H_
#define ASH_LOGIN_UI_LOGIN_PIN_INPUT_VIEW_H_

#include "ash/ash_export.h"
#include "ash/login/ui/access_code_input.h"
#include "ash/login/ui/login_palette.h"
#include "ash/login/ui/non_accessible_view.h"
#include "ui/views/view.h"

namespace ash {

class LoginPinInput;

// LoginPinInputView is the dedicated PIN input field used for pin auto submit.
//
// The UI looks like this:
//
//   o   o   o
//  ___ ___ ___ ___ ___ ___
//
// An underline is shown for each digit of the user's PIN. The characters are
// obscured and it is not possible to navigate the fields. It is always focused
// on the next field to be populated. When the last digit is inserted,
// `OnPinSubmit` is called.
//
// When the length changes (e.g.: selecting a user with a different pin length)
// the internal view `code_input_` is destroyed and a new one is inserted.
//
class ASH_EXPORT LoginPinInputView : public views::View {
 public:
  using OnPinSubmit = base::RepeatingCallback<void(const std::u16string& pin)>;
  using OnPinChanged = base::RepeatingCallback<void(bool is_empty)>;

  static const int kDefaultLength;

  class ASH_EXPORT TestApi {
   public:
    explicit TestApi(LoginPinInputView* view);
    ~TestApi();

    views::View* code_input();
    base::Optional<std::string> GetCode();

   private:
    LoginPinInputView* const view_;
  };

  explicit LoginPinInputView(const LoginPalette& palette);
  LoginPinInputView& operator=(const LoginPinInputView&) = delete;
  LoginPinInputView(const LoginPinInputView&) = delete;
  ~LoginPinInputView() override;

  // Checks whether PIN auto submit is supported for the given length.
  static bool IsAutosubmitSupported(int length);

  // `on_submit` is called when the user typed all the digits.
  // `on_changed` is called upon each modification with a boolean indicating if
  // all field are empty. (Drives the visibility of 'Backspace' on the pin pad.)
  void Init(const OnPinSubmit& on_submit, const OnPinChanged& on_changed);

  // Updates the length of the field. Used when switching users.
  void UpdateLength(const size_t pin_length);

  // Updates the palette use by the view.
  void UpdatePalette(const LoginPalette& palette);

  // Updates the view. This can be called when either the length or the
  // palette is updated.
  void UpdateView();

  // When set, hitting return will attempt an unlock with an empty PIN.
  // LoginAuthUserView interprets such attempts as a SmartLock unlock.
  void SetAuthenticateWithEmptyPinOnReturnKey(bool enabled);

  void Reset();
  void Backspace();
  void InsertDigit(int digit);

  // Sets the field as read only. The field is made read only during an
  // authentication request.
  void SetReadOnly(bool read_only);
  // views::View
  gfx::Size CalculatePreferredSize() const override;
  void RequestFocus() override;
  bool OnKeyPressed(const ui::KeyEvent& event) override;

 private:
  // The code input will call this when all digits are in.
  void SubmitPin(const std::u16string& pin);

  // Called by the inner view whenever the fields change.
  void OnChanged(bool is_empty);

  // Current field length.
  size_t length_ = kDefaultLength;

  // Palette for the instance.
  LoginPalette palette_;

  // Whether the field is read only.
  bool is_read_only_ = false;

  // The input field owned by this view.
  LoginPinInput* code_input_ = nullptr;

  // Whether the 'Return' key should trigger an unlock with an empty PIN.
  bool authenticate_with_empty_pin_on_return_key_ = false;

  OnPinSubmit on_submit_;
  OnPinChanged on_changed_;
};

}  // namespace ash

#endif  // ASH_LOGIN_UI_LOGIN_PIN_INPUT_VIEW_H_
