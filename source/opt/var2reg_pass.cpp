// Copyright (c) 2016 Google Inc.
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

#include "var2reg_pass.h"

#include "def_use_manager.h"
#include "basic_block.h"
#include "function.h"
#include "instruction.h"

#include "spirv/1.1/spirv.h"

#include <unordered_set>

namespace spvtools {
namespace opt {

  struct stack_state {
    ir::BasicBlock* blk;

    std::unordered_map<ir::Instruction*, ir::Instruction*> end_state;
  };

  ir::BasicBlock* find_block_for_id(ir::Function*func, uint32_t id) {
    for (ir::BasicBlock& blk : *func) {
      if (blk.begin()->result_id() == id)
        return &blk;
    }
    return NULL;
  }

  void visit_blocks(analysis::DefUseManager &def_use, std::unordered_map<ir::Instruction*, ir::Instruction*> &working_list, ir::Function*func) {

    std::vector<stack_state> stack;
    stack.push_back(stack_state{ &*func->begin(),working_list });

    while (!stack.empty()) {
      stack_state &state = stack.back();

      auto blk = find_block_for_id(func, 0);
    }

  }

  bool Mem2RegPass::Process(ir::Module* module) {
    analysis::DefUseManager def_use(module);


    bool modified = false;

    for (auto& func : *module) {
      std::unordered_map<ir::Instruction*, ir::Instruction*> working_list;
      ir::BasicBlock* startBlock = &*func.begin();

      for (auto &instruction : *startBlock) {
        if (instruction.opcode() == SpvOpVariable) {
          working_list.insert(std::make_pair<ir::Instruction*, ir::Instruction*>(&instruction, 0));

        repeat_from_scratch:

          std::unordered_set<ir::Instruction*> geps;
          bool loaded = false;
          bool passedToFunc = false;
          bool storedCount = 0;

          analysis::UseList* uselist = def_use.GetUses(instruction.result_id());

          for (auto& use : *uselist) {
            switch (use.inst->opcode()) {
            case SpvOpAccessChain:
            case SpvOpInBoundsAccessChain: {
              geps.insert(use.inst);
            }break;
            case SpvOpCopyMemory: {
              if (use.operand_index == 0) storedCount++;
              if (use.operand_index == 1) loaded = true;
            }break;
            case SpvOpCopyObject: {
              def_use.ReplaceAllUsesWith(use.inst->result_id(), instruction.result_id());
              def_use.KillInst(use.inst);
              modified = true;

              //iterator we are using just changed, have to break out
              goto repeat_from_scratch;
            }break;
            case SpvOpPhi: {
              //not legal but bail anyway
              passedToFunc = true;
            }break;
            case SpvOpFunctionCall:
            case SpvOpExtInst: {
              //passed to function/extension function not much to do...
              passedToFunc = true;
            }break;

            default: {
              //some unkown use ... bail out
              passedToFunc = true;
            }break;
            }
          }

          //can't remove variable when used as parameter.
          if (passedToFunc)break;
          //TODO(ratchet freak): try and optimize loads and stores from/to it

          if (!loaded && geps.empty()) {
            //never loaded from -> can kill all stores
            for (auto& use : *uselist) {
              if (use.inst->type_id())
                def_use.KillInst(use.inst);
            }
            working_list.erase(working_list.find(&instruction));
            def_use.KillInst(&instruction);
            modified = true;
          }

          visit_blocks(def_use, working_list, &func);

        }
      }
    }

    return modified;
  }

}  // namespace opt
}  // namespace spvtools
