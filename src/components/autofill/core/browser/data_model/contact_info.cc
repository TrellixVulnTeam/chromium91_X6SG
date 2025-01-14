// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/autofill/core/browser/data_model/contact_info.h"

#include <stddef.h>
#include <ostream>
#include <string>

#include "base/check_op.h"
#include "base/macros.h"
#include "base/notreached.h"
#include "base/strings/string_util.h"
#include "base/strings/utf_string_conversions.h"
#include "components/autofill/core/browser/autofill_data_util.h"
#include "components/autofill/core/browser/autofill_regexes.h"
#include "components/autofill/core/browser/autofill_type.h"
#include "components/autofill/core/browser/data_model/autofill_profile.h"
#include "components/autofill/core/browser/data_model/autofill_structured_address_utils.h"
#include "components/autofill/core/common/autofill_features.h"
#include "components/autofill/core/common/autofill_l10n_util.h"

namespace autofill {

namespace {

// Factory for the structured tree to be used in NameInfo.
std::unique_ptr<AddressComponent> CreateStructuredNameTree() {
  if (structured_address::HonorificPrefixEnabled()) {
    return std::make_unique<structured_address::NameFullWithPrefix>();
  }
  return std::make_unique<structured_address::NameFull>();
}

}  // namespace

using structured_address::VerificationStatus;

NameInfo::NameInfo() : name_(CreateStructuredNameTree()) {}

NameInfo::NameInfo(const NameInfo& info) : NameInfo() {
  *this = info;
}

NameInfo::~NameInfo() = default;

NameInfo& NameInfo::operator=(const NameInfo& info) {
  if (this == &info)
    return *this;

  // TODO(crbug.com/1103421): Clean legacy implementation once structured names
  // are fully launched.
  if (structured_address::StructuredNamesEnabled()) {
    name_->CopyFrom(*info.name_);
  } else {
    given_ = info.given_;
    middle_ = info.middle_;
    family_ = info.family_;
    full_ = info.full_;
  }
  return *this;
}

bool NameInfo::MergeStructuredName(const NameInfo& newer) {
  return name_->MergeWithComponent(newer.GetStructuredName());
}

void NameInfo::MergeStructuredNameValidationStatuses(const NameInfo& newer) {
  name_->MergeVerificationStatuses(newer.GetStructuredName());
}

bool NameInfo::IsStructuredNameMergeable(const NameInfo& newer) const {
  if (!structured_address::StructuredNamesEnabled())
    NOTREACHED();

  return name_->IsMergeableWithComponent(newer.GetStructuredName());
}

bool NameInfo::FinalizeAfterImport(bool profile_is_verified) {
  if (structured_address::StructuredNamesEnabled()) {
    name_->MigrateLegacyStructure(profile_is_verified);
    return name_->CompleteFullTree();
  }
  return true;
}

bool NameInfo::operator==(const NameInfo& other) const {
  if (this == &other)
    return true;

  // TODO(crbug.com/1103421): Clean legacy implementation once structured names
  // are fully launched.
  if (structured_address::StructuredNamesEnabled())
    return name_->SameAs(*other.name_);

  return given_ == other.given_ && middle_ == other.middle_ &&
         family_ == other.family_ && full_ == other.full_;
}

std::u16string NameInfo::GetRawInfo(ServerFieldType type) const {
  DCHECK_EQ(FieldTypeGroup::kName, AutofillType(type).group());

  // TODO(crbug.com/1103421): Clean legacy implementation once structured names
  // are fully launched.
  if (structured_address::StructuredNamesEnabled()) {
    // Without the second generation of the structured name tree, honorific
    // prefixes and the name including the prefix are unsupported types.
    if ((type == NAME_HONORIFIC_PREFIX ||
         type == NAME_FULL_WITH_HONORIFIC_PREFIX) &&
        !structured_address::HonorificPrefixEnabled()) {
      return std::u16string();
    }
    return name_->GetValueForType(type);
  }
  switch (type) {
    case NAME_FIRST:
      return given_;

    case NAME_MIDDLE:
      return middle_;

    case NAME_LAST:
      return family_;

    case NAME_MIDDLE_INITIAL:
      return MiddleInitial();

    case NAME_FULL:
      return full_;

    default:
      return std::u16string();
  }
}

void NameInfo::SetRawInfoWithVerificationStatus(ServerFieldType type,
                                                const std::u16string& value,
                                                VerificationStatus status) {
  DCHECK_EQ(FieldTypeGroup::kName, AutofillType(type).group());
  // TODO(crbug.com/1103421): Clean legacy implementation once structured names
  // are fully launched.
  if (structured_address::StructuredNamesEnabled()) {
    // Without the second generation of the structured name tree, honorific
    // prefixes and the name including the prefix are unsupported types.
    if ((type == NAME_HONORIFIC_PREFIX ||
         type == NAME_FULL_WITH_HONORIFIC_PREFIX) &&
        !structured_address::HonorificPrefixEnabled()) {
      return;
    }
    bool success = name_->SetValueForTypeIfPossible(type, value, status);
    DCHECK(success) << AutofillType::ServerFieldTypeToString(type);
    return;
  }
  switch (type) {
    case NAME_FIRST:
      given_ = value;
      break;

    case NAME_MIDDLE:
    case NAME_MIDDLE_INITIAL:
      middle_ = value;
      break;

    case NAME_LAST:
      family_ = value;
      break;

    case NAME_FULL:
      full_ = value;
      break;

    case NAME_LAST_FIRST:
    case NAME_LAST_SECOND:
    case NAME_LAST_CONJUNCTION:
    case NAME_HONORIFIC_PREFIX:
    case NAME_FULL_WITH_HONORIFIC_PREFIX:
      break;

    default:
      NOTREACHED();
  }
}

void NameInfo::GetSupportedTypes(ServerFieldTypeSet* supported_types) const {
  // TODO(crbug.com/1103421): Clean legacy implementation once structured names
  // are fully launched.
  if (structured_address::StructuredNamesEnabled()) {
    name_->GetSupportedTypes(supported_types);
  } else {
    supported_types->insert(NAME_FIRST);
    supported_types->insert(NAME_MIDDLE);
    supported_types->insert(NAME_LAST);
    supported_types->insert(NAME_MIDDLE_INITIAL);
    supported_types->insert(NAME_FULL);
  }
}

std::u16string NameInfo::GetInfoImpl(const AutofillType& type,
                                     const std::string& app_locale) const {
  // TODO(crbug.com/1103421): Clean legacy implementation once structured names
  // are fully launched.
  if (!structured_address::StructuredNamesEnabled()) {
    if (type.GetStorableType() == NAME_FULL)
      return FullName();
  }
  return GetRawInfo(type.GetStorableType());
}

bool NameInfo::SetInfoWithVerificationStatusImpl(const AutofillType& type,
                                                 const std::u16string& value,
                                                 const std::string& app_locale,
                                                 VerificationStatus status) {
  // TODO(crbug.com/1103421): Clean legacy implementation once structured names
  // are fully launched.
  if (structured_address::StructuredNamesEnabled()) {
    if (type.GetStorableType() == NAME_FULL) {
      // If the set string is token equivalent to the old one, the value can
      // just be updated, otherwise create a new name record and complete it in
      // the end.
      bool token_equivalent = structured_address::AreStringTokenEquivalent(
          value, name_->GetValueForType(NAME_FULL));
      name_->SetValueForTypeIfPossible(
          type.GetStorableType(), value, status,
          /*invalidate_child_nodes=*/!token_equivalent);
      return true;
    }
    return FormGroup::SetInfoWithVerificationStatusImpl(type, value, app_locale,
                                                        status);
  }
  // Always clear out the full name if we're making a change.
  if (value != GetInfo(type, app_locale))
    full_.clear();

  if (type.GetStorableType() == NAME_FULL) {
    SetFullName(value);
    return true;
  }
  return FormGroup::SetInfoWithVerificationStatusImpl(type, value, app_locale,
                                                      status);
}

void NameInfo::GetMatchingTypes(const std::u16string& text,
                                const std::string& app_locale,
                                ServerFieldTypeSet* matching_types) const {
  FormGroup::GetMatchingTypes(text, app_locale, matching_types);
  // Replace type matches for |NAME_FULL_WITH_HONORIFIC_PREFIX| with |NAME_FULL|
  // to always vote for a full name field even if the user decides to add an
  // additional honorific prefix to their name.
  if (matching_types->contains(NAME_FULL_WITH_HONORIFIC_PREFIX)) {
    matching_types->erase(NAME_FULL_WITH_HONORIFIC_PREFIX);
    matching_types->insert(NAME_FULL);
  }
}

VerificationStatus NameInfo::GetVerificationStatusImpl(
    ServerFieldType type) const {
  // TODO(crbug.com/1103421): Clean legacy implementation once structured
  // names are fully launched.
  // Without the second generation of the structured name tree, honorific
  // prefixes and the name including the prefix are unsupported types.
  if (structured_address::StructuredNamesEnabled() &&
      !((type == NAME_HONORIFIC_PREFIX ||
         type == NAME_FULL_WITH_HONORIFIC_PREFIX) &&
        !structured_address::HonorificPrefixEnabled())) {
    return name_->GetVerificationStatusForType(type);
  }
  return VerificationStatus::kNoStatus;
}

std::u16string NameInfo::FullName() const {
  // TODO(crbug.com/1103421): Clean legacy implementation once structured
  // names are fully launched.
  if (structured_address::StructuredNamesEnabled())
    NOTREACHED();
  if (!full_.empty())
    return full_;

  return data_util::JoinNameParts(given_, middle_, family_);
}

std::u16string NameInfo::MiddleInitial() const {
  // TODO(crbug.com/1103421): Clean legacy implementation once structured
  // names are fully launched.
  if (structured_address::StructuredNamesEnabled())
    NOTREACHED();
  return middle_.empty() ? std::u16string() : middle_.substr(0U, 1U);
}

void NameInfo::SetFullName(const std::u16string& full) {
  // TODO(crbug.com/1103421): Clean legacy implementation once structured
  // names are fully launched.
  if (structured_address::StructuredNamesEnabled())
    NOTREACHED();
  full_ = full;
  data_util::NameParts parts = data_util::SplitName(full);
  given_ = parts.given;
  middle_ = parts.middle;
  family_ = parts.family;
}

EmailInfo::EmailInfo() = default;

EmailInfo::EmailInfo(const EmailInfo& info) {
  *this = info;
}

EmailInfo::~EmailInfo() = default;

EmailInfo& EmailInfo::operator=(const EmailInfo& info) {
  if (this == &info)
    return *this;

  email_ = info.email_;
  return *this;
}

bool EmailInfo::operator==(const EmailInfo& other) const {
  return this == &other || email_ == other.email_;
}

void EmailInfo::GetSupportedTypes(ServerFieldTypeSet* supported_types) const {
  supported_types->insert(EMAIL_ADDRESS);
}

std::u16string EmailInfo::GetRawInfo(ServerFieldType type) const {
  if (type == EMAIL_ADDRESS)
    return email_;

  return std::u16string();
}

void EmailInfo::SetRawInfoWithVerificationStatus(ServerFieldType type,
                                                 const std::u16string& value,
                                                 VerificationStatus status) {
  DCHECK_EQ(EMAIL_ADDRESS, type);
  email_ = value;
}

CompanyInfo::CompanyInfo() = default;

CompanyInfo::CompanyInfo(const AutofillProfile* profile) : profile_(profile) {}

CompanyInfo::CompanyInfo(const CompanyInfo& info) {
  *this = info;
}

CompanyInfo::~CompanyInfo() = default;

CompanyInfo& CompanyInfo::operator=(const CompanyInfo& info) {
  if (this == &info)
    return *this;

  company_name_ = info.GetRawInfo(COMPANY_NAME);
  return *this;
}

bool CompanyInfo::operator==(const CompanyInfo& other) const {
  return this == &other ||
         GetRawInfo(COMPANY_NAME) == other.GetRawInfo(COMPANY_NAME);
}

void CompanyInfo::GetSupportedTypes(ServerFieldTypeSet* supported_types) const {
  supported_types->insert(COMPANY_NAME);
}

std::u16string CompanyInfo::GetRawInfo(ServerFieldType type) const {
  return IsValidOrVerified(company_name_) ? company_name_ : std::u16string();
}

void CompanyInfo::SetRawInfoWithVerificationStatus(ServerFieldType type,
                                                   const std::u16string& value,
                                                   VerificationStatus status) {
  DCHECK_EQ(COMPANY_NAME, type);
  company_name_ = value;
}

bool CompanyInfo::IsValidOrVerified(const std::u16string& value) const {
  // TODO(crbug/1117296): retrieve regular expressions dynamically.
  static const char* kBirthyearRe = "^(19|20)\\d{2}$";
  static const char* kSocialTitleRe =
      "^(Ms\\.?|Mrs\\.?|Mr\\.?|Miss|Mistress|Mister|"
      "Frau|Herr|"
      "Mlle|Mme|M\\.|"
      "Dr\\.?|Prof\\.?)$";
  return (profile_ && profile_->IsVerified()) ||
         (!MatchesPattern(value, base::UTF8ToUTF16(kBirthyearRe)) &&
          !MatchesPattern(value, base::UTF8ToUTF16(kSocialTitleRe)));
}

}  // namespace autofill
