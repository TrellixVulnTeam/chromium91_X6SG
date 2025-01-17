// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_AUTOFILL_CORE_BROWSER_AUTOFILL_TEST_UTILS_H_
#define COMPONENTS_AUTOFILL_CORE_BROWSER_AUTOFILL_TEST_UTILS_H_

#include <memory>
#include <string>
#include <vector>

#include "components/autofill/core/browser/autofill_field.h"
#include "components/autofill/core/browser/data_model/autofill_offer_data.h"
#include "components/autofill/core/browser/data_model/autofill_profile.h"
#include "components/autofill/core/browser/data_model/credit_card.h"
#include "components/autofill/core/browser/data_model/credit_card_cloud_token_data.h"
#include "components/autofill/core/browser/field_types.h"
#include "components/autofill/core/browser/form_structure.h"
#include "components/autofill/core/browser/proto/api_v1.pb.h"
#include "components/autofill/core/browser/proto/server.pb.h"

class PrefService;

namespace user_prefs {
class PrefRegistrySyncable;
}  // namespace user_prefs

namespace autofill {

class AutofillExternalDelegate;
class AutofillProfile;
class AutofillTable;
struct FormData;
struct FormFieldData;
struct FormDataPredictions;
struct FormFieldDataPredictions;

// Defined by pair-wise equality of all members.
bool operator==(const FormFieldDataPredictions& a,
                const FormFieldDataPredictions& b);

inline bool operator!=(const FormFieldDataPredictions& a,
                       const FormFieldDataPredictions& b) {
  return !(a == b);
}

// Holds iff the underlying FormDatas sans field values are equal and the
// remaining members are pairwise equal.
bool operator==(const FormDataPredictions& a, const FormDataPredictions& b);

inline bool operator!=(const FormDataPredictions& a,
                       const FormDataPredictions& b) {
  return !(a == b);
}

// Common utilities shared amongst Autofill tests.
namespace test {

// A compound data type that contains the type, the value and the verification
// status for a form group entry (an AutofillProfile).
struct FormGroupValue {
  ServerFieldType type;
  std::string value;
  structured_address::VerificationStatus verification_status =
      structured_address::VerificationStatus::kNoStatus;
};

// Convenience declaration for multiple FormGroup values.
using FormGroupValues = std::vector<FormGroupValue>;

// Creates a non-empty LocalFrameToken (no variation among different calls).
LocalFrameToken GetLocalFrameToken();

// Creates new, pairwise distinct FormRendererIds.
FormRendererId MakeFormRendererId();

// Creates new, pairwise distinct FieldRendererIds.
FieldRendererId MakeFieldRendererId();

// Creates new, pairwise distinct FormGlobalIds.
FormGlobalId MakeFormGlobalId();

// Creates new, pairwise distinct FieldGlobalIds.
FieldGlobalId MakeFieldGlobalId();

// Helper function to set values and verification statuses to a form group.
void SetFormGroupValues(FormGroup& form_group,
                        const std::vector<FormGroupValue>& values);

// Helper function to verify the expectation of values and verification
// statuses in a form group. If |ignore_status| is set, status checking is
// omitted.
void VerifyFormGroupValues(const FormGroup& form_group,
                           const std::vector<FormGroupValue>& values,
                           bool ignore_status = false);

const char kEmptyOrigin[] = "";

// The following methods return a PrefService that can be used for
// Autofill-related testing in contexts where the PrefService would otherwise
// have to be constructed manually (e.g., in unit tests within Autofill core
// code). The returned PrefService has had Autofill preferences registered on
// its associated registry.
std::unique_ptr<PrefService> PrefServiceForTesting();
std::unique_ptr<PrefService> PrefServiceForTesting(
    user_prefs::PrefRegistrySyncable* registry);

// Provides a quick way to populate a FormField with c-strings.
void CreateTestFormField(const char* label,
                         const char* name,
                         const char* value,
                         const char* type,
                         FormFieldData* field);

// Provides a quick way to populate a select field.
void CreateTestSelectField(const char* label,
                           const char* name,
                           const char* value,
                           const std::vector<const char*>& values,
                           const std::vector<const char*>& contents,
                           size_t select_size,
                           FormFieldData* field);

void CreateTestSelectField(const std::vector<const char*>& values,
                           FormFieldData* field);

// Provides a quick way to populate a datalist field.
void CreateTestDatalistField(const char* label,
                             const char* name,
                             const char* value,
                             const std::vector<const char*>& values,
                             const std::vector<const char*>& labels,
                             FormFieldData* field);

// Populates |form| with data corresponding to a simple address form.
// Note that this actually appends fields to the form data, which can be useful
// for building up more complex test forms. Another version of the function is
// provided in case the caller wants the vector of expected field |types|. Use
// |unique_id| optionally ensure that each form has its own signature.
void CreateTestAddressFormData(FormData* form, const char* unique_id = nullptr);
void CreateTestAddressFormData(FormData* form,
                               std::vector<ServerFieldTypeSet>* types,
                               const char* unique_id = nullptr);

// Populates |form| with data corresponding to a simple personal information
// form, including name and email, but no address-related fields. Use
// |unique_id| to optionally ensure that each form has its own signature.
void CreateTestPersonalInformationFormData(FormData* form,
                                           const char* unique_id = nullptr);

// Populates |form| with data corresponding to a simple credit card form.
// Note that this actually appends fields to the form data, which can be
// useful for building up more complex test forms. Use |unique_id| to optionally
// ensure that each form has its own signature.
void CreateTestCreditCardFormData(FormData* form,
                                  bool is_https,
                                  bool use_month_type,
                                  bool split_names = false,
                                  const char* unique_id = nullptr);

// Returns a full profile with valid info according to rules for Canada.
AutofillProfile GetFullValidProfileForCanada();

// Returns a full profile with valid info according to rules for China.
AutofillProfile GetFullValidProfileForChina();

// Returns a profile full of dummy info.
AutofillProfile GetFullProfile();

// Returns a profile full of dummy info, different to the above.
AutofillProfile GetFullProfile2();

// Returns a profile full of dummy info, different to the above.
AutofillProfile GetFullCanadianProfile();

// Returns an incomplete profile of dummy info.
AutofillProfile GetIncompleteProfile1();

// Returns an incomplete profile of dummy info, different to the above.
AutofillProfile GetIncompleteProfile2();

// Returns a verified profile full of dummy info.
AutofillProfile GetVerifiedProfile();

// Returns a server profile full of dummy info.
AutofillProfile GetServerProfile();

// Returns a server profile full of dummy info, different to the above.
AutofillProfile GetServerProfile2();

// Returns a credit card full of dummy info.
CreditCard GetCreditCard();

// Returns a credit card full of dummy info, different to the above.
CreditCard GetCreditCard2();

// Returns an expired credit card full of fake info.
CreditCard GetExpiredCreditCard();

// Returns an incomplete credit card full of fake info with card holder's name
// missing.
CreditCard GetIncompleteCreditCard();

// Returns a masked server card full of dummy info.
CreditCard GetMaskedServerCard();
CreditCard GetMaskedServerCardAmex();
CreditCard GetMaskedServerCardWithNickname();
CreditCard GetMaskedServerCardWithInvalidNickname();

// Returns a full server card full of dummy info.
CreditCard GetFullServerCard();

// Returns a randomly generated credit card of |record_type|. Note that the
// card is not guaranteed to be valid/sane from a card validation standpoint.
CreditCard GetRandomCreditCard(CreditCard::RecordType record_Type);

// Returns a credit card cloud token data full of dummy info.
CreditCardCloudTokenData GetCreditCardCloudTokenData1();

// Returns a credit card cloud token data full of dummy info, different from the
// one above.
CreditCardCloudTokenData GetCreditCardCloudTokenData2();

// Returns an autofill card linked offer data full of dummy info.
AutofillOfferData GetCardLinkedOfferData1();

// Returns an autofill card linked offer data full of dummy info, different from
// the one above.
AutofillOfferData GetCardLinkedOfferData2();

// A unit testing utility that is common to a number of the Autofill unit
// tests.  |SetProfileInfo| provides a quick way to populate a profile with
// c-strings.
void SetProfileInfo(AutofillProfile* profile,
                    const char* first_name,
                    const char* middle_name,
                    const char* last_name,
                    const char* email,
                    const char* company,
                    const char* address1,
                    const char* address2,
                    const char* dependent_locality,
                    const char* city,
                    const char* state,
                    const char* zipcode,
                    const char* country,
                    const char* phone,
                    bool finalize = true,
                    structured_address::VerificationStatus =
                        structured_address::VerificationStatus::kObserved);

// This one doesn't require the |dependent_locality|.
void SetProfileInfo(AutofillProfile* profile,
                    const char* first_name,
                    const char* middle_name,
                    const char* last_name,
                    const char* email,
                    const char* company,
                    const char* address1,
                    const char* address2,
                    const char* city,
                    const char* state,
                    const char* zipcode,
                    const char* country,
                    const char* phone,
                    bool finalize = true,
                    structured_address::VerificationStatus =
                        structured_address::VerificationStatus::kObserved);

void SetProfileInfoWithGuid(
    AutofillProfile* profile,
    const char* guid,
    const char* first_name,
    const char* middle_name,
    const char* last_name,
    const char* email,
    const char* company,
    const char* address1,
    const char* address2,
    const char* city,
    const char* state,
    const char* zipcode,
    const char* country,
    const char* phone,
    bool finalize = true,
    structured_address::VerificationStatus =
        structured_address::VerificationStatus::kObserved);

// A unit testing utility that is common to a number of the Autofill unit
// tests.  |SetCreditCardInfo| provides a quick way to populate a credit card
// with c-strings.
void SetCreditCardInfo(CreditCard* credit_card,
                       const char* name_on_card,
                       const char* card_number,
                       const char* expiration_month,
                       const char* expiration_year,
                       const std::string& billing_address_id);

// TODO(isherman): We should do this automatically for all tests, not manually
// on a per-test basis: http://crbug.com/57221
// Disables or mocks out code that would otherwise reach out to system services.
// Revert this configuration with |ReenableSystemServices|.
void DisableSystemServices(PrefService* prefs);

// Undoes the mocking set up by |DisableSystemServices|
void ReenableSystemServices();

// Sets |cards| for |table|. |cards| may contain full, unmasked server cards,
// whereas AutofillTable::SetServerCreditCards can only contain masked cards.
void SetServerCreditCards(AutofillTable* table,
                          const std::vector<CreditCard>& cards);

// Adds an element at the end of |possible_field_types| and
// |possible_field_types_validities| given |possible_type| and their
// corresponding |validity_state|.
void InitializePossibleTypesAndValidities(
    std::vector<ServerFieldTypeSet>& possible_field_types,
    std::vector<ServerFieldTypeValidityStatesMap>&
        possible_field_types_validities,
    const std::vector<ServerFieldType>& possible_type,
    const std::vector<AutofillDataModel::ValidityState>& validity_state = {});

// Fills the upload |field| with the information passed by parameter. If the
// value of a const char* parameter is NULL, the corresponding attribute won't
// be set at all, as opposed to being set to empty string.
void FillUploadField(AutofillUploadContents::Field* field,
                     unsigned signature,
                     const char* name,
                     const char* control_type,
                     const char* autocomplete,
                     unsigned autofill_type,
                     unsigned validity_state = 0);

void FillUploadField(AutofillUploadContents::Field* field,
                     unsigned signature,
                     const char* name,
                     const char* control_type,
                     const char* autocomplete,
                     const std::vector<unsigned>& autofill_type,
                     const std::vector<unsigned>& validity_state = {});

void FillUploadField(AutofillUploadContents::Field* field,
                     unsigned signature,
                     const char* name,
                     const char* control_type,
                     const char* autocomplete,
                     unsigned autofill_type,
                     const std::vector<unsigned>& validity_states);

// Creates the structure of signatures that would be encoded by
// FormStructure::EncodeUploadRequest() and FormStructure::EncodeQueryRequest()
// and consumed by FormStructure::ParseQueryResponse() and
// FormStructure::ParseApiQueryResponse().
//
// Perhaps a neater way would be to move this to TestFormStructure.
std::vector<FormSignature> GetEncodedSignatures(const FormStructure& form);
std::vector<FormSignature> GetEncodedSignatures(
    const std::vector<FormStructure*>& forms);

// Calls the required functions on the given external delegate to cause the
// delegate to display a popup.
void GenerateTestAutofillPopup(
    AutofillExternalDelegate* autofill_external_delegate);

std::string ObfuscatedCardDigitsAsUTF8(const std::string& str);

// Returns 2-digit month string, like "02", "10".
std::string NextMonth();
std::string LastYear();
std::string NextYear();
std::string TenYearsFromNow();

void AddFieldSuggestionToForm(
    const autofill::FormFieldData& field_data,
    ServerFieldType field_type,
    ::autofill::AutofillQueryResponse_FormSuggestion* form_suggestion);

// Adds |field_types| predictions of |field_data| to |form_suggestion| query
// response. Assumes int type can be cast to ServerFieldType.
void AddFieldPredictionsToForm(
    const autofill::FormFieldData& field_data,
    const std::vector<int>& field_types,
    ::autofill::AutofillQueryResponse_FormSuggestion* form_suggestion);

void AddFieldPredictionsToForm(
    const autofill::FormFieldData& field_data,
    const std::vector<ServerFieldType>& field_types,
    ::autofill::AutofillQueryResponse_FormSuggestion* form_suggestion);

}  // namespace test
}  // namespace autofill

#endif  // COMPONENTS_AUTOFILL_CORE_BROWSER_AUTOFILL_TEST_UTILS_H_
