/*
 * Copyright 2021 WebAssembly Community Group participants
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 *     http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 */

//
// Write the module to binary, and load it from there. This is useful in
// testing to check for the effects of roundtripping in a single wasm-opt
// parameter.
//

#include "ir/module-utils.h"
#include "pass.h"
#include "wasm-binary.h"
#include "wasm-io.h"
#include "wasm.h"

namespace wasm {

struct LLVMOpt : public Pass {
  void run(PassRunner* runner, Module* module) override {
    BufferWithRandomAccess buffer;
    // Save features, which would not otherwise make it through a round trip if
    // the target features section has been stripped.
    auto features = module->features;
    // Write the module to a temp file.
    WasmBinaryWriter(module, buffer).write();
    FILE* temp = std::tmpfile();
    std::fwrite(buffer.data(), buffer.size(), 1, temp);
    std::fclose(temp);
    // Clear the module in preparation for reading, and read it.
    ModuleUtils::clearModule(*module);
    auto input = buffer.getAsChars();
    WasmBinaryBuilder parser(*module, input);
    try {
      parser.read();
    } catch (ParseException& p) {
      p.dump(std::cerr);
      std::cerr << '\n';
      Fatal() << "error in parsing wasm binary";
    }
    // Reapply features
    module->features = features;
  }
};

Pass* createLLVMOptPass() { return new LLVMOpt(); }

} // namespace wasm