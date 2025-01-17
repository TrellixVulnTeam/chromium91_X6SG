// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/autofill_assistant/browser/actions/fallback_handler/required_fields_fallback_handler.h"

#include "base/bind.h"
#include "base/callback.h"
#include "base/guid.h"
#include "base/strings/string_number_conversions.h"
#include "base/test/gmock_callback_support.h"
#include "base/test/mock_callback.h"
#include "components/autofill/core/browser/autofill_test_utils.h"
#include "components/autofill/core/browser/data_model/autofill_profile.h"
#include "components/autofill_assistant/browser/actions/action_test_utils.h"
#include "components/autofill_assistant/browser/actions/fallback_handler/required_field.h"
#include "components/autofill_assistant/browser/actions/mock_action_delegate.h"
#include "components/autofill_assistant/browser/client_status.h"
#include "components/autofill_assistant/browser/service.pb.h"
#include "components/autofill_assistant/browser/web/element_finder.h"
#include "components/autofill_assistant/browser/web/mock_web_controller.h"
#include "testing/gmock/include/gmock/gmock.h"

namespace autofill_assistant {
namespace {

using ::base::test::RunOnceCallback;
using ::testing::_;
using ::testing::Expectation;
using ::testing::InSequence;
using ::testing::Invoke;
using ::testing::Return;

RequiredField CreateRequiredField(const std::string& value_expression,
                                  const std::vector<std::string>& selector) {
  RequiredField required_field;
  required_field.value_expression = value_expression;
  required_field.selector = Selector(selector);
  required_field.status = RequiredField::EMPTY;
  return required_field;
}

class RequiredFieldsFallbackHandlerTest : public testing::Test {
 public:
  void SetUp() override {
    ON_CALL(mock_action_delegate_, RunElementChecks)
        .WillByDefault(Invoke([this](BatchElementChecker* checker) {
          checker->Run(&mock_web_controller_);
        }));
    test_util::MockFindAnyElement(mock_web_controller_);
    ON_CALL(mock_action_delegate_, GetWebController)
        .WillByDefault(Return(&mock_web_controller_));
    ON_CALL(mock_web_controller_, GetElementTag(_, _))
        .WillByDefault(RunOnceCallback<1>(OkClientStatus(), "INPUT"));
    ON_CALL(mock_web_controller_, SetValueAttribute(_, _, _))
        .WillByDefault(RunOnceCallback<2>(OkClientStatus()));
    ON_CALL(mock_action_delegate_, WaitUntilDocumentIsInReadyState(_, _, _, _))
        .WillByDefault(RunOnceCallback<3>(OkClientStatus(),
                                          base::TimeDelta::FromSeconds(0)));
    ON_CALL(mock_web_controller_, ScrollIntoView(_, _, _))
        .WillByDefault(RunOnceCallback<2>(OkClientStatus()));
    ON_CALL(mock_web_controller_, WaitUntilElementIsStable(_, _, _, _))
        .WillByDefault(RunOnceCallback<3>(OkClientStatus(),
                                          base::TimeDelta::FromSeconds(0)));
  }

