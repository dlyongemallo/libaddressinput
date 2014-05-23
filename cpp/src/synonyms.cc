// Copyright (C) 2014 Google Inc.
//
// Licensed under the Apache License, Version 2.0 (the "License");
// you may not use this file except in compliance with the License.
// You may obtain a copy of the License at
//
// http://www.apache.org/licenses/LICENSE-2.0
//
// Unless required by applicable law or agreed to in writing, software
// distributed under the License is distributed on an "AS IS" BASIS,
// WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
// See the License for the specific language governing permissions and
// limitations under the License.

#include <libaddressinput/synonyms.h>

#include <libaddressinput/address_data.h>
#include <libaddressinput/address_field.h>
#include <libaddressinput/preload_supplier.h>

#include <cassert>
#include <cstddef>

#include "language.h"
#include "lookup_key.h"
#include "region_data_constants.h"
#include "rule.h"
#include "util/string_compare.h"

namespace i18n {
namespace addressinput {

Synonyms::Synonyms(const PreloadSupplier* supplier)
    : supplier_(supplier),
      compare_(new StringCompare) {
  assert(supplier_ != NULL);
}

Synonyms::~Synonyms() {}

void Synonyms::NormalizeForDisplay(AddressData* address) const {
  assert(address != NULL);
  assert(supplier_->IsLoaded(address->region_code));

  AddressData region_address;
  region_address.region_code = address->region_code;
  LookupKey parent_key;
  parent_key.FromAddress(region_address);
  const Rule* parent_rule = supplier_->GetRule(parent_key);
  assert(parent_rule != NULL);

  const Language& best_language =
      ChooseBestAddressLanguage(*parent_rule, Language(address->language_code));

  LookupKey lookup_key;
  for (size_t depth = 1; depth < arraysize(LookupKey::kHierarchy); ++depth) {
    AddressField field = LookupKey::kHierarchy[depth];
    if (address->IsFieldEmpty(field)) {
      return;
    }
    const std::string& field_value = address->GetFieldValue(field);
    bool no_match_found_yet = true;
    for (std::vector<std::string>::const_iterator
         key_it = parent_rule->GetSubKeys().begin();
         key_it != parent_rule->GetSubKeys().end(); ++key_it) {
      lookup_key.FromLookupKey(parent_key, *key_it);
      const Rule* rule = supplier_->GetRule(lookup_key);
      assert(rule != NULL);

      bool matches_latin_name =
          compare_->NaturalEquals(field_value, rule->GetLatinName());
      bool matches_local_name_id =
          compare_->NaturalEquals(field_value, *key_it) ||
          compare_->NaturalEquals(field_value, rule->GetName());
      if (matches_latin_name || matches_local_name_id) {
        address->SetFieldValue(
            field, matches_latin_name ? rule->GetLatinName() : *key_it);
        no_match_found_yet = false;
        parent_key.FromLookupKey(parent_key, *key_it);
        parent_rule = supplier_->GetRule(parent_key);
        assert(parent_rule != NULL);
        break;
      }
    }
    if (no_match_found_yet) {
      return;  // Abort search.
    }
  }
}

}  // namespace addressinput
}  // namespace i18n