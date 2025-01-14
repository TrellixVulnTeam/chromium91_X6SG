// Copyright 2020 The Tint Authors.
//
// Licensed under the Apache License, Version 2.0 (the "License");
// you may not use this file except in compliance with the License.
// You may obtain a copy of the License at
//
//     http://www.apache.org/licenses/LICENSE-2.0
//
// Unless required by applicable law or agreed to in writing, software
// distributed under the License is distributed on an "AS IS" BASIS,
// WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
// See the License for the specific language governing permissions and
// limitations under the License.

#include "src/reader/wgsl/parser_impl_test_helper.h"

namespace tint {
namespace reader {
namespace wgsl {
namespace {

TEST_F(ParserImplTest, SamplerType_Invalid) {
  auto p = parser("1234");
  auto t = p->sampler_type();
  EXPECT_FALSE(t.matched);
  EXPECT_FALSE(t.errored);
  EXPECT_EQ(t.value, nullptr);
  EXPECT_FALSE(p->has_error());
}

TEST_F(ParserImplTest, SamplerType_Sampler) {
  auto p = parser("sampler");
  auto t = p->sampler_type();
  EXPECT_TRUE(t.matched);
  EXPECT_FALSE(t.errored);
  ASSERT_NE(t.value, nullptr);
  ASSERT_TRUE(t->Is<type::Sampler>());
  EXPECT_FALSE(t->As<type::Sampler>()->IsComparison());
  EXPECT_FALSE(p->has_error());
}

TEST_F(ParserImplTest, SamplerType_ComparisonSampler) {
  auto p = parser("sampler_comparison");
  auto t = p->sampler_type();
  EXPECT_TRUE(t.matched);
  EXPECT_FALSE(t.errored);
  ASSERT_NE(t.value, nullptr);
  ASSERT_TRUE(t->Is<type::Sampler>());
  EXPECT_TRUE(t->As<type::Sampler>()->IsComparison());
  EXPECT_FALSE(p->has_error());
}

}  // namespace
}  // namespace wgsl
}  // namespace reader
}  // namespace tint
