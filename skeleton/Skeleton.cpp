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

    const string GET = "_Z3getPi";
    const string GET_ADDRESS = "_Z3getPPi";
    const string PUT = "_Z3putPii";
    const string PUT_ADDRESS = "_Z3putPPiS_";

    static char ID;
    Function *hookLoad;
    Function *hookLoadAddress;
    Function *hookLoadPointerAddress;
    Function *hookStore;
    Function *hookStoreAddress;
    Function *hookStorePointerAddress;

    vector<Instruction*> deleteList;
    vector<Instruction*> addList;

    SimpleDCE() : ModulePass(ID) {}

    // checks if the load return value is pointer
    bool isPointerLoad(LoadInst* value) {
      return value->getType()->getTypeID() == POINTER_TYPE_ID;
    }

    bool isPointerAddressLoad(LoadInst* value) {
      if (value->getType()->getTypeID() == POINTER_TYPE_ID) {
        PointerType* pt = cast<PointerType>(value->getType());
        return pt->getElementType()->getTypeID() == POINTER_TYPE_ID;
      }
      return false;
    }

    // checks if the value stored is pointer
    bool isPointerStore(Value* value) {
      return value->getType()->getTypeID() == POINTER_TYPE_ID;
    }

    bool isPointerAddressStore(Value* value) {
      if (value->getType()->getTypeID() == POINTER_TYPE_ID) {
        PointerType* pt = cast<PointerType>(value->getType());
        return pt->getElementType()->getTypeID() == POINTER_TYPE_ID;
      }
      return false;
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
      if (isPointerAddressLoad(ld)) {
        prepareForTranslation(hookLoadPointerAddress, print_load_arguments, instruction);
      } else if (isPointerLoad(ld)) {
        prepareForTranslation(hookLoadAddress, print_load_arguments, instruction);
      } else {
        prepareForTranslation(hookLoad, print_load_arguments, instruction);
      }
    }

    void changeStoreToPut(Instruction* instruction) {
      StoreInst* st = cast<StoreInst>(instruction);
      Value* value_to_store = st->getOperand(0);
      Value* address_of_store = st->getOperand(1);
      Value* print_store_arguments[] = { address_of_store, value_to_store };
      if (isPointerAddressStore(value_to_store)) {
        prepareForTranslation(hookStorePointerAddress, print_store_arguments, instruction);
      } else if (isPointerStore(value_to_store)) {
        prepareForTranslation(hookStoreAddress, print_store_arguments, instruction);
      } else {
        prepareForTranslation(hookStore, print_store_arguments, instruction);
      }
    }

    void prepareFunctionHooks(Module &M) {
      Type* pointer = Type::getInt32PtrTy(M.getContext());
      Type* pointerToPointer = PointerType::get(pointer, pointer->getPointerAddressSpace());
      Type* pointerToPointerToPointer = PointerType::get(pointerToPointer, pointerToPointer->getPointerAddressSpace());

      Constant *hookLoadFunc = M.getOrInsertFunction(GET, Type::getInt32Ty(M.getContext()), pointer);
      Constant *hookLoadAddressFunc = M.getOrInsertFunction(GET_ADDRESS, pointer, pointerToPointer);
      Constant *hookLoadPointerAddressFunc = M.getOrInsertFunction("_Z3getPPPi", pointerToPointer, pointerToPointerToPointer);
      Constant *hookStoreFunc = M.getOrInsertFunction(PUT,
                                                      Type::getVoidTy(M.getContext()),
                                                      pointer,
                                                      Type::getInt32Ty(M.getContext()));
      Constant *hookStoreAddressFunc = M.getOrInsertFunction(PUT_ADDRESS,
                                                             Type::getVoidTy(M.getContext()),
                                                             pointerToPointer,
                                                             pointer);
      Constant *hookStorePointerAddressFunc = M.getOrInsertFunction("_Z3putPPPiS0_",
                                                                    Type::getVoidTy(M.getContext()),
                                                                    pointerToPointerToPointer,
                                                                    pointerToPointer);

      hookLoad = cast<Function>(hookLoadFunc);
      hookLoadAddress = cast<Function>(hookLoadAddressFunc);
      hookLoadPointerAddress = cast<Function>(hookLoadPointerAddressFunc);
      hookStore = cast<Function>(hookStoreFunc);
      hookStoreAddress = cast<Function>(hookStoreAddressFunc);
      hookStorePointerAddress = cast<Function>(hookStorePointerAddressFunc);
    }

    virtual bool runOnModule(Module &M) {
      prepareFunctionHooks(M);

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
    X("skeletonpass", "Load and store translation"); // NOLINT
