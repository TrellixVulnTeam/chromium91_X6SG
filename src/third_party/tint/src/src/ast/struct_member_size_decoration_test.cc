// Copyright 2021 The Tint Authors.
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

#include "src/ast/struct_member_size_decoration.h"

#include "src/ast/test_helper.h"

namespace tint {
namespace ast {
namespace {

using StructMemberOffsetDecorationTest = TestHelper;

TEST_F(StructMemberOffsetDecorationTest, Creation) {
  auto* d = create<StructMemberSizeDecoration>(2);
  EXPECT_EQ(2u, d->size());
}

TEST_F(StructMemberOffsetDecorationTest, Is) {
  auto* d = create<StructMemberSizeDecoration>(2);
  EXPECT_TRUE(d->Is<StructMemberSizeDecoration>());
}

}  // namespace
}  // namespace ast
}  // namespace tint