 protected:
  MockActionDelegate mock_action_delegate_;
  MockWebController mock_web_controller_;
};

TEST_F(RequiredFieldsFallbackHandlerTest,
       AutofillFailureExitsEarlyForEmptyRequiredFields) {
  RequiredFieldsFallbackHandler fallback_handler(
      /* required_fields = */ {},
      /* fallback_values = */ {}, &mock_action_delegate_);

  fallback_handler.CheckAndFallbackRequiredFields(
      ClientStatus(OTHER_ACTION_STATUS),
      base::BindOnce([](const ClientStatus& status) {
        EXPECT_EQ(status.proto_status(), OTHER_ACTION_STATUS);
        EXPECT_FALSE(status.details().has_autofill_error_info());
      }));
}

TEST_F(RequiredFieldsFallbackHandlerTest, AutofillFailureGetsForwarded) {
  // Everything is full, no need to do work. Required fields succeed by
  // default.
  ON_CALL(mock_web_controller_, GetFieldValue(_, _))
      .WillByDefault(RunOnceCallback<1>(OkClientStatus(), "value"));

  std::vector<RequiredField> required_fields = {
      CreateRequiredField("${51}", {"#card_name"})};

  RequiredFieldsFallbackHandler fallback_handler(required_fields, {},
                                                 &mock_action_delegate_);
  fallback_handler.CheckAndFallbackRequiredFields(
      ClientStatus(OTHER_ACTION_STATUS),
      base::BindOnce([](const ClientStatus& status) {
        EXPECT_EQ(status.proto_status(), OTHER_ACTION_STATUS);
        EXPECT_EQ(
            status.details().autofill_error_info().autofill_error_status(),
            OTHER_ACTION_STATUS);
      }));
}

TEST_F(RequiredFieldsFallbackHandlerTest,
       AutofillFailureReturnedOverFallbackError) {
  // Everything is empty. Required fields fail by default.
  ON_CALL(mock_web_controller_, GetFieldValue(_, _))
      .WillByDefault(RunOnceCallback<1>(OkClientStatus(), std::string()));

  std::vector<RequiredField> required_fields = {
      CreateRequiredField("${51}", {"#card_name"})};

  RequiredFieldsFallbackHandler fallback_handler(required_fields, {},
                                                 &mock_action_delegate_);
  fallback_handler.CheckAndFallbackRequiredFields(
      ClientStatus(OTHER_ACTION_STATUS),
      base::BindOnce([](const ClientStatus& status) {
        EXPECT_EQ(status.proto_status(), OTHER_ACTION_STATUS);
        EXPECT_EQ(
            status.details().autofill_error_info().autofill_error_status(),
            OTHER_ACTION_STATUS);
      }));
}

TEST_F(RequiredFieldsFallbackHandlerTest,
       AddsMissingOrEmptyFallbackValuesToError) {
  // The checks should only run once (initially). There should not be a
  // "non-empty" validation because it failed before that.
  Selector card_name_selector({"#card_name"});
  Selector card_number_selector({"#card_number"});
  Selector card_network_selector({"#card_network"});
  EXPECT_CALL(mock_web_controller_,
              GetFieldValue(EqualsElement(test_util::MockFindElement(
                                mock_web_controller_, card_name_selector)),
                            _))
      .WillOnce(RunOnceCallback<1>(OkClientStatus(), std::string()));
  EXPECT_CALL(mock_web_controller_,
              GetFieldValue(EqualsElement(test_util::MockFindElement(
                                mock_web_controller_, card_number_selector)),
                            _))
      .WillOnce(RunOnceCallback<1>(OkClientStatus(), std::string()));
  EXPECT_CALL(mock_web_controller_,
              GetFieldValue(EqualsElement(test_util::MockFindElement(
                                mock_web_controller_, card_network_selector)),
                            _))
      .WillOnce(RunOnceCallback<1>(OkClientStatus(), std::string()));

  std::vector<RequiredField> required_fields = {
      CreateRequiredField("${51}", {"#card_name"}),
      CreateRequiredField("${52}", {"#card_number"}),
      CreateRequiredField("${-3}", {"#card_network"})};

  std::map<std::string, std::string> fallback_values = {
      {base::NumberToString(
           static_cast<int>(autofill::ServerFieldType::CREDIT_CARD_NAME_FULL)),
       "John Doe"},
      {base::NumberToString(
           static_cast<int>(AutofillFormatProto::CREDIT_CARD_NETWORK)),
       std::string()}};

  RequiredFieldsFallbackHandler fallback_handler(
      required_fields, fallback_values, &mock_action_delegate_);

  base::OnceCallback<void(const ClientStatus&)> callback =
      base::BindOnce([](const ClientStatus& status) {
        EXPECT_EQ(status.proto_status(), AUTOFILL_INCOMPLETE);
        ASSERT_EQ(
            status.details().autofill_error_info().autofill_field_error_size(),
            2);
        EXPECT_EQ(status.details()
                      .autofill_error_info()
                      .autofill_field_error(0)
                      .value_expression(),
                  "${52}");
        EXPECT_TRUE(status.details()
                        .autofill_error_info()
                        .autofill_field_error(0)
                        .no_fallback_value());
        EXPECT_EQ(status.details()
                      .autofill_error_info()
                      .autofill_field_error(1)
                      .value_expression(),
                  "${-3}");
        EXPECT_TRUE(status.details()
                        .autofill_error_info()
                        .autofill_field_error(1)
                        .no_fallback_value());
      });

  fallback_handler.CheckAndFallbackRequiredFields(OkClientStatus(),
                                                  std::move(callback));
}

TEST_F(RequiredFieldsFallbackHandlerTest, AddsFirstFieldFillingError) {
  ON_CALL(mock_web_controller_, GetFieldValue(_, _))
      .WillByDefault(RunOnceCallback<1>(OkClientStatus(), ""));
  ON_CALL(mock_web_controller_, SetValueAttribute(_, _, _))
      .WillByDefault(RunOnceCallback<2>(ClientStatus(OTHER_ACTION_STATUS)));

  std::vector<RequiredField> required_fields = {
      CreateRequiredField("${51}", {"#card_name"}),
      CreateRequiredField("${52}", {"#card_number"})};

  std::map<std::string, std::string> fallback_values = {
      {base::NumberToString(
           static_cast<int>(autofill::ServerFieldType::CREDIT_CARD_NAME_FULL)),
       "John Doe"},
      {base::NumberToString(
           static_cast<int>(autofill::ServerFieldType::CREDIT_CARD_NUMBER)),
       "4111111111111111"}};

  RequiredFieldsFallbackHandler fallback_handler(
      required_fields, fallback_values, &mock_action_delegate_);

  base::OnceCallback<void(const ClientStatus&)> callback =
      base::BindOnce([](const ClientStatus& status) {
        EXPECT_EQ(status.proto_status(), AUTOFILL_INCOMPLETE);
        ASSERT_EQ(
            status.details().autofill_error_info().autofill_field_error_size(),
            1);
        EXPECT_EQ(status.details()
                      .autofill_error_info()
                      .autofill_field_error(0)
                      .value_expression(),
                  "${51}");
        EXPECT_EQ(status.details()
                      .autofill_error_info()
                      .autofill_field_error(0)
                      .status(),
                  OTHER_ACTION_STATUS);
      });

  fallback_handler.CheckAndFallbackRequiredFields(OkClientStatus(),
                                                  std::move(callback));
}

TEST_F(RequiredFieldsFallbackHandlerTest,
       AddsFirstEmptyFieldAfterFillingToError) {
  ON_CALL(mock_web_controller_, GetFieldValue(_, _))
      .WillByDefault(RunOnceCallback<1>(OkClientStatus(), ""));

  std::vector<RequiredField> required_fields = {
      CreateRequiredField("${51}", {"#card_name"}),
      CreateRequiredField("${52}", {"#card_number"})};

  std::map<std::string, std::string> fallback_values = {
      {base::NumberToString(
           static_cast<int>(autofill::ServerFieldType::CREDIT_CARD_NAME_FULL)),
       "John Doe"},
      {base::NumberToString(
           static_cast<int>(autofill::ServerFieldType::CREDIT_CARD_NUMBER)),
       "4111111111111111"}};

  RequiredFieldsFallbackHandler fallback_handler(
      required_fields, fallback_values, &mock_action_delegate_);

  base::OnceCallback<void(const ClientStatus&)> callback =
      base::BindOnce([](const ClientStatus& status) {
        EXPECT_EQ(status.proto_status(), AUTOFILL_INCOMPLETE);
        ASSERT_EQ(
            status.details().autofill_error_info().autofill_field_error_size(),
            1);
        EXPECT_EQ(status.details()
                      .autofill_error_info()
                      .autofill_field_error(0)
                      .value_expression(),
                  "${51}");
        EXPECT_TRUE(status.details()
                        .autofill_error_info()
                        .autofill_field_error(0)
                        .empty_after_fallback());
      });

  fallback_handler.CheckAndFallbackRequiredFields(OkClientStatus(),
                                                  std::move(callback));
}

TEST_F(RequiredFieldsFallbackHandlerTest, DoesNotFallbackIfFieldsAreFilled) {
  ON_CALL(mock_web_controller_, GetFieldValue(_, _))
      .WillByDefault(RunOnceCallback<1>(OkClientStatus(), "value"));
  EXPECT_CALL(mock_web_controller_, SetValueAttribute(_, _, _)).Times(0);

  std::vector<RequiredField> required_fields = {
      CreateRequiredField("${51}", {"#card_name"})};

  RequiredFieldsFallbackHandler fallback_handler(required_fields, {},
                                                 &mock_action_delegate_);
  fallback_handler.CheckAndFallbackRequiredFields(
      OkClientStatus(), base::BindOnce([](const ClientStatus& status) {
        EXPECT_EQ(status.proto_status(), ACTION_APPLIED);
      }));
}

TEST_F(RequiredFieldsFallbackHandlerTest, FillsEmptyRequiredField) {
  EXPECT_CALL(mock_web_controller_, GetFieldValue(_, _))
      .WillOnce(RunOnceCallback<1>(OkClientStatus(), ""));
  Selector expected_selector({"#card_name"});
  Expectation set_value =
      EXPECT_CALL(
          mock_web_controller_,
          SetValueAttribute("John Doe",
                            EqualsElement(test_util::MockFindElement(
                                mock_action_delegate_, expected_selector)),
                            _))
          .WillOnce(RunOnceCallback<2>(OkClientStatus()));
  EXPECT_CALL(mock_web_controller_, GetFieldValue(_, _))
      .After(set_value)
      .WillOnce(RunOnceCallback<1>(OkClientStatus(), "John Doe"));

  std::vector<RequiredField> required_fields = {
      CreateRequiredField("${51}", {"#card_name"})};

  std::map<std::string, std::string> fallback_values = {
      {base::NumberToString(
           static_cast<int>(autofill::ServerFieldType::CREDIT_CARD_NAME_FULL)),
       "John Doe"}};

  RequiredFieldsFallbackHandler fallback_handler(
      required_fields, fallback_values, &mock_action_delegate_);
  fallback_handler.CheckAndFallbackRequiredFields(
      OkClientStatus(), base::BindOnce([](const ClientStatus& status) {
        EXPECT_EQ(status.proto_status(), ACTION_APPLIED);
      }));
}

TEST_F(RequiredFieldsFallbackHandlerTest, FallsBackForForcedFilledField) {
  ON_CALL(mock_web_controller_, GetFieldValue(_, _))
      .WillByDefault(RunOnceCallback<1>(OkClientStatus(), "value"));
  Selector expected_selector({"#card_name"});
  EXPECT_CALL(mock_web_controller_,
              SetValueAttribute("John Doe",
                                EqualsElement(test_util::MockFindElement(
                                    mock_action_delegate_, expected_selector)),
                                _))
      .WillOnce(RunOnceCallback<2>(OkClientStatus()));

  std::vector<RequiredField> required_fields = {
      CreateRequiredField("${51}", {"#card_name"})};
  required_fields[0].forced = true;

  std::map<std::string, std::string> fallback_values = {
      {base::NumberToString(
           static_cast<int>(autofill::ServerFieldType::CREDIT_CARD_NAME_FULL)),
       "John Doe"}};

  RequiredFieldsFallbackHandler fallback_handler(
      required_fields, fallback_values, &mock_action_delegate_);
  fallback_handler.CheckAndFallbackRequiredFields(
      OkClientStatus(), base::BindOnce([](const ClientStatus& status) {
        EXPECT_EQ(status.proto_status(), ACTION_APPLIED);
      }));
}

TEST_F(RequiredFieldsFallbackHandlerTest, FailsIfForcedFieldDidNotGetFilled) {
  ON_CALL(mock_web_controller_, GetFieldValue(_, _))
      .WillByDefault(RunOnceCallback<1>(OkClientStatus(), "value"));
  EXPECT_CALL(mock_web_controller_, SetValueAttribute(_, _, _)).Times(0);

  std::vector<RequiredField> required_fields = {
      CreateRequiredField("${51}", {"#card_name"})};
  required_fields[0].forced = true;

  RequiredFieldsFallbackHandler fallback_handler(required_fields, {},
                                                 &mock_action_delegate_);

  base::OnceCallback<void(const ClientStatus&)> callback =
      base::BindOnce([](const ClientStatus& status) {
        EXPECT_EQ(status.proto_status(), AUTOFILL_INCOMPLETE);
        ASSERT_EQ(
            status.details().autofill_error_info().autofill_field_error_size(),
            1);
        EXPECT_EQ(status.details()
                      .autofill_error_info()
                      .autofill_field_error(0)
                      .value_expression(),
                  "${51}");
        EXPECT_TRUE(status.details()
                        .autofill_error_info()
                        .autofill_field_error(0)
                        .no_fallback_value());
      });

  fallback_handler.CheckAndFallbackRequiredFields(OkClientStatus(),
                                                  std::move(callback));
}

TEST_F(RequiredFieldsFallbackHandlerTest, FillsFieldWithPattern) {
  EXPECT_CALL(mock_web_controller_, GetFieldValue(_, _))
      .WillOnce(RunOnceCallback<1>(OkClientStatus(), ""));
  Selector expected_selector({"#card_expiry"});
  Expectation set_value =
      EXPECT_CALL(
          mock_web_controller_,
          SetValueAttribute("08/2050",
                            EqualsElement(test_util::MockFindElement(
                                mock_action_delegate_, expected_selector)),
                            _))
          .WillOnce(RunOnceCallback<2>(OkClientStatus()));
  EXPECT_CALL(mock_web_controller_, GetFieldValue(_, _))
      .After(set_value)
      .WillOnce(RunOnceCallback<1>(OkClientStatus(), "not empty"));

  std::vector<RequiredField> required_fields = {
      CreateRequiredField("${53}/${55}", {"#card_expiry"})};

  std::map<std::string, std::string> fallback_values = {
      {base::NumberToString(
           static_cast<int>(autofill::ServerFieldType::CREDIT_CARD_EXP_MONTH)),
       "08"},
      {base::NumberToString(static_cast<int>(
           autofill::ServerFieldType::CREDIT_CARD_EXP_4_DIGIT_YEAR)),
       "2050"}};

  RequiredFieldsFallbackHandler fallback_handler(
      required_fields, fallback_values, &mock_action_delegate_);
  fallback_handler.CheckAndFallbackRequiredFields(
      OkClientStatus(), base::BindOnce([](const ClientStatus& status) {
        EXPECT_EQ(status.proto_status(), ACTION_APPLIED);
      }));
}

TEST_F(RequiredFieldsFallbackHandlerTest,
       FailsToFillFieldWithUnknownOrEmptyKey) {
  EXPECT_CALL(mock_web_controller_, GetFieldValue(_, _))
      .Times(2)
      .WillRepeatedly(RunOnceCallback<1>(OkClientStatus(), ""));
  EXPECT_CALL(mock_web_controller_, SetValueAttribute(_, _, _)).Times(0);

  std::vector<RequiredField> required_fields = {
      CreateRequiredField("${53}", {"#card_expiry"}),
      CreateRequiredField("${-3}", {"#card_network"})};

  std::map<std::string, std::string> fallback_values;
  fallback_values.emplace(base::NumberToString(static_cast<int>(
                              AutofillFormatProto::CREDIT_CARD_NETWORK)),
                          "");

  RequiredFieldsFallbackHandler fallback_handler(
      required_fields, fallback_values, &mock_action_delegate_);

  base::OnceCallback<void(const ClientStatus&)> callback =
      base::BindOnce([](const ClientStatus& status) {
        EXPECT_EQ(status.proto_status(), AUTOFILL_INCOMPLETE);
        ASSERT_EQ(
            status.details().autofill_error_info().autofill_field_error_size(),
            2);
        EXPECT_EQ(status.details()
                      .autofill_error_info()
                      .autofill_field_error(0)
                      .value_expression(),
                  "${53}");
        EXPECT_TRUE(status.details()
                        .autofill_error_info()
                        .autofill_field_error(0)
                        .no_fallback_value());
        EXPECT_EQ(status.details()
                      .autofill_error_info()
                      .autofill_field_error(1)
                      .value_expression(),
                  "${-3}");
        EXPECT_TRUE(status.details()
                        .autofill_error_info()
                        .autofill_field_error(1)
                        .no_fallback_value());
      });

  fallback_handler.CheckAndFallbackRequiredFields(OkClientStatus(),
                                                  std::move(callback));
}

TEST_F(RequiredFieldsFallbackHandlerTest, UsesSelectOptionForDropdowns) {
  InSequence sequence;

  Selector expected_selector({"#year"});

  // First validation fails.
  EXPECT_CALL(mock_web_controller_,
              GetFieldValue(EqualsElement(test_util::MockFindElement(
                                mock_web_controller_, expected_selector)),
                            _))
      .WillOnce(RunOnceCallback<1>(OkClientStatus(), std::string()));

  // Fill field.
  const ElementFinder::Result& expected_element =
      test_util::MockFindElement(mock_action_delegate_, expected_selector);
  EXPECT_CALL(mock_web_controller_,
              GetElementTag(EqualsElement(expected_element), _))
      .WillOnce(RunOnceCallback<1>(OkClientStatus(), "SELECT"));
  EXPECT_CALL(mock_web_controller_,
              SelectOption("^2050", false, SelectOptionProto::LABEL,
                           EqualsElement(expected_element), _))
      .WillOnce(RunOnceCallback<4>(OkClientStatus()));

  // Second validation succeeds.
  EXPECT_CALL(mock_web_controller_,
              GetFieldValue(EqualsElement(test_util::MockFindElement(
                                mock_web_controller_, expected_selector)),
                            _))
      .WillOnce(RunOnceCallback<1>(OkClientStatus(), "2050"));

  std::vector<RequiredField> required_fields = {
      CreateRequiredField("${55}", {"#year"})};

  std::map<std::string, std::string> fallback_values = {
      {base::NumberToString(static_cast<int>(
           autofill::ServerFieldType::CREDIT_CARD_EXP_4_DIGIT_YEAR)),
       "2050"}};

  RequiredFieldsFallbackHandler fallback_handler(
      required_fields, fallback_values, &mock_action_delegate_);
  fallback_handler.CheckAndFallbackRequiredFields(
      OkClientStatus(), base::BindOnce([](const ClientStatus& status) {
        EXPECT_EQ(status.proto_status(), ACTION_APPLIED);
      }));
}

TEST_F(RequiredFieldsFallbackHandlerTest, ClicksOnCustomDropdown) {
  EXPECT_CALL(mock_web_controller_, GetFieldValue(_, _)).Times(0);
  EXPECT_CALL(mock_web_controller_, SetValueAttribute(_, _, _)).Times(0);
  Selector expected_main_selector({"#card_expiry"});
  EXPECT_CALL(
      mock_action_delegate_,
      ClickOrTapElement(ClickType::TAP,
                        EqualsElement(test_util::MockFindElement(
                            mock_action_delegate_, expected_main_selector)),
                        _))
      .WillOnce(RunOnceCallback<2>(OkClientStatus()));
  Selector expected_option_selector({".option"});
  expected_option_selector.MatchingInnerText("08");
  EXPECT_CALL(mock_action_delegate_,
              OnShortWaitForElement(expected_option_selector, _))
      .WillOnce(RunOnceCallback<1>(OkClientStatus(),
                                   base::TimeDelta::FromSeconds(0)));
  EXPECT_CALL(
      mock_action_delegate_,
      ClickOrTapElement(ClickType::TAP,
                        EqualsElement(test_util::MockFindElement(
                            mock_action_delegate_, expected_option_selector)),
                        _))
      .WillOnce(RunOnceCallback<2>(OkClientStatus()));

  std::vector<RequiredField> required_fields = {
      CreateRequiredField("${53}", {"#card_expiry"})};
  required_fields[0].fallback_click_element = Selector({".option"});

  std::map<std::string, std::string> fallback_values = {
      {base::NumberToString(
           static_cast<int>(autofill::ServerFieldType::CREDIT_CARD_EXP_MONTH)),
       "08"}};

  RequiredFieldsFallbackHandler fallback_handler(
      required_fields, fallback_values, &mock_action_delegate_);
  fallback_handler.CheckAndFallbackRequiredFields(
      OkClientStatus(), base::BindOnce([](const ClientStatus& status) {
        EXPECT_EQ(status.proto_status(), ACTION_APPLIED);
      }));
}

TEST_F(RequiredFieldsFallbackHandlerTest, CustomDropdownClicksStopOnError) {
  EXPECT_CALL(mock_web_controller_, GetFieldValue(_, _)).Times(0);
  EXPECT_CALL(mock_web_controller_, SetValueAttribute(_, _, _)).Times(0);
  Selector expected_main_selector({"#card_expiry"});
  Expectation main_click =
      EXPECT_CALL(
          mock_action_delegate_,
          ClickOrTapElement(ClickType::TAP,
                            EqualsElement(test_util::MockFindElement(
                                mock_action_delegate_, expected_main_selector)),
                            _))
          .WillOnce(RunOnceCallback<2>(OkClientStatus()));
  Selector expected_option_selector({".option"});
  expected_option_selector.MatchingInnerText("08");
  EXPECT_CALL(mock_action_delegate_,
              OnShortWaitForElement(expected_option_selector, _))
      .WillOnce(RunOnceCallback<1>(ClientStatus(ELEMENT_RESOLUTION_FAILED),
                                   base::TimeDelta::FromSeconds(0)));
  EXPECT_CALL(mock_action_delegate_, FindElement(_, _))
      .Times(0)
      .After(main_click);
  EXPECT_CALL(mock_action_delegate_, ClickOrTapElement(_, _, _))
      .Times(0)
      .After(main_click);

  std::vector<RequiredField> required_fields = {
      CreateRequiredField("${53}", {"#card_expiry"})};
  required_fields[0].fallback_click_element = Selector({".option"});

  std::map<std::string, std::string> fallback_values = {
      {base::NumberToString(
           static_cast<int>(autofill::ServerFieldType::CREDIT_CARD_EXP_MONTH)),
       "08"}};

  RequiredFieldsFallbackHandler fallback_handler(
      required_fields, fallback_values, &mock_action_delegate_);
  fallback_handler.CheckAndFallbackRequiredFields(
      OkClientStatus(), base::BindOnce([](const ClientStatus& status) {
        EXPECT_EQ(status.proto_status(), AUTOFILL_INCOMPLETE);
      }));
}

TEST_F(RequiredFieldsFallbackHandlerTest, ClearsFilledField) {
  InSequence sequence;

  Selector expected_selector({"#field"});

  // First validation fails
  EXPECT_CALL(mock_web_controller_,
              GetFieldValue(EqualsElement(test_util::MockFindElement(
                                mock_web_controller_, expected_selector)),
                            _))
      .WillOnce(RunOnceCallback<1>(OkClientStatus(), "value"));

  // Clears field.
  EXPECT_CALL(mock_web_controller_,
              SetValueAttribute(std::string(),
                                EqualsElement(test_util::MockFindElement(
                                    mock_action_delegate_, expected_selector)),
                                _))
      .WillOnce(RunOnceCallback<2>(OkClientStatus()));

  // Second validation succeeds.
  EXPECT_CALL(mock_web_controller_,
              GetFieldValue(EqualsElement(test_util::MockFindElement(
                                mock_web_controller_, expected_selector)),
                            _))
      .WillRepeatedly(RunOnceCallback<1>(OkClientStatus(), std::string()));

  std::vector<RequiredField> required_fields = {
      CreateRequiredField(std::string(), {"#field"})};
  std::map<std::string, std::string> fallback_values;

  RequiredFieldsFallbackHandler fallback_handler(
      required_fields, fallback_values, &mock_action_delegate_);
  fallback_handler.CheckAndFallbackRequiredFields(
      OkClientStatus(), base::BindOnce([](const ClientStatus& status) {
        EXPECT_EQ(status.proto_status(), ACTION_APPLIED);
      }));
}

TEST_F(RequiredFieldsFallbackHandlerTest, SkipsForcedFieldCheckOnFirstRun) {
  InSequence sequence;

  Selector forced_field_selector({"#forced_field"});

  // First validation skips forced fields.
  EXPECT_CALL(mock_web_controller_, GetFieldValue(_, _)).Times(0);

  // Fills field.
  EXPECT_CALL(
      mock_web_controller_,
      SetValueAttribute("value",
                        EqualsElement(test_util::MockFindElement(
                            mock_action_delegate_, forced_field_selector)),
                        _))
      .WillOnce(RunOnceCallback<2>(OkClientStatus()));

  // Second validation checks the field.
  EXPECT_CALL(mock_web_controller_,
              GetFieldValue(EqualsElement(test_util::MockFindElement(
                                mock_web_controller_, forced_field_selector)),
                            _))
      .WillRepeatedly(RunOnceCallback<1>(OkClientStatus(), "value"));

  auto forced_field = CreateRequiredField("value", {"#forced_field"});
  forced_field.forced = true;
  std::vector<RequiredField> required_fields = {forced_field};

  std::map<std::string, std::string> fallback_values;

  RequiredFieldsFallbackHandler fallback_handler(
      required_fields, fallback_values, &mock_action_delegate_);
  fallback_handler.CheckAndFallbackRequiredFields(
      OkClientStatus(), base::BindOnce([](const ClientStatus& status) {
        EXPECT_EQ(status.proto_status(), ACTION_APPLIED);
      }));
}

TEST_F(RequiredFieldsFallbackHandlerTest,
       EmptyValueDoesNotFailForFieldNotNeedingToBeFilled) {
  Selector card_name_selector({"#card_name"});
  Selector card_number_selector({"#card_number"});
  auto card_name_element =
      test_util::MockFindElement(mock_web_controller_, card_name_selector, 2);
  EXPECT_CALL(mock_web_controller_,
              GetFieldValue(EqualsElement(card_name_element), _))
      .WillOnce(RunOnceCallback<1>(OkClientStatus(), std::string()))
      .WillOnce(RunOnceCallback<1>(OkClientStatus(), "value"));
  auto card_number_element =
      test_util::MockFindElement(mock_web_controller_, card_number_selector, 2);
  EXPECT_CALL(mock_web_controller_,
              GetFieldValue(EqualsElement(card_number_element), _))
      .Times(2)
      .WillRepeatedly(RunOnceCallback<1>(OkClientStatus(), "value"));
  EXPECT_CALL(mock_web_controller_,
              SetValueAttribute(_,
                                EqualsElement(test_util::MockFindElement(
                                    mock_action_delegate_, card_name_selector)),
                                _))
      .WillOnce(RunOnceCallback<2>(OkClientStatus()));

  std::vector<RequiredField> required_fields = {
      CreateRequiredField("${51}", {"#card_name"}),
      CreateRequiredField("${52}", {"#card_number"})};

  std::map<std::string, std::string> fallback_values = {
      {base::NumberToString(
           static_cast<int>(autofill::ServerFieldType::CREDIT_CARD_NAME_FULL)),
       "John Doe"}};

  RequiredFieldsFallbackHandler fallback_handler(
      required_fields, fallback_values, &mock_action_delegate_);

  base::OnceCallback<void(const ClientStatus&)> callback =
      base::BindOnce([](const ClientStatus& status) {
        EXPECT_EQ(status.proto_status(), ACTION_APPLIED);
        EXPECT_EQ(
            status.details().autofill_error_info().autofill_field_error_size(),
            0);
      });

  fallback_handler.CheckAndFallbackRequiredFields(OkClientStatus(),
                                                  std::move(callback));
}

}  // namespace
}  // namespace autofill_assistant
