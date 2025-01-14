// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/password_manager/core/browser/leak_detection_dialog_utils.h"

#include "base/i18n/message_formatter.h"
#include "base/strings/utf_string_conversions.h"
#include "base/test/scoped_feature_list.h"
#include "build/build_config.h"
#include "components/password_manager/core/browser/insecure_credentials_table.h"
#include "components/password_manager/core/common/password_manager_features.h"
#include "components/strings/grit/components_strings.h"
#include "components/url_formatter/elide_url.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "ui/base/l10n/l10n_util.h"
#include "url/gurl.h"
#include "url/origin.h"

using password_manager::CreateLeakType;
using password_manager::CredentialLeakFlags;
using password_manager::CredentialLeakType;
using password_manager::IsReused;
using password_manager::IsSaved;
using password_manager::IsSyncing;

namespace password_manager {

namespace {

// Contains information that should be displayed on the leak dialog for
// specified |leak_type|.
const struct {
  // Specifies the test case.
  CredentialLeakType leak_type;
  // The rest of the fields specify what should be displayed for this test case.
  int accept_button_id;
  int cancel_button_id;
  int leak_message_id;
  int leak_title_id;
  bool should_show_cancel_button;
  bool should_check_passwords;
} kLeakTypesTestCases[] = {
    {CreateLeakType(IsSaved(false), IsReused(false), IsSyncing(false)), IDS_OK,
     IDS_CLOSE, IDS_CREDENTIAL_LEAK_CHANGE_PASSWORD_MESSAGE,
     IDS_CREDENTIAL_LEAK_TITLE_CHANGE, false, false},
    {CreateLeakType(IsSaved(false), IsReused(false), IsSyncing(true)), IDS_OK,
     IDS_CLOSE, IDS_CREDENTIAL_LEAK_CHANGE_PASSWORD_MESSAGE,
     IDS_CREDENTIAL_LEAK_TITLE_CHANGE, false, false},
    {CreateLeakType(IsSaved(false), IsReused(true), IsSyncing(true)),
     IDS_LEAK_CHECK_CREDENTIALS, IDS_CLOSE,
     IDS_CREDENTIAL_LEAK_CHANGE_AND_CHECK_PASSWORDS_MESSAGE,
     IDS_CREDENTIAL_LEAK_TITLE_CHECK, true, true},
    {CreateLeakType(IsSaved(true), IsReused(false), IsSyncing(true)), IDS_OK,
     IDS_CLOSE, IDS_CREDENTIAL_LEAK_CHANGE_PASSWORD_MESSAGE,
     IDS_CREDENTIAL_LEAK_TITLE_CHANGE, false, false},
    {CreateLeakType(IsSaved(true), IsReused(true), IsSyncing(true)),
     IDS_LEAK_CHECK_CREDENTIALS, IDS_CLOSE,
     IDS_CREDENTIAL_LEAK_CHECK_PASSWORDS_MESSAGE,
     IDS_CREDENTIAL_LEAK_TITLE_CHECK, true, true}};

struct BulkCheckParams {
  // Specifies the test case.
  CredentialLeakType leak_type;
  bool should_check_passwords;
} kBulkCheckTestCases[] = {
    {CreateLeakType(IsSaved(false), IsReused(false), IsSyncing(false)), false},
    {CreateLeakType(IsSaved(true), IsReused(false), IsSyncing(false)), false},
    {CreateLeakType(IsSaved(false), IsReused(true), IsSyncing(false)), true},
    {CreateLeakType(IsSaved(true), IsReused(true), IsSyncing(false)), true},
    {CreateLeakType(IsSaved(true), IsReused(true), IsSyncing(true)), true}};
}  // namespace

TEST(CredentialLeakDialogUtilsTest, GetAcceptButtonLabel) {
  for (size_t i = 0; i < base::size(kLeakTypesTestCases); ++i) {
    SCOPED_TRACE(testing::Message() << i);
    EXPECT_EQ(
        l10n_util::GetStringUTF16(kLeakTypesTestCases[i].accept_button_id),
        GetAcceptButtonLabel(kLeakTypesTestCases[i].leak_type));
  }
}

TEST(CredentialLeakDialogUtilsTest, GetCancelButtonLabel) {
  for (size_t i = 0; i < base::size(kLeakTypesTestCases); ++i) {
    SCOPED_TRACE(testing::Message() << i);
    EXPECT_EQ(
        l10n_util::GetStringUTF16(kLeakTypesTestCases[i].cancel_button_id),
        GetCancelButtonLabel());
  }
}

TEST(CredentialLeakDialogUtilsTest, GetCheckPasswordsDescription) {
  GURL origin("https://example.com");
  for (size_t i = 0; i < base::size(kLeakTypesTestCases); ++i) {
    if (kLeakTypesTestCases[i].leak_message_id ==
        IDS_CREDENTIAL_LEAK_CHECK_PASSWORDS_MESSAGE) {
      SCOPED_TRACE(testing::Message() << i);
      std::u16string expected_message = l10n_util::GetStringUTF16(
          IDS_CREDENTIAL_LEAK_CHECK_PASSWORDS_MESSAGE);
      EXPECT_EQ(expected_message,
                GetDescription(kLeakTypesTestCases[i].leak_type, origin));
    }
  }
}

TEST(CredentialLeakDialogUtilsTest, GetChangeAndCheckPasswordsDescription) {
  GURL origin("https://example.com");
  for (size_t i = 0; i < base::size(kLeakTypesTestCases); ++i) {
    if (kLeakTypesTestCases[i].leak_message_id ==
        IDS_CREDENTIAL_LEAK_CHANGE_AND_CHECK_PASSWORDS_MESSAGE) {
      SCOPED_TRACE(testing::Message() << i);
      std::u16string expected_message = l10n_util::GetStringUTF16(
          IDS_CREDENTIAL_LEAK_CHANGE_AND_CHECK_PASSWORDS_MESSAGE);
      EXPECT_EQ(expected_message,
                GetDescription(kLeakTypesTestCases[i].leak_type, origin));
    }
  }
}

TEST(CredentialLeakDialogUtilsTest, GetChangePasswordDescription) {
  GURL origin("https://example.com");
  for (size_t i = 0; i < base::size(kLeakTypesTestCases); ++i) {
    if (kLeakTypesTestCases[i].leak_message_id ==
        IDS_CREDENTIAL_LEAK_CHANGE_PASSWORD_MESSAGE) {
      SCOPED_TRACE(testing::Message() << i);
      std::u16string expected_message = l10n_util::GetStringUTF16(
          IDS_CREDENTIAL_LEAK_CHANGE_PASSWORD_MESSAGE);
      EXPECT_EQ(expected_message,
                GetDescription(kLeakTypesTestCases[i].leak_type, origin));
    }
  }
}

TEST(CredentialLeakDialogUtilsTest, GetTitle) {
  for (size_t i = 0; i < base::size(kLeakTypesTestCases); ++i) {
    SCOPED_TRACE(testing::Message() << i);
    EXPECT_EQ(l10n_util::GetStringUTF16(kLeakTypesTestCases[i].leak_title_id),
              GetTitle(kLeakTypesTestCases[i].leak_type));
  }
}

TEST(CredentialLeakDialogUtilsTest, ShouldCheckPasswords) {
  for (size_t i = 0; i < base::size(kLeakTypesTestCases); ++i) {
    SCOPED_TRACE(testing::Message() << i);
    EXPECT_EQ(kLeakTypesTestCases[i].should_check_passwords,
              ShouldCheckPasswords(kLeakTypesTestCases[i].leak_type));
  }
}

TEST(CredentialLeakDialogUtilsTest, ShouldShowCancelButton) {
  for (size_t i = 0; i < base::size(kLeakTypesTestCases); ++i) {
    SCOPED_TRACE(testing::Message() << i);
    EXPECT_EQ(kLeakTypesTestCases[i].should_show_cancel_button,
              ShouldShowCancelButton(kLeakTypesTestCases[i].leak_type));
  }
}

class BulkCheckCredentialLeakDialogUtilsTest
    : public testing::TestWithParam<BulkCheckParams> {
};

TEST_P(BulkCheckCredentialLeakDialogUtilsTest, ShouldCheckPasswords) {
  SCOPED_TRACE(testing::Message() << GetParam().leak_type);
  EXPECT_EQ(GetParam().should_check_passwords,
            ShouldCheckPasswords(GetParam().leak_type));
}

TEST_P(BulkCheckCredentialLeakDialogUtilsTest, Buttons) {
  SCOPED_TRACE(testing::Message() << GetParam().leak_type);
  EXPECT_EQ(GetParam().should_check_passwords,
            ShouldShowCancelButton(GetParam().leak_type));
  EXPECT_EQ(l10n_util::GetStringUTF16(GetParam().should_check_passwords
                                          ? IDS_LEAK_CHECK_CREDENTIALS
                                          : IDS_OK),
            GetAcceptButtonLabel(GetParam().leak_type));
  EXPECT_EQ(l10n_util::GetStringUTF16(IDS_CLOSE), GetCancelButtonLabel());
}

TEST_P(BulkCheckCredentialLeakDialogUtilsTest, Title) {
  SCOPED_TRACE(testing::Message() << GetParam().leak_type);
  EXPECT_EQ(l10n_util::GetStringUTF16(GetParam().should_check_passwords
                                          ? IDS_CREDENTIAL_LEAK_TITLE_CHECK
                                          : IDS_CREDENTIAL_LEAK_TITLE_CHANGE),
            GetTitle(GetParam().leak_type));
}

INSTANTIATE_TEST_SUITE_P(InstantiationName,
                         BulkCheckCredentialLeakDialogUtilsTest,
                         testing::ValuesIn(kBulkCheckTestCases));

#if defined(OS_ANDROID)
struct PasswordChangeParams {
  // Specifies the test case.
  CredentialLeakType leak_type;
  int accept_button_id;
  bool should_show_cancel_button;
  bool should_show_change_password_button;
} kPasswordChangeTestCases[] = {
    {CreateLeakType(IsSaved(false), IsReused(false), IsSyncing(false)), IDS_OK,
     false, false},
    {CreateLeakType(IsSaved(false), IsReused(false), IsSyncing(true)), IDS_OK,
     false, false},
    {CreateLeakType(IsSaved(false), IsReused(true), IsSyncing(false)),
     IDS_LEAK_CHECK_CREDENTIALS, true, false},
    {CreateLeakType(IsSaved(false), IsReused(true), IsSyncing(true)),
     IDS_LEAK_CHECK_CREDENTIALS, true, false},
    {CreateLeakType(IsSaved(true), IsReused(false), IsSyncing(false)), IDS_OK,
     false, false},
    {CreateLeakType(IsSaved(true), IsReused(false), IsSyncing(true)),
     IDS_PASSWORD_CHANGE, true, true},
    {CreateLeakType(IsSaved(true), IsReused(true), IsSyncing(false)),
     IDS_LEAK_CHECK_CREDENTIALS, true, false},
    {CreateLeakType(IsSaved(true), IsReused(true), IsSyncing(true)),
     IDS_LEAK_CHECK_CREDENTIALS, true, false}};

class PasswordChangeCredentialLeakDialogUtilsTest
    : public testing::TestWithParam<PasswordChangeParams> {
 public:
  PasswordChangeCredentialLeakDialogUtilsTest() {
    feature_list_.InitAndEnableFeature(features::kPasswordChange);
  }

 private:
  base::test::ScopedFeatureList feature_list_;
};

TEST_P(PasswordChangeCredentialLeakDialogUtilsTest,
       ShouldShowChangePasswordButton) {
  SCOPED_TRACE(testing::Message() << GetParam().leak_type);

  // ShouldCheckPasswords and ShouldShowChangePasswordButton
  // should never be true both.
  EXPECT_FALSE(ShouldCheckPasswords(GetParam().leak_type) &&
               ShouldShowChangePasswordButton(GetParam().leak_type));

  EXPECT_EQ(GetParam().should_show_change_password_button,
            ShouldShowChangePasswordButton(GetParam().leak_type));
}

TEST_P(PasswordChangeCredentialLeakDialogUtilsTest, ShouldShowCancelButton) {
  SCOPED_TRACE(testing::Message() << GetParam().leak_type);
  EXPECT_EQ(GetParam().should_show_cancel_button,
            ShouldShowCancelButton(GetParam().leak_type));
}

TEST_P(PasswordChangeCredentialLeakDialogUtilsTest, GetAcceptButtonLabel) {
  SCOPED_TRACE(testing::Message() << GetParam().leak_type);
  EXPECT_EQ(l10n_util::GetStringUTF16(GetParam().accept_button_id),
            GetAcceptButtonLabel(GetParam().leak_type));
}

INSTANTIATE_TEST_SUITE_P(InstantiationName,
                         PasswordChangeCredentialLeakDialogUtilsTest,
                         testing::ValuesIn(kPasswordChangeTestCases));
#endif
}  // namespace password_manager
