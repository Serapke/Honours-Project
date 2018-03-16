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
#include "llvm/Support/raw_ostream.h"
#include "llvm/IR/CallSite.h"
#include <set>
#include <iostream>
using namespace llvm;
using namespace std;

namespace {

  struct HeapTranslation : public ModulePass {

    const int POINTER_TYPE_ID = 15;

    const string GET = "_Z3gety";
    const string PUT = "_Z3putyx";
    const string HEAP_BEGIN = "_Z12getHeapBeginv";
    const string HEAP_END = "_Z10getHeapEndv";
    const string MEMCPY = "_Z6memcpyPvPKvm";
    const string MEMSET = "_Z6memsetPvim";
    const string MEMSET_CHAR = "_Z6memsetPvcm";
    const string MALLOC = "_Z9my_mallocm";
    const string FREE = "_Z7my_freePv";
    const string REALLOC = "_Z10my_reallocPvm";
    const string CALLOC = "_Z9my_callocmm";

    static char ID;

    Function *hookGet;
    Function *hookPut;
    Function *hookGetHeapBegin;
    Function *hookGetHeapEnd;

    Function *hookMemCpy;
    Function *hookMemSet;
    Function *hookMemSetChar;
    Function *hookMalloc;
    Function *hookFree;
    Function *hookRealloc;
    Function *hookCalloc;

    Type* INT64TY;
    Type* VOIDTY;
    Type* INT8PTRTY;

    vector<Instruction*> deleteList;
    vector<Instruction*> addList;
    vector<Instruction*> oldStores;
    vector<Instruction*> oldLoads;

    vector<Value*> loadConds;
    vector<Value*> storeConds;
    vector<Instruction*> splitsBeforeLoad;
    vector<Instruction*> splitsBeforeStore;

    vector<TerminatorInst*> thenTermsForLoads;
    vector<TerminatorInst*> elseTermsForLoads;
    vector<TerminatorInst*> thenTermsForStores;
    vector<TerminatorInst*> elseTermsForStores;

    vector<Instruction*> stores;
    vector<Instruction*> loads;

    HeapTranslation() : ModulePass(ID) {}

    // TODO: improve the detection of gRPC code
    // gRPC code starts with a function, which name includes 'functions.cpp' substring
    bool isgRPCCode(Function* func) {
      if (!func->getName().empty()) {
        return strstr(func->getName().data(), "functions.cpp");
      }
      return true;
    }

