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
// DCE the types in the program, removing unneeded fields of structs which can
// then lead to removal of more types and so forth.
//
// For convenience at the cost of efficiency this operates on the text format.
// That format has all types in a single place and allows types to be modified,
// unlike Binaryen IR which is designed to assume types are immutable.
//

#include "ir/module-utils.h"
#include "ir/names.h"
#include "pass.h"
#include "support/colors.h"
#include "wasm-binary.h"
#include "wasm-s-parser.h"
#include "wasm.h"

using namespace std;

namespace wasm {

struct TypeDCE : public Pass {
  void run(PassRunner* runner, Module* module) override {
    // Keep DCEing while we can.
    while (iteration(*module)) {}
  }

  // Global state of what is the next type.field we will attempt to DCE.
  Name nextType;
  Index nextField;

  bool iteration(Module& wasm) {
std::cout << "iter\n";
    // Find all the types.
    std::vector<HeapType> types;
    std::unordered_map<HeapType, Index> typeIndices;
    ModuleUtils::collectHeapTypes(wasm, types, typeIndices);

    // Ensure all types and fields are named, so that we can refer to them
    // easily in the text format.
    ensureNames(wasm, types);

    // Generate the text format and parse it.
    std::stringstream stream;
    Colors::setEnabled(false);
    stream << wasm;
    Colors::setEnabled(true);
std::cout << "iter1\n";
    SExpressionParser parser(const_cast<char*>(stream.str().c_str()));
std::cout << "iter2\n";
    Element& root = *(*parser.root)[0];
    // Prune the next type.field, and see if we are left with a valid module.
    if (!pruneNext(root)) {
      // Nothing to prune.
      return false;
    }

    Module pruned;
    pruned.features = wasm.features;
std::cout << "iter3\n";
std::cout << root << '\n';
  try {
    SExpressionWasmBuilder builder(pruned, root, IRProfile::Normal);
  } catch (ParseException& p) {
    p.dump(std::cerr);
    Fatal() << "error in parsing input";
  }
std::cout << "iter4\n";

    if (!wasm::WasmValidator().validate(pruned)) {
      // Move on to try the next field.
      nextField++;
      return false;
    }

    // Success!
    ModuleUtils::clearModule(wasm);
    ModuleUtils::copyModule(pruned, wasm);

    // Do not increment nextField - we just pruned at the current index, so
    // there will be a new field in that position.
    return true;
  }

  void ensureNames(Module& wasm, std::vector<HeapType>& types) {
    // Type names.
    std::unordered_set<Name> used;
    for (auto type : types) {
      auto name = wasm.typeNames[type].name;
      if (name.is()) {
        used.insert(name);
      }
    }
    for (auto type : types) {
      auto& name = wasm.typeNames[type].name;
      if (!name.is()) {
        name = Names::getValidName("$type", [&](Name test) {
          return used.count(test) == 0;
        });
      }
    }

    // Field names.
    for (auto type : types) {
      if (!type.isStruct()) {
        continue;
      }
      auto& fieldNames = wasm.typeNames[type].fieldNames;
      std::unordered_set<Name> used;
      for (auto& kv : fieldNames) {
        used.insert(kv.second);
      }
      auto& fields = type.getStruct().fields;
      for (Index i = 0; i < fields.size(); i++) {
        if (fieldNames.count(i) == 0) {
          fieldNames[i] = Names::getValidName("$field", [&](Name test) {
            return used.count(test) == 0;
          });
        }
      }
    }
  }

  // Finds and prunes the next thing. Returns true if successful and false if
  // nothing could be found to prune.
  bool pruneNext(Element& root) {
    // Look for nextType.nextField. It is possible the type does not exist, if
    // it was optimized out; it is also possible the next field is one past the
    // end. In both cases simply continue forward in order.
    Element* found = nullptr;
    Name foundName;
    for (Index i = 0; i < root.size(); i++) {
      auto& item = *root[i];
std::cout << "pruneloop " << item << '\n';
      // Look for (type ..)
      if (!item.isList() || !item.size() || *item[0] != TYPE) {
        continue;
      }
      Name name = item[1]->str();
      // If we know what to look for (this is not the very first iteration), and
      // this is too small, ignore it. We want the first item >= that the
      // target.
      if (nextType.is() && name < nextType) {
        continue;
      }
std::cout << "  l1\n";
      // Look for the name we want, or the first after it.
      if (!nextType.is() || !found || name < foundName) {
std::cout << "  l2\n";
        // Look for (struct ..), and not (func ..) etc.
        auto& inner = *item[2];
std::cout << inner << '\n';
        if (inner[0]->str() == "struct") {
          found = &inner;
          foundName = name;
        }
      }
    }
    if (!found) {
std::cout << "none\n";
      return false;
    }
std::cout << "found! " << foundName << "\n";
    if (!nextType.is() || nextType < foundName) {
      // We did not find the exact type, but one after it, or this is the very
      // first iteration, so reset the field.
      nextField = 0;
    }
    // "found" is a type declaration, something like this:
    //      (type $struct.A (struct (field ..) (field ..) ..))
    auto& struct_ = *found;
std::cout << struct_ << '\n';
    assert(struct_[0]->str() == "struct");
//    auto numFields = struct_.size() - 1;
    return true;
  }
};

Pass* createTypeDCEPass() { return new TypeDCE(); }

} // namespace wasm