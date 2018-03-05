#include "llvm/Pass.h"
#include "llvm/IR/Module.h"
#include "llvm/IR/Function.h"
#include "llvm/Support/raw_ostream.h"
#include "llvm/IR/LegacyPassManager.h"
#include "llvm/Transforms/IPO/PassManagerBuilder.h"
#include "llvm/IR/Instruction.h"
#include "llvm/IR/Instructions.h"
#include "llvm/IR/IntrinsicInst.h"
#include "llvm/IR/Constants.h"
#include "llvm/Transforms/Utils/BasicBlockUtils.h"
#include <set>
using namespace llvm;
using namespace std;

namespace {

  struct SimpleDCE : public ModulePass {

    const int POINTER_TYPE_ID = 15;

    const string GET = "_Z3getPi";
    const string GET_64 = "_Z3getPl";
    const string GET_ADDRESS = "_Z3getPPi";
    const string GET_ADDRESS_8 = "_Z3getPPa";
    const string GET_POINTER_ADDRESS = "_Z3getPPPi";
    const string PUT = "_Z3putPii";
    const string PUT64 = "_Z3putPll";
    const string PUT_ADDRESS = "_Z3putPPiS_";
    const string PUT_ADDRESS_8 = "_Z3putPPaS_";
    const string PUT_POINTER_ADDRESS = "_Z3putPPPiS0_";
    const string MEMCPY = "_Z6memcpyPvPKvm";
    const string MEMSET = "_Z6memsetPvim";
    const string MEMSET_CHAR = "_Z6memsetPvcm";

    static char ID;
    Function *hookLoad;
    Function *hookLoad64;
    Function *hookLoadAddress;
    Function *hookLoadAddress8;
    Function *hookLoadPointerAddress;
    Function *hookStore;
    Function *hookStore64;
    Function *hookStoreAddress;
    Function *hookStoreAddress8;
    Function *hookStorePointerAddress;
    Function *hookMemCpy;
    Function *hookMemSet;
    Function *hookMemSetChar;

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
      if (isPointerStore(value)) {
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
        if (PointerType* PT = cast<PointerType>(ld->getType())) {
          if (IntegerType* IT = cast<IntegerType>(PT->getPointerElementType())) {
            if (IT->getBitWidth() == 8) {
              prepareForTranslation(hookLoadAddress8, print_load_arguments, instruction);
            } else if (IT->getBitWidth() == 32) {
              prepareForTranslation(hookLoadAddress, print_load_arguments, instruction);
            }
          }
        }
      } else {
        if (IntegerType* IT = cast<IntegerType>(ld->getType())) {
          if (IT->getBitWidth() == 64) {
            prepareForTranslation(hookLoad64, print_load_arguments, instruction);
          } else if (IT->getBitWidth() == 32) {
            prepareForTranslation(hookLoad, print_load_arguments, instruction);
          }
        }
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
        if (PointerType* PT = cast<PointerType>(value_to_store->getType())) {
          if (IntegerType* IT = cast<IntegerType>(PT->getPointerElementType())) {
            if (IT->getBitWidth() == 8) {
              prepareForTranslation(hookStoreAddress8, print_store_arguments, instruction);
            } else if (IT->getBitWidth() == 32) {
              prepareForTranslation(hookStoreAddress, print_store_arguments, instruction);
            }
          }
        }
      } else {
        if (IntegerType* IT = cast<IntegerType>(value_to_store->getType())) {
          if (IT->getBitWidth() == 64)
            prepareForTranslation(hookStore64, print_store_arguments, instruction);
          else if (IT->getBitWidth() == 32)
            prepareForTranslation(hookStore, print_store_arguments, instruction);
        }
      }
    }

    void changeMemCpy(Instruction* instruction) {
      MemCpyInst* mc = cast<MemCpyInst>(instruction);
      Value* dest = mc->getOperand(0);
      Value* src = mc->getOperand(1);
      Value* size = mc->getOperand(2);
      Value* memcpy_arguments[] = { dest, src, size };
      prepareForTranslation(hookMemCpy, memcpy_arguments, instruction);
    }

    void changeMemSet(Instruction* instruction) {
      MemSetInst* ms = cast<MemSetInst>(instruction);
      Value* ptr = ms->getOperand(0);
      Value* value = ms->getOperand(1);
      Value* size = ms->getOperand(2);
      Value* memset_arguments[] = { ptr, value, size };
      if (IntegerType* IT = cast<IntegerType>(value->getType())) {
        if (IT->getBitWidth() == 8) {
          prepareForTranslation(hookMemSetChar, memset_arguments, instruction);
        } else if (IT->getBitWidth() == 32) {
          prepareForTranslation(hookMemSet, memset_arguments, instruction);
        }
      }
    }