    bool hasLinkage(Function* func) {
      if (func->hasLinkOnceODRLinkage()) {
        return true;
      }
      return false;
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

    void prepareForTranslation(Instruction* newInst, Instruction* oldInst) {
      addList.push_back(newInst);
      deleteList.push_back(oldInst);
    }

    void prepareForTranslation(Function* hook, ArrayRef<Value*> args, Instruction* instructionToChange) {
      Instruction* functionCallInst = CallInst::Create(hook, args, "");
      addList.push_back(functionCallInst);
      deleteList.push_back(instructionToChange);
    }

    void prepareForBranching(BasicBlock* BB, BasicBlock::iterator BI, int index) {
      Value* address = &*BI->getOperand(index);
      ArrayRef<Value*> argsFixed;
      Instruction* callGetHeapBegin = CallInst::Create(hookGetHeapBegin, argsFixed, "");
      Instruction* callGetHeapEnd = CallInst::Create(hookGetHeapEnd, argsFixed, "");
      Instruction* ptrToIntInstAddr = cast<Instruction>(new PtrToIntInst(address, INT64TY, ""));

      Instruction* icmp1 = cast<Instruction>(new ICmpInst(CmpInst::ICMP_SLE, callGetHeapBegin, ptrToIntInstAddr));
      Instruction* icmp2 = cast<Instruction>(new ICmpInst(CmpInst::ICMP_SLT, ptrToIntInstAddr, callGetHeapEnd));
      Instruction* add = cast<Instruction>(BinaryOperator::Create(Instruction::And, icmp1, icmp2));

      BB->getInstList().insert(BI, callGetHeapBegin);
      BB->getInstList().insert(BI, callGetHeapEnd);
      BB->getInstList().insert(BI, ptrToIntInstAddr);
      BB->getInstList().insert(BI, icmp1);
      BB->getInstList().insert(BI, icmp2);
      BB->getInstList().insert(BI, add);
    }

    void getTypeInfo(Type* type, int &ind_count, Type** elementType) {
      *elementType = type;
      if (PointerType* pt = dyn_cast<PointerType>(type)) {
        ind_count++;
        while (PointerType* ptt = dyn_cast<PointerType>(pt->getElementType())) {
          ind_count++;
          pt = ptt;
        }
        *elementType = pt->getElementType();
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

    void changeMalloc(Instruction* instruction) {
      CallInst* ci = cast<CallInst>(instruction);
      Value* size = ci->getArgOperand(0);
      Value* malloc_arguments[] = { size };
      prepareForTranslation(hookMalloc, malloc_arguments, instruction);
    }

    void changeFree(Instruction* instruction) {
      CallInst* ci = cast<CallInst>(instruction);
      Value* ptr = ci->getArgOperand(0);
      Value* free_arguments[] = { ptr };
      prepareForTranslation(hookFree, free_arguments, instruction);
    }

    void changeRealloc(Instruction* instruction) {
      CallInst* ci = cast<CallInst>(instruction);
      Value* ptr = ci->getArgOperand(0);
      Value* size = ci->getArgOperand(1);
      Value* realloc_arguments[] = { ptr, size };
      prepareForTranslation(hookRealloc, realloc_arguments, instruction);
    }

    void changeCalloc(Instruction* instruction) {
      CallInst* ci = cast<CallInst>(instruction);
      Value* num = ci->getArgOperand(0);
      Value* size = ci->getArgOperand(1);
      Value* calloc_arguments[] = { num, size };
      prepareForTranslation(hookCalloc, calloc_arguments, instruction);
    }

    void prepareTypes(Module &M) {
      INT64TY = Type::getInt64Ty(M.getContext());
      VOIDTY = Type::getVoidTy(M.getContext());
      INT8PTRTY = Type::getInt8PtrTy(M.getContext());
    }

    void prepareFunctionHooks(Module &M) {
      hookGet = cast<Function>(M.getOrInsertFunction(GET, INT64TY, INT64TY));
      hookPut = cast<Function>(M.getOrInsertFunction(PUT, VOIDTY, INT64TY, INT64TY));
      hookGetHeapBegin = cast<Function>(M.getOrInsertFunction(HEAP_BEGIN, INT64TY));
      hookGetHeapEnd = cast<Function>(M.getOrInsertFunction(HEAP_END, INT64TY));

      hookMemCpy = cast<Function>(M.getOrInsertFunction(MEMCPY, INT8PTRTY, INT8PTRTY, INT8PTRTY, INT64TY));
      hookMemSet = cast<Function>(M.getOrInsertFunction(MEMSET, INT8PTRTY, INT8PTRTY, Type::getInt32Ty(M.getContext()), INT64TY));
      hookMemSetChar = cast<Function>(M.getOrInsertFunction(MEMSET_CHAR, INT8PTRTY, INT8PTRTY, Type::getInt8Ty(M.getContext()), INT64TY));

      hookMalloc = cast<Function>(M.getOrInsertFunction(MALLOC, INT8PTRTY, INT64TY));
      hookFree = cast<Function>(M.getOrInsertFunction(FREE, VOIDTY, INT8PTRTY));
      hookRealloc = cast<Function>(M.getOrInsertFunction(REALLOC, INT8PTRTY, INT8PTRTY, INT64TY));
      hookCalloc = cast<Function>(M.getOrInsertFunction(CALLOC, INT8PTRTY, INT64TY, INT64TY));
    }

    void createIfThenPatternForStores() {
      if (storeConds.size() != splitsBeforeStore.size()) {
        errs() << "Error!\n";
        return;
      }
      for (int i = 0; i < storeConds.size(); i++) {
        TerminatorInst *thenTerm = nullptr;
        TerminatorInst *elseTerm = nullptr;
        Value* cond = storeConds[i];
        Instruction* splitBefore = splitsBeforeStore[i];
        SplitBlockAndInsertIfThenElse(cond, splitBefore, &thenTerm, &elseTerm);
        thenTermsForStores.push_back(thenTerm);
        elseTermsForStores.push_back(elseTerm);
      }
    }

    void createIfThenPatternForLoads() {
      if (loadConds.size() != splitsBeforeLoad.size()) {
        errs() << "Error!\n";
        return;
      }
      for (int i = 0; i < loadConds.size(); i++) {
        TerminatorInst *thenTerm = nullptr;
        TerminatorInst *elseTerm = nullptr;
        Value* cond = loadConds[i];
        Instruction* splitBefore = splitsBeforeLoad[i];
        SplitBlockAndInsertIfThenElse(cond, splitBefore, &thenTerm, &elseTerm);
        thenTermsForLoads.push_back(thenTerm);
        elseTermsForLoads.push_back(elseTerm);
      }
    }

    void preparePutBranch(int i) {
      TerminatorInst *thenTerm = thenTermsForStores[i];
      BasicBlock *parentThen = thenTerm->getParent();
      BasicBlock::iterator BI = parentThen->begin();
      Value *value_to_store = stores[i]->getOperand(0);
      Value *address_of_store = stores[i]->getOperand(1);

      Instruction *ptrToIntInstAddr = cast<Instruction>(new PtrToIntInst(address_of_store, INT64TY, ""));

      Value *argsFixed[2];
      argsFixed[0] = ptrToIntInstAddr;
      argsFixed[1] = value_to_store;

      bool needs_casting = false;

      // cast value to 64 bit integer, if required
      Instruction *sextInst = nullptr;
      Instruction *ptrToIntInst = nullptr;
      if (IntegerType * it = dyn_cast<IntegerType>(value_to_store->getType())) {
        if (it->getBitWidth() != 64) {
          needs_casting = true;
          argsFixed[1] = cast<Instruction>(new SExtInst(value_to_store, INT64TY, ""));
        }
      } else if (PointerType * pt = dyn_cast<PointerType>(value_to_store->getType())) {
        needs_casting = true;
        argsFixed[1] = cast<Instruction>(new PtrToIntInst(value_to_store, INT64TY, ""));
      }

      Instruction *callPut = CallInst::Create(hookPut, argsFixed, "");

      parentThen->getInstList().insert(BI, ptrToIntInstAddr);
      if (needs_casting)
        parentThen->getInstList().insert(BI, cast<Instruction>(argsFixed[1]));
      parentThen->getInstList().insert(BI, callPut);
    }

    void prepareStoreBranch(int i) {
      TerminatorInst *elseTerm = elseTermsForStores[i];
      BasicBlock *parent = elseTerm->getParent();
      BasicBlock::iterator BI = parent->begin();
      parent->getInstList().insert(BI, stores[i]);
    }

    Instruction* prepareGetBranch(int i, Module &M) {
      TerminatorInst *thenTerm = thenTermsForLoads[i];
      BasicBlock *parentThen = thenTerm->getParent();
      BasicBlock::iterator BI = parentThen->begin();
      Value* address_of_load = loads[i]->getOperand(0);

      // get (underlying element, if pointer) type and keep count of number of levels of indirection
      Type* value_type;
      int ind_count = 0;
      getTypeInfo(address_of_load->getType(), ind_count, &value_type);
      ind_count--;

      // if integer, get bit width
      int bw = -1;
      if (IntegerType *it = dyn_cast<IntegerType>(value_type)) {
        bw = it->getBitWidth();
      }

      // new way
      Instruction* ptrToIntInst = cast<Instruction>(new PtrToIntInst(address_of_load, INT64TY, ""));
      Value* argsFixed[] = { ptrToIntInst };


      Instruction* callGet = CallInst::Create(hookGet, argsFixed, "");

      // truncate if integer and bit width is smaller
      Instruction* truncInst = nullptr;
      IntegerType *it = dyn_cast<IntegerType>(callGet->getType());
      if (bw != -1) {   // expected return value type is not integer
        if (it->getBitWidth() > bw) {
          truncInst = cast<Instruction>(new TruncInst(callGet, IntegerType::get(M.getContext(), bw), ""));
        }
      }

      // cast to pointer if pointer type is expected
      Instruction* intToPtrInst = nullptr;
      Instruction* instBefore = truncInst ? truncInst : callGet;
      if (ind_count) {
        Type* it = value_type;
        Type* it_p = PointerType::getUnqual(it);
        ind_count--;
        while (ind_count) {
          it_p = PointerType::getUnqual(it_p);
          ind_count--;
        }
        intToPtrInst = cast<Instruction>(new IntToPtrInst(instBefore, it_p, ""));
      }

      parentThen->getInstList().insert(BI, ptrToIntInst);
      parentThen->getInstList().insert(BI, callGet);
      if (truncInst)
        parentThen->getInstList().insert(BI, truncInst);
      if (intToPtrInst)
        parentThen->getInstList().insert(BI, intToPtrInst);
      return callGet;
    }

    Instruction* prepareLoadBranch(int i) {
      TerminatorInst *elseTerm = elseTermsForLoads[i];
      BasicBlock *parent = elseTerm->getParent();
      BasicBlock::iterator BI = parent->begin();
      parent->getInstList().insert(BI, loads[i]);
      return loads[i];
    }

    void printFunctions(Module &M) {
      for(Module::iterator F = M.begin(), E = M.end(); F!= E; ++F) {
        if (isgRPCCode(&*F)) {
          break;
        }
        else if (hasLinkage(&*F)) {
          continue;
        }
        errs() << *F << "\n";
      }
    }

    virtual bool runOnModule(Module &M) {
      prepareTypes(M);
      prepareFunctionHooks(M);

      for(Module::iterator F = M.begin(), E = M.end(); F!= E; ++F) {
        // if reached gRPC code, translation is ended
        if (isgRPCCode(&*F)) {
          break;
        }
        else if (hasLinkage(&*F)) {
          continue;
        }
        for(Function::iterator BB = F->begin(), E = F->end(); BB != E; ++BB) {
          HeapTranslation::runOnBasicBlock(BB, M);
        }
      }

      for(Module::iterator F = M.begin(), E = M.end(); F!= E; ++F) {
        if (isgRPCCode(&*F)) {
          break;
        }
        else if (hasLinkage(&*F)) {
          continue;
        }
        for(Function::iterator BB = F->begin(), E = F->end(); BB != E; ++BB) {
          for (BasicBlock::iterator BI = BB->begin(), BE = BB->end(); BI != BE; ++BI) {
            if (isa<StoreInst>(&*BI)) {
              storeConds.push_back(&*BI->getPrevNode());
              splitsBeforeStore.push_back(&*BI);
              stores.push_back(&*BI->clone());
            } else if (isa<LoadInst>(&*BI)) {
              loadConds.push_back(&*BI->getPrevNode());
              splitsBeforeLoad.push_back(&*BI);
              loads.push_back(&*BI->clone());
            }
          }
        }
      }

      createIfThenPatternForStores();
      createIfThenPatternForLoads();

      for(Module::iterator F = M.begin(), E = M.end(); F!= E; ++F) {
        if (isgRPCCode(&*F)) {
          break;
        }
        else if (hasLinkage(&*F)) {
          continue;
        }
        for(Function::iterator BB = F->begin(), E = F->end(); BB != E; ++BB) {
          for (BasicBlock::iterator BI = BB->begin(), BE = BB->end(); BI != BE; ++BI) {
            if (isa<StoreInst>(&*BI)) {
              oldStores.push_back(&*BI);
            } else if (isa<LoadInst>(&*BI)) {
              oldLoads.push_back(&*BI);
            }
          }
        }
      }
      for (int i = 0; i < deleteList.size(); i++) {
          oldStores[i]->eraseFromParent();
      }
      for (int i = 0; i < storeConds.size(); i++) {
        preparePutBranch(i);
        prepareStoreBranch(i);
      }
      for (int i = 0; i < loadConds.size(); i++) {
        prepareGetBranch(i, M);
        prepareLoadBranch(i);
        TerminatorInst *thenTerm = thenTermsForLoads[i];
        Instruction* get = thenTerm->getPrevNode();
        BasicBlock *thenBB = thenTerm->getParent();
        TerminatorInst *elseTerm = elseTermsForLoads[i];
        Instruction* load = elseTerm->getPrevNode();
        BasicBlock *elseBB = elseTerm->getParent();
        Instruction* inst = oldLoads[i];

        PHINode* phi = PHINode::Create(inst->getType(), 2);
        phi->addIncoming(get, thenBB);
        phi->addIncoming(load, elseBB);
        ReplaceInstWithInst(inst, phi);
      }

      replaceInstructions(deleteList, addList);

      printFunctions(M);

      return true;
    }
    virtual bool runOnBasicBlock(Function::iterator &FI, Module &M) {
      BasicBlock* BB = &*FI;
      for (BasicBlock::iterator BI = FI->begin(), BE = FI->end(); BI != BE; ++BI) {
        if (isa<LoadInst>(&*BI)) {
          prepareForBranching(BB, BI, 0);
        } else if (isa<StoreInst>(&*BI)) {
          prepareForBranching(BB, BI, 1);
        }
//        else if (isa<MemCpyInst>(&*BI)) {
//          changeMemCpy(&*BI);
//        } else if (isa<MemSetInst>(&*BI)) {
//          changeMemSet(&*BI);
//        }
        else if (CallInst *CI = dyn_cast<CallInst>(&*BI)) {
          Function* callee = CI->getCalledFunction();
          if (callee == NULL) continue;
          if (callee->getName() == "malloc") {
            changeMalloc(&*BI);
          } else if (callee->getName() == "free") {
            changeFree(&*BI);
          } else if (callee->getName() == "realloc") {
            changeRealloc(&*BI);
          } else if (callee->getName() == "calloc") {
            changeCalloc(&*BI);
          }
        }
      }
      return true;
    }
  };
}
char HeapTranslation::ID = 0;
static RegisterPass<HeapTranslation>
    X("heap-translation", "Load and store on heap translation"); // NOLINT


