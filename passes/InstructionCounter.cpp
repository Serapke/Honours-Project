#include "llvm/Pass.h"
#include "llvm/IR/Function.h"
#include "llvm/IR/Module.h"
#include "llvm/Support/raw_ostream.h"
#include "llvm/IR/LegacyPassManager.h"
#include "llvm/Transforms/IPO/PassManagerBuilder.h"
using namespace llvm;

namespace {
  struct InstructionCounter : public ModulePass {
    static char ID;
    int count;
    InstructionCounter() : ModulePass(ID) {}

    int countInFunction(Function *F) {
      int n = 0;
      outs() << F->getName();
      for (Function::iterator BB = F->begin(), E = F->end(); BB != E; ++BB) {
        for (BasicBlock::iterator BI = BB->begin(), BE = BB->end(); BI != BE; ++BI) {
          n++;
        }
      }
      outs() << " : " << n << "\n";
      return n;
    }

    virtual bool runOnModule(Module &M) {
      count = 0;
      for(Module::iterator F = M.begin(), E = M.end(); F!= E; ++F) {
        count += countInFunction(&*F);
      }
      outs() << "Instruction count: " << count << "\n";
    }
  };
}
char InstructionCounter::ID = 0;
static RegisterPass<InstructionCounter>
    X("instruction-counter", "Count instructions"); // NOLINT

