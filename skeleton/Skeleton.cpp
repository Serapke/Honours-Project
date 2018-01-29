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

    const int POINTER_TYPE_ID = 15;

    static char ID;
    Function *hookLoad;
    Function *hookLoadAddress;
    Function *hookStore;
    Function *hookStoreAddress;

    vector<Instruction*> deleteList;
    vector<Instruction*> addList;

    SimpleDCE() : ModulePass(ID) {}

    // checks if the load return value is pointer
    bool isPointerLoad(LoadInst* value) {
      return value->getType()->getTypeID() == POINTER_TYPE_ID;
    }

    // checks if the value stored is pointer
    bool isPointerStore(Value* value) {
      return value->getType()->getTypeID() == POINTER_TYPE_ID;
    }

    // TODO: improve the detection of gRPC code
    // gRPC code starts with a function, which name includes 'functions.cpp' substring
    bool isgRPCCode(Function* func) {
      return strstr(func->getName().data(), "functions.cpp");
    }

    // replaces all of the instructions from oldInstructions list to the ones in newInstructions
    void replaceInstructions(vector<Instruction*> oldInstructions, vector<Instruction*> newInstructions) {
      if (oldInstructions.size() != newInstructions.size()) {
        errs() << "Error! Old and new instruction lists differ in size!\n";
        return;
      }
      for (int i = 0; i < newInstructions.size(); i++) {
        Instruction* newInst = newInstructions[i];
        Instruction* oldInst = oldInstructions[i];
        ReplaceInstWithInst(oldInst, newInst);
      }
    }

    void prepareForTranslation(Function* hook, ArrayRef<Value*> args, Instruction* instructionToChange) {
      Instruction* functionCallInst = CallInst::Create(hook, args, "");
      addList.push_back(functionCallInst);
      deleteList.push_back(instructionToChange);
    }

    void changeLoadToGet(Instruction* instruction) {
      LoadInst* ld = cast<LoadInst>(instruction);
      Value* address_of_load = ld->getOperand(0);
      Value* print_load_arguments[] = { address_of_load };
      if (isPointerLoad(ld)) {
        prepareForTranslation(hookLoadAddress, print_load_arguments, instruction);
        //CallInst::Create(hookLoadAddress, print_load_arguments, "")->insertAfter(ld);
      } else {
        prepareForTranslation(hookLoad, print_load_arguments, instruction);
      }
    }

    void changeStoreToPut(Instruction* instruction) {
      StoreInst* st = cast<StoreInst>(instruction);
      Value* value_to_store = st->getOperand(0);
      Value* address_of_store = st->getOperand(1);
      Value* print_store_arguments[] = { address_of_store, value_to_store };
      if (isPointerStore(value_to_store)) {
        prepareForTranslation(hookStoreAddress, print_store_arguments, instruction);
        //CallInst::Create(hookStoreAddress, print_store_arguments, "")->insertAfter(st);
      } else {
        prepareForTranslation(hookStore, print_store_arguments, instruction);
      }
    }

    virtual bool runOnModule(Module &M) {
      Type* pointerType = Type::getInt32PtrTy(M.getContext());

      Constant *hookLoadFunc = M.getOrInsertFunction("_Z3getPi", Type::getInt32Ty(M.getContext()), Type::getInt32PtrTy(M.getContext()));
      Constant *hookLoadAddressFunc = M.getOrInsertFunction("_Z3getPPi", Type::getInt32PtrTy(M.getContext()), PointerType::get(pointerType, pointerType->getPointerAddressSpace()));
      Constant *hookStoreFunc = M.getOrInsertFunction("_Z3putPii", Type::getVoidTy(M.getContext()), Type::getInt32PtrTy(M.getContext()), Type::getInt32Ty(M.getContext()));
      Constant *hookStoreAddressFunc = M.getOrInsertFunction("_Z3putPPiS_", Type::getVoidTy(M.getContext()), PointerType::get(pointerType, pointerType->getPointerAddressSpace()), pointerType);

      hookLoad = cast<Function>(hookLoadFunc);
      hookLoadAddress = cast<Function>(hookLoadAddressFunc);
      hookStore = cast<Function>(hookStoreFunc);
      hookStoreAddress = cast<Function>(hookStoreAddressFunc);

      for(Module::iterator F = M.begin(), E = M.end(); F!= E; ++F) {
        // if reached gRPC code, translation is ended
        if (isgRPCCode(&*F)) {
            break;
        } 
        for(Function::iterator BB = F->begin(), E = F->end(); BB != E; ++BB) {
          SimpleDCE::runOnBasicBlock(BB);
        }
        // errs() << *F << "\n";
      }

      replaceInstructions(deleteList, addList);
      return true;
    }
    virtual bool runOnBasicBlock(Function::iterator &BB) {
      for (BasicBlock::iterator BI = BB->begin(), BE = BB->end(); BI != BE; ++BI) {
        if (isa<LoadInst>(&*BI)) {
          changeLoadToGet(&*BI);
        } else if (isa<StoreInst>(&*BI)) {
          changeStoreToPut(&*BI);
        }
      }
      return true;
    }
  };
}
char SimpleDCE::ID = 0;
static RegisterPass<SimpleDCE>
    X("skeletonpass", "Simple dead code elimination"); // NOLINT