    void prepareFunctionHooks(Module &M) {
      Type* intPointer = Type::getInt32PtrTy(M.getContext());
      Type* intPointerToPointer = PointerType::get(intPointer, intPointer->getPointerAddressSpace());
      Type* pointerToPointerToPointer = PointerType::get(intPointerToPointer, intPointerToPointer->getPointerAddressSpace());

      Type* int8 = IntegerType::get(M.getContext(), 8);
      Type* intPointer8 = PointerType::getUnqual(int8);
      Type* intPointerToPointer8 = PointerType::get(intPointer8, intPointer8->getPointerAddressSpace());

      Type* int64 = IntegerType::get(M.getContext(), 64);
      Type* intPointer64 = PointerType::getUnqual(int64);

      Constant *hookLoadFunc = M.getOrInsertFunction(GET, Type::getInt32Ty(M.getContext()), intPointer);
      Constant *hookLoadFunc64 = M.getOrInsertFunction(GET_64, int64, intPointer64);
      Constant *hookLoadAddressFunc = M.getOrInsertFunction(GET_ADDRESS, intPointer, intPointerToPointer);
      Constant *hookLoadAddressFunc8 = M.getOrInsertFunction(GET_ADDRESS_8, intPointer8, intPointerToPointer8);
      Constant *hookLoadPointerAddressFunc = M.getOrInsertFunction(GET_POINTER_ADDRESS, intPointerToPointer, pointerToPointerToPointer);
      Constant *hookStoreFunc = M.getOrInsertFunction(PUT,
                                                      Type::getVoidTy(M.getContext()),
                                                      intPointer,
                                                      Type::getInt32Ty(M.getContext()));
      Constant *hookStoreFunc64 = M.getOrInsertFunction(PUT64,
                                                      Type::getVoidTy(M.getContext()),
                                                      intPointer64,
                                                      int64);
      Constant *hookStoreAddressFunc = M.getOrInsertFunction(PUT_ADDRESS,
                                                             Type::getVoidTy(M.getContext()),
                                                             intPointerToPointer,
                                                             intPointer);
      Constant *hookStoreAddressFunc8 = M.getOrInsertFunction(PUT_ADDRESS_8,
                                                              Type::getVoidTy(M.getContext()),
                                                              intPointerToPointer8,
                                                              intPointer8);
      Constant *hookStorePointerAddressFunc = M.getOrInsertFunction(PUT_POINTER_ADDRESS,
                                                                    Type::getVoidTy(M.getContext()),
                                                                    pointerToPointerToPointer,
                                                                    intPointerToPointer);
      Constant *hookMemCpyFunc = M.getOrInsertFunction(MEMCPY,
                                                    Type::getInt8PtrTy(M.getContext()),
                                                    Type::getInt8PtrTy(M.getContext()),
                                                    Type::getInt8PtrTy(M.getContext()),
                                                    Type::getInt64Ty(M.getContext()));
      Constant *hookMemSetFunc = M.getOrInsertFunction(MEMSET,
                                                   Type::getInt8PtrTy(M.getContext()),
                                                   Type::getInt8PtrTy(M.getContext()),
                                                   Type::getInt32Ty(M.getContext()),
                                                   Type::getInt64Ty(M.getContext()));
      Constant *hookMemSetCharFunc = M.getOrInsertFunction(MEMSET_CHAR,
                                                       Type::getInt8PtrTy(M.getContext()),
                                                       Type::getInt8PtrTy(M.getContext()),
                                                       Type::getInt8Ty(M.getContext()),
                                                       Type::getInt64Ty(M.getContext()));

      hookLoad = cast<Function>(hookLoadFunc);
      hookLoad64 = cast<Function>(hookLoadFunc64);
      hookLoadAddress = cast<Function>(hookLoadAddressFunc);
      hookLoadAddress8 = cast<Function>(hookLoadAddressFunc8);
      hookLoadPointerAddress = cast<Function>(hookLoadPointerAddressFunc);
      hookStore = cast<Function>(hookStoreFunc);
      hookStore64 = cast<Function>(hookStoreFunc64);
      hookStoreAddress = cast<Function>(hookStoreAddressFunc);
      hookStoreAddress8 = cast<Function>(hookStoreAddressFunc8);
      hookStorePointerAddress = cast<Function>(hookStorePointerAddressFunc);
      hookMemCpy = cast<Function>(hookMemCpyFunc);
      hookMemSet = cast<Function>(hookMemSetFunc);
      hookMemSetChar = cast<Function>(hookMemSetCharFunc);
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
        } else if (isa<MemCpyInst>(&*BI)) {
          changeMemCpy(&*BI);
        } else if (isa<MemSetInst>(&*BI)) {
          changeMemSet(&*BI);
        } else if (CallInst *CI = dyn_cast<CallInst>(&*BI)) {
          Function* callee = CI->getCalledFunction();
          if (callee->getName() == "malloc") {
            errs() << "malloc called!" << "\n";
          } else if (callee->getName() == "free") {
            errs() << "free called!" << "\n";
          } else if (callee->getName() == "realloc") {
            errs() << "realloc called!" << "\n";
          } else if (callee->getName() == "calloc") {
            errs() << "calloc called!" << "\n";
          }
        }
      }
      return true;
    }
  };
}
char SimpleDCE::ID = 0;
static RegisterPass<SimpleDCE>
    X("skeletonpass", "Load and store translation"); // NOLINT

