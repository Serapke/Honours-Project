#include "llvm/Pass.h"
#include "llvm/IR/Module.h"
#include "llvm/IR/Function.h"
#include "llvm/Support/raw_ostream.h"
#include "llvm/IR/LegacyPassManager.h"
#include "llvm/Transforms/IPO/PassManagerBuilder.h"
#include "llvm/IR/Instruction.h"
#include "llvm/IR/Instructions.h"
#include "llvm/IR/Constants.h"
#include "llvm/Transforms/Utils/BasicBlockUtils.h"
#include <set>
using namespace llvm;
using namespace std;

namespace {
  struct SimpleDCE : public ModulePass {
    static char ID;
    Function *hookLoad;
    Function *hookLoadAddress;
    Function *hookStore;
    Function *hookStoreAddress;

    SimpleDCE() : ModulePass(ID) {}

    virtual bool runOnModule(Module &M) {
      Type* pointerType = Type::getInt32PtrTy(M.getContext());

      Constant *hookLoadFunc = M.getOrInsertFunction("_Z3getPi", Type::getInt32Ty(M.getContext()), Type::getInt32PtrTy(M.getContext()));
//      Constant *hookLoadAddressFunc = M.getOrInsertFunction("printLoadAddress", Type::getVoidTy(M.getContext()), PointerType::get(pointerType, pointerType->getPointerAddressSpace()), pointerType);
      Constant *hookStoreFunc = M.getOrInsertFunction("_Z3putPii", Type::getVoidTy(M.getContext()), Type::getInt32PtrTy(M.getContext()), Type::getInt32Ty(M.getContext()));
//      Constant *hookStoreAddressFunc = M.getOrInsertFunction("printStoreAddress", Type::getVoidTy(M.getContext()), PointerType::get(pointerType, pointerType->getPointerAddressSpace()), pointerType);

      hookLoad = cast<Function>(hookLoadFunc);
//      hookLoadAddress = cast<Function>(hookLoadAddressFunc);
      hookStore = cast<Function>(hookStoreFunc);
//      hookStoreAddress = cast<Function>(hookStoreAddressFunc);

      bool endOfUser = false;
      bool changed = true;

      for(Module::iterator F = M.begin(), E = M.end(); F!= E; ++F) {
        Function* func = &*F;
        if (strstr(func->getName().data(), "functions.cpp")) {
            break;
        } 
        for(Function::iterator BB = F->begin(), E = F->end(); BB != E; ++BB) {
          changed |= SimpleDCE::runOnBasicBlock(BB);
//          for(BasicBlock::iterator BI = BB->begin(), BE = BB->end(); BI != BE; ++BI) {
//            errs() << *BI << "\n";
//          }
        }
        errs() << *F << "\n";
      }

      return changed;
    }
    virtual bool runOnBasicBlock(Function::iterator &BB) {
      vector<Instruction*> deleteList;
      vector<Instruction*> addList; 
      Function* f =  BB->getParent();
      if (f == hookLoad || f == hookStore || f == hookLoadAddress || f == hookStoreAddress) {
        return false;
      }
      for (BasicBlock::iterator BI = BB->begin(), BE = BB->end(); BI != BE; ++BI) {
        if (isa<LoadInst>(&*BI)) {
          LoadInst *ld = cast<LoadInst>(&*BI);
          Value* address_of_load = ld->getOperand(0);
          Value *print_load_arguments[] = { address_of_load };
          bool pointerToPointer = ld->getType()->getTypeID() == 15;
          if (pointerToPointer) {
            //CallInst::Create(hookLoadAddress, print_load_arguments, "")->insertAfter(ld);
          } else {
            Instruction* newInst = CallInst::Create(hookLoad, print_load_arguments, "");
            addList.push_back(newInst);
            deleteList.push_back(&*BI);
//            BasicBlock::iterator ii(oldInst);
           // ReplaceInstWithInst(ld, newInst);
            // deleteList.push_back(&*BI);
          }
        } else if (isa<StoreInst>(&*BI)) {
          StoreInst *st = cast<StoreInst>(&*BI);
          Value* value_to_store = st->getOperand(0);
          Value* address_of_store = st->getOperand(1);
          Value *print_store_arguments[] = { address_of_store, value_to_store };
          bool pointerToPointer = value_to_store->getType()->getTypeID() == 15;
          if (pointerToPointer) {
            //CallInst::Create(hookStoreAddress, print_store_arguments, "")->insertAfter(st);
          } else {
            Instruction* newInst = CallInst::Create(hookStore, print_store_arguments, "");
            addList.push_back(newInst);
            deleteList.push_back(&*BI);
          }
        }

      }
      for (int i = 0; i < addList.size(); i++) {
        Instruction* newInst = addList[i];
        Instruction* oldInst = deleteList[i];
        ReplaceInstWithInst(oldInst, newInst);
      }
      return true;
    }
  };
}
char SimpleDCE::ID = 0;
static RegisterPass<SimpleDCE>
    X("skeletonpass", "Simple dead code elimination"); // NOLINT
