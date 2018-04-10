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
#include "llvm/IR/CallSite.h"
using namespace llvm;
using namespace std;

namespace {

  struct HeapTranslation : public ModulePass {

    const string GET = "_Z3gety";
    const string PUT = "_Z3putyx";
    const string HEAP_BEGIN = "_Z12getHeapBeginv";
    const string HEAP_END = "_Z10getHeapEndv";
    const string MALLOC = "_Z9my_mallocm";
    const string FREE = "_Z7my_freePv";
    const string REALLOC = "_Z10my_reallocPvm";
    const string CALLOC = "_Z9my_callocmm";

    static char ID;

    Function *hookGet;
    Function *hookPut;
    Function *hookGetHeapBegin;
    Function *hookGetHeapEnd;
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

    vector<TerminatorInst*> thenTermsForLoads;
    vector<TerminatorInst*> elseTermsForLoads;
    vector<TerminatorInst*> thenTermsForStores;
    vector<TerminatorInst*> elseTermsForStores;

    vector<Instruction*> stores;
    vector<Instruction*> loads;

    HeapTranslation() : ModulePass(ID) {}

    bool functionNameContains(Function* func, const char* data) {
      if (!func->getName().empty()) {
        return strstr(func->getName().data(), data);
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

    /*
     * if index == 1, BI points to store instruction; otherwise, to load instruction
     */
    bool includesGlobalVariable(BasicBlock::iterator BI, int index) {
      if (index == 0) {
        if (const GlobalValue *g = dyn_cast<GlobalValue>(&*BI->getOperand(index))) return true;
      }
      if (const GlobalValue *g = dyn_cast<GlobalValue>(&*BI->getOperand(index))) return true;
      return false;
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

    void changeMalloc(Instruction* instruction) {
      CallInst* ci = cast<CallInst>(instruction);
      Value* size = ci->getArgOperand(0);
      Value* malloc_arguments[] = { size };
      Instruction* mallocCallInst = CallInst::Create(hookMalloc, malloc_arguments, "");
      prepareForTranslation(mallocCallInst, instruction);
    }

    void changeFree(Instruction* instruction) {
      CallInst* ci = cast<CallInst>(instruction);
      Value* ptr = ci->getArgOperand(0);
      Value* free_arguments[] = { ptr };
      Instruction* freeCallInst = CallInst::Create(hookFree, free_arguments, "");
      prepareForTranslation(freeCallInst, instruction);
    }

    void changeRealloc(Instruction* instruction) {
      CallInst* ci = cast<CallInst>(instruction);
      Value* ptr = ci->getArgOperand(0);
      Value* size = ci->getArgOperand(1);
      Value* realloc_arguments[] = { ptr, size };
      Instruction* reallocCallInst = CallInst::Create(hookRealloc, realloc_arguments, "");
      prepareForTranslation(reallocCallInst, instruction);
    }

    void changeCalloc(Instruction* instruction) {
      CallInst* ci = cast<CallInst>(instruction);
      Value* num = ci->getArgOperand(0);
      Value* size = ci->getArgOperand(1);
      Value* calloc_arguments[] = { num, size };
      Instruction* callocCallInst = CallInst::Create(hookCalloc, calloc_arguments, "");
      prepareForTranslation(callocCallInst, instruction);
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

      hookMalloc = cast<Function>(M.getOrInsertFunction(MALLOC, INT8PTRTY, INT64TY));
      hookFree = cast<Function>(M.getOrInsertFunction(FREE, VOIDTY, INT8PTRTY));
      hookRealloc = cast<Function>(M.getOrInsertFunction(REALLOC, INT8PTRTY, INT8PTRTY, INT64TY));
      hookCalloc = cast<Function>(M.getOrInsertFunction(CALLOC, INT8PTRTY, INT64TY, INT64TY));
    }

    void createIfThenPatternForStores() {
      if (storeConds.size() != oldStores.size()) {
        errs() << "Error!\n";
        return;
      }
      for (int i = 0; i < storeConds.size(); i++) {
        TerminatorInst *thenTerm = nullptr;
        TerminatorInst *elseTerm = nullptr;
        Value* cond = storeConds[i];
        Instruction* splitBefore = oldStores[i];
        SplitBlockAndInsertIfThenElse(cond, splitBefore, &thenTerm, &elseTerm);
        thenTermsForStores.push_back(thenTerm);
        elseTermsForStores.push_back(elseTerm);
      }
    }

    void createIfThenPatternForLoads() {
      if (loadConds.size() != oldLoads.size()) {
        errs() << "Error!\n";
        return;
      }
      for (int i = 0; i < loadConds.size(); i++) {
        TerminatorInst *thenTerm = nullptr;
        TerminatorInst *elseTerm = nullptr;
        Value* cond = loadConds[i];
        Instruction* splitBefore = oldLoads[i];
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

    void addGetCall(Value* address_of_load, Module &M, Instruction** ptrToIntInst,
                       Instruction** callGet, Instruction** truncInst, Instruction** intToPtrInst) {
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

      *ptrToIntInst = cast<Instruction>(new PtrToIntInst(address_of_load, INT64TY, ""));
      Value* argsFixed[] = { *ptrToIntInst };

      *callGet = CallInst::Create(hookGet, argsFixed, "");

      // truncate if integer and bit width is smaller
      *truncInst = nullptr;
      IntegerType *it = dyn_cast<IntegerType>((*callGet)->getType());
      if (bw != -1 && ind_count == 0) {   // expected return value type is not integer
        if (it->getBitWidth() > bw) {
          *truncInst = cast<Instruction>(new TruncInst(*callGet, IntegerType::get(M.getContext(), bw), ""));
        }
      }

      // cast to pointer if pointer type is expected
      *intToPtrInst = nullptr;
      Instruction* instBefore = *truncInst ? *truncInst : *callGet;
      if (ind_count) {
        Type* it = value_type;
        Type* it_p = PointerType::getUnqual(it);
        ind_count--;
        while (ind_count) {
          it_p = PointerType::getUnqual(it_p);
          ind_count--;
        }
        *intToPtrInst = cast<Instruction>(new IntToPtrInst(instBefore, it_p, ""));
      }
    }

    void prepareGetBranch(int i, Module &M) {
      TerminatorInst *thenTerm = thenTermsForLoads[i];
      BasicBlock *parentThen = thenTerm->getParent();
      BasicBlock::iterator BI = parentThen->begin();
      Value* address_of_load = loads[i]->getOperand(0);

      Instruction *instructions[4];
      addGetCall(address_of_load, M, &instructions[0], &instructions[1],  &instructions[2],  &instructions[3]);

      parentThen->getInstList().insert(BI, instructions[0]);
      parentThen->getInstList().insert(BI, instructions[1]);
      if (instructions[2])
        parentThen->getInstList().insert(BI, instructions[2]);
      if (instructions[3])
        parentThen->getInstList().insert(BI, instructions[3]);
    }

    void prepareLoadBranch(int i) {
      TerminatorInst *elseTerm = elseTermsForLoads[i];
      BasicBlock *parent = elseTerm->getParent();
      BasicBlock::iterator BI = parent->begin();
      parent->getInstList().insert(BI, loads[i]);
    }

    void preparePHINode(int i) {
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

    void printFunctions(Module &M) {
      for(Module::iterator F = M.begin(), E = M.end(); F!= E; ++F) {
        if (functionNameContains(&*F, "functions.cpp")) {
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
        if (functionNameContains(&*F, "functions.cpp")) {
          break;
        }
        else if (hasLinkage(&*F) || functionNameContains(&*F, "main")) {
          continue;
        }
        for(Function::iterator BB = F->begin(), E = F->end(); BB != E; ++BB) {
          HeapTranslation::runOnBasicBlock(BB, M);
        }
      }

      for(Module::iterator F = M.begin(), E = M.end(); F!= E; ++F) {
        if (functionNameContains(&*F, "functions.cpp")) {
          break;
        }
        else if (hasLinkage(&*F) || functionNameContains(&*F, "main")) {
          continue;
        }
        for(Function::iterator BB = F->begin(), E = F->end(); BB != E; ++BB) {
          for (BasicBlock::iterator BI = BB->begin(), BE = BB->end(); BI != BE; ++BI) {
            if (isa<StoreInst>(&*BI) && !includesGlobalVariable(BI, 1)) {
              storeConds.push_back(&*BI->getPrevNode());
              oldStores.push_back(&*BI);
              stores.push_back(&*BI->clone());
            } else if (isa<LoadInst>(&*BI) && !includesGlobalVariable(BI, 0)) {
              loadConds.push_back(&*BI->getPrevNode());
              oldLoads.push_back(&*BI);
              loads.push_back(&*BI->clone());
            }
          }
        }
      }

      createIfThenPatternForStores();
      createIfThenPatternForLoads();

      for (int i = 0; i < storeConds.size(); i++) {
        preparePutBranch(i);
        prepareStoreBranch(i);
      }
      for (int i = 0; i < loadConds.size(); i++) {
        prepareGetBranch(i, M);
        prepareLoadBranch(i);
        preparePHINode(i);
      }
      for (int i = 0; i < oldStores.size(); i++) {
        oldStores[i]->eraseFromParent();
      }

      replaceInstructions(deleteList, addList);
//      printFunctions(M);
      return true;
    }
    virtual bool runOnBasicBlock(Function::iterator &FI, Module &M) {
      BasicBlock* BB = &*FI;
      for (BasicBlock::iterator BI = FI->begin(), BE = FI->end(); BI != BE; ++BI) {
        if (isa<LoadInst>(&*BI) && !includesGlobalVariable(BI, 0)) {
          prepareForBranching(BB, BI, 0);
        } else if (isa<StoreInst>(&*BI) && !includesGlobalVariable(BI, 1)) {
          prepareForBranching(BB, BI, 1);
        }
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





