// Copyright 2020 The Tint Authors
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

#include "src/reader/spirv/spirv_tools_helpers_test.h"

#include "gtest/gtest.h"
#include "spirv-tools/libspirv.hpp"

namespace tint {
namespace reader {
namespace spirv {
namespace test {

std::vector<uint32_t> Assemble(const std::string& spirv_assembly) {
  // TODO(dneto): Use ScopedTrace?

  // (The target environment doesn't affect assembly.
  spvtools::SpirvTools tools(SPV_ENV_UNIVERSAL_1_0);
  std::stringstream errors;
  std::vector<uint32_t> result;
  tools.SetMessageConsumer([&errors](spv_message_level_t, const char*,
                                     const spv_position_t& position,
                                     const char* message) {
    errors << "assembly error:" << position.line << ":" << position.column
           << ": " << message;
  });

  const auto success = tools.Assemble(
      spirv_assembly, &result, SPV_TEXT_TO_BINARY_OPTION_PRESERVE_NUMERIC_IDS);
  EXPECT_TRUE(success) << errors.str();

  return result;
}

std::string AssembleFailure(const std::string& spirv_assembly) {
  // TODO(dneto): Use ScopedTrace?

  // (The target environment doesn't affect assembly.
  spvtools::SpirvTools tools(SPV_ENV_UNIVERSAL_1_0);
  std::stringstream errors;
  std::vector<uint32_t> result;
  tools.SetMessageConsumer([&errors](spv_message_level_t, const char*,
                                     const spv_position_t& position,
                                     const char* message) {
    errors << position.line << ":" << position.column << ": " << message;
  });

  const auto success = tools.Assemble(
      spirv_assembly, &result, SPV_TEXT_TO_BINARY_OPTION_PRESERVE_NUMERIC_IDS);
  EXPECT_FALSE(success);

  return errors.str();
}

std::string Disassemble(const std::vector<uint32_t>& spirv_module) {
  spvtools::SpirvTools tools(SPV_ENV_UNIVERSAL_1_0);
  std::stringstream errors;
  tools.SetMessageConsumer([&errors](spv_message_level_t, const char*,
                                     const spv_position_t& position,
                                     const char* message) {
    errors << "disassmbly error:" << position.line << ":" << position.column
           << ": " << message;
  });

  std::string result;
  const auto success = tools.Disassemble(spirv_module, &result);
  EXPECT_TRUE(success) << errors.str();

  return result;
}

}  // namespace test
}  // namespace spirv
}  // namespace reader
}  // namespace tint
