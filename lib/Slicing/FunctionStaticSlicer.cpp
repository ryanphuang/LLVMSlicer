//===- Hello.cpp - Example code from "Writing an LLVM Pass" ---------------===//
//
//                     The LLVM Compiler Infrastructure
//
// This file is distributed under the University of Illinois Open Source
// License. See LICENSE.TXT for details.
//
//===----------------------------------------------------------------------===//
// Details are in a white paper by F. Tip called:
// A survey of program slicing techniques
//===----------------------------------------------------------------------===//

#include <ctype.h>
#include <map>
#include <set>
#include <list>

#include "llvm/IntrinsicInst.h"
#include "llvm/Constants.h"
#include "llvm/Function.h"
#include "llvm/GlobalVariable.h"
#include "llvm/Instructions.h"
#include "llvm/Module.h"
#include "llvm/Pass.h"
#include "llvm/Support/TypeBuilder.h"
#include "llvm/ADT/SmallVector.h"
#include "llvm/Analysis/PostDominators.h"
#include "llvm/Support/CFG.h"
#include "llvm/Support/InstIterator.h"
#include "llvm/Support/raw_ostream.h"
#include "llvm/Transforms/Utils/BasicBlockUtils.h"

#include "Matcher.h"
#include "PostDominanceFrontier.h"
#include "Callgraph.h"
#include "Modifies.h"
#include "PointsTo.h"
#include "LLVMSupport.h"

#include "FunctionStaticSlicer.h"

using namespace llvm;
using namespace llvm::slicing;

// #define DEBUG_SLICE

InsInfo::InsInfo(const Instruction *i, const ptr::PointsToSets &PS,
    const mods::Modifies &MOD) : ins(i), sliced(true) {
  typedef ptr::PointsToSets::PointsToSet PTSet;

  if (const LoadInst *LI = dyn_cast<const LoadInst>(i)) {
    addDEF(i);

    const Value *op = elimConstExpr(LI->getPointerOperand());
    if (isa<ConstantPointerNull>(op)) {
      errs() << "ERROR in analysed code -- reading from address 0 at " <<
        i->getParent()->getParent()->getName() << ":\n";
      i->print(errs());
    } else if (isa<ConstantInt>(op)) {
    } else {
      addREF(op);
      if (!hasExtraReference(op)) {
        const PTSet &S = getPointsToSet(op,PS);
        for (PTSet::const_iterator I = S.begin(), E = S.end(); I != E; ++I)
          addREF(*I);
      }
    }
  } else if (const StoreInst *SI = dyn_cast<const StoreInst>(i)) {
    const Value *l = elimConstExpr(SI->getPointerOperand());
    if (isa<ConstantPointerNull>(l)) {
      errs() << "ERROR in analysed code -- writing to address 0 at " <<
        i->getParent()->getParent()->getName() << ":\n";
      i->print(errs());
    } else if (isa<ConstantInt>(l)) {
    } else {
      if (hasExtraReference(l)) {
        addDEF(l);
      } else {
        const PTSet &S = getPointsToSet(l, PS);

        for (PTSet::const_iterator I = S.begin(), E = S.end(); I != E; ++I)
          addDEF(*I);
      }

      if (!l->getType()->isIntegerTy())
        addREF(l);
      const Value *r = elimConstExpr(SI->getValueOperand());
      if (!hasExtraReference(r) && !isConstantValue(r))
        addREF(r);
    }
  } else if (const GetElementPtrInst *gep =
      dyn_cast<const GetElementPtrInst>(i)) {
    addDEF(i);

    addREF(gep->getPointerOperand());

    for (unsigned i = 1, e = gep->getNumOperands(); i != e; ++i) {
      Value *op = gep->getOperand(i);
      if (!isa<ConstantInt>(op))
        addREF(op);
    }
  } else if (CallInst const* const C = dyn_cast<const CallInst>(i)) {
    const Value *cv = C->getCalledValue();

    if (isInlineAssembly(C)) {
      errs() << "ERROR: Inline assembler detected in " <<
        i->getParent()->getParent()->getName() << ", ignoring\n";
    } else if (isMemoryAllocation(cv)) {
      addDEF(i);
    } else if (isMemoryDeallocation(cv)) {
    } else if (isMemoryCopy(cv) || isMemoryMove(cv)) {
      const Value *l = elimConstExpr(C->getOperand(0));
      if (isPointerValue(l)) {
        const PTSet &L = getPointsToSet(l, PS);
        for (PTSet::const_iterator p = L.begin(); p != L.end(); ++p)
          addDEF(*p);
      }
      const Value *r = elimConstExpr(C->getOperand(1));
      const Value *len = elimConstExpr(C->getOperand(2));
      addREF(l);
      addREF(r);
      /* memcpy/memset wouldn't work with len being 'undef' */
      addREF(len);
      if (isPointerValue(r)) {
        const PTSet &R = getPointsToSet(r, PS);
        for (PTSet::const_iterator p = R.begin(); p != R.end(); ++p)
          addREF(*p);
      }
    } else if (!memoryManStuff(C)) {
      typedef std::vector<const llvm::Function *> CalledVec;
      CalledVec CV;
      getCalledFunctions(C, PS, std::back_inserter(CV));
      const Value *callie = C->getCalledValue();

      if (!isa<Function>(callie))
        addREF(callie);

      for (CalledVec::const_iterator f = CV.begin(); f != CV.end(); ++f) {
        mods::Modifies::mapped_type const& M = getModSet(*f, MOD);
        for (mods::Modifies::mapped_type::const_iterator v = M.begin();
            v != M.end(); ++v)
          addDEF(*v);
      }

      if (!callToVoidFunction(C))
        addDEF(C);
    }
  } else if (isa<const ReturnInst>(i)) {
  } else if (const BinaryOperator *BO = dyn_cast<const BinaryOperator>(i)) {
    addDEF(i);

    if (!isConstantValue(BO->getOperand(0)))
      addREF(BO->getOperand(0));
    if (!isConstantValue(BO->getOperand(1)))
      addREF(BO->getOperand(1));
  } else if (const CastInst *CI = dyn_cast<const CastInst>(i)) {
    addDEF(i);

    if (!hasExtraReference(CI->getOperand(0)))
      addREF(CI->getOperand(0));
  } else if (const AllocaInst *AI = dyn_cast<const AllocaInst>(i)) {
    addDEF(AI);
  } else if (const CmpInst *CI = dyn_cast<const CmpInst>(i)) {
    addDEF(i);

    if (!isConstantValue(CI->getOperand(0)))
      addREF(CI->getOperand(0));
    if (!isConstantValue(CI->getOperand(1)))
      addREF(CI->getOperand(1));
  } else if (const BranchInst *BI = dyn_cast<const BranchInst>(i)) {
    if (BI->isConditional() && !isConstantValue(BI->getCondition()))
      addREF(BI->getCondition());
  } else if (const PHINode *phi = dyn_cast<const PHINode>(i)) {
    addDEF(i);

    for (unsigned k = 0; k < phi->getNumIncomingValues(); ++k)
      if (!isConstantValue(phi->getIncomingValue(k)))
        addREF(phi->getIncomingValue(k));
  } else if (const SwitchInst *SI = dyn_cast<SwitchInst>(i)) {
    if (!isConstantValue(SI->getCondition()))
      addREF(SI->getCondition());
  } else if (const SelectInst *SI = dyn_cast<const SelectInst>(i)) {
    // TODO: THE FOLLOWING CODE HAS NOT BEEN TESTED YET

    addDEF(i);

    if (!isConstantValue(SI->getCondition()))
      addREF(SI->getCondition());
    if (!isConstantValue(SI->getTrueValue()))
      addREF(SI->getTrueValue());
    if (!isConstantValue(SI->getFalseValue()))
      addREF(SI->getFalseValue());
  } else if (isa<const UnreachableInst>(i)) {
  } else if (const ExtractValueInst *EV = dyn_cast<const ExtractValueInst>(i)) {
    addDEF(i);
    addREF(EV->getAggregateOperand());
  } else if (const InsertValueInst *IV = dyn_cast<const InsertValueInst>(i)) {
    //      TODO THE FOLLOWING CODE HAS NOT BEEN TESTED YET

    const Value *r = IV->getInsertedValueOperand();
    addDEF(IV->getAggregateOperand());
    if (!isConstantValue(r))
      addREF(r);
  } else {
    errs() << "ERROR: Unsupported instruction reached\n";
    i->print(errs());
  }
}

namespace {
  class FunctionSlicer : public ModulePass {
    public:
      static char ID;

      FunctionSlicer() : ModulePass(ID) {}

      virtual bool runOnModule(Module &M);

      void getAnalysisUsage(AnalysisUsage &AU) const {
        AU.addRequired<PostDominatorTree>();
        AU.addRequired<PostDominanceFrontier>();
      }
    private:
      bool runOnFunction(Function &F, const ptr::PointsToSets &PS,
          const mods::Modifies &MOD);
  };
}

static RegisterPass<FunctionSlicer> X("slice", "Slices the code");
char FunctionSlicer::ID;

FunctionStaticSlicer::~FunctionStaticSlicer() {
  for (InsInfoMap::const_iterator I = insInfoMap.begin(), E = insInfoMap.end();
      I != E; I++)
    delete I->second;
}

typedef llvm::SmallVector<const Instruction *, 10> SuccList;

static SuccList getSuccList(const Instruction *i) {
  SuccList succList;
  const BasicBlock *bb = i->getParent();
  if (i != &bb->back()) {
    BasicBlock::const_iterator I(i);
    I++;
    succList.push_back(&*I);
  } else {
    for (succ_const_iterator I = succ_begin(bb), E = succ_end(bb); I != E; I++)
      succList.push_back(&(*I)->front());
  }
  return succList;
}

bool FunctionStaticSlicer::sameValues(const Value *val1, const Value *val2)
{
  return val1 == val2;
}

/*
 * RC(i)=RC(i) \cup
 *   {v| v \in RC(j), v \notin DEF(i)} \cup
 *   {v| v \in REF(i), DEF(i) \cap RC(j) \neq \emptyset}
 */
bool FunctionStaticSlicer::computeRCi(InsInfo *insInfoi, InsInfo *insInfoj) {
  bool changed = false;

  /* {v| v \in RC(j), v \notin DEF(i)} */
  for (ValSet::const_iterator I = insInfoj->RC_begin(),
      E = insInfoj->RC_end(); I != E; I++) {
    const Value *RCj = *I;
    bool in_DEF = false;
    for (ValSet::const_iterator II = insInfoi->DEF_begin(),
        EE = insInfoi->DEF_end(); II != EE; II++)
      if (sameValues(*II, RCj)) {
        in_DEF = true;
        break;
      }
    if (!in_DEF)
      if (insInfoi->addRC(RCj))
        changed = true;
  }
  /* DEF(i) \cap RC(j) \neq \emptyset */
  bool isect_nonempty = false;
  for (ValSet::const_iterator I = insInfoi->DEF_begin(),
      E = insInfoi->DEF_end(); I != E && !isect_nonempty; I++) {
    const Value *DEFi = *I;
    for (ValSet::const_iterator II = insInfoj->RC_begin(),
        EE = insInfoj->RC_end(); II != EE; II++) {
      if (sameValues(DEFi, *II)) {
        isect_nonempty = true;
        break;
      }
    }
  }

  /* {v| v \in REF(i), ...} */
  if (isect_nonempty)
    for (ValSet::const_iterator I = insInfoi->REF_begin(),
        E = insInfoi->REF_end(); I != E; I++)
      if (insInfoi->addRC(*I))
        changed = true;
#ifdef DEBUG_RC
  errs() << "  " << __func__ << "2 END";
  if (changed)
    errs() << " ----------CHANGED";
  errs() << '\n';
#endif
  return changed;
}

bool FunctionStaticSlicer::computeRCi(InsInfo *insInfoi) {
  const Instruction *i = insInfoi->getIns();
  bool changed = false;
#ifdef DEBUG_RC
  errs() << "  " << __func__ << ": " << i->getOpcodeName();
  if (i->hasName())
    errs() << " (" << i->getName() << ")";
  errs() << '\n';
  errs() << "    DUMP: ";
  i->print(errs());
  errs() << '\n';
#endif
  SuccList succList = getSuccList(i);
  for (SuccList::const_iterator I = succList.begin(), E = succList.end();
      I != E; I++)
    changed |= computeRCi(insInfoi, getInsInfo(*I));

  return changed;
}

void FunctionStaticSlicer::computeRC() {
  bool changed;
#ifdef DEBUG_RC
  int it = 1;
#endif
  do {
    changed = false;
#ifdef DEBUG_RC
    errs() << __func__ << ": ============== Iteration " << it++ << '\n';
#endif
    typedef std::reverse_iterator<Function::iterator> revFun;
    for (revFun I = revFun(fun.end()), E = revFun(fun.begin()); I != E; I++) {
      typedef std::reverse_iterator<BasicBlock::iterator> rev;
      InsInfo *past = NULL;
      for (rev II = rev(I->end()), EE = rev(I->begin()); II != EE; ++II) {
        InsInfo *insInfo = getInsInfo(&*II);
        if (!past)
          changed |= computeRCi(insInfo);
        else
          changed |= computeRCi(insInfo, past);
        past = insInfo;
      }
    }
  } while (changed);
}

/*
 * SC(i)={i| DEF(i) \cap RC(j) \neq \emptyset}
 */
void FunctionStaticSlicer::computeSCi(const Instruction *i, const Instruction *j) {
  InsInfo *insInfoi = getInsInfo(i), *insInfoj = getInsInfo(j);

  bool isect_nonempty = false;
  for (ValSet::const_iterator I = insInfoi->DEF_begin(),
      E = insInfoi->DEF_end(); I != E && !isect_nonempty; I++) {
    const Value *DEFi = *I;
    for (ValSet::const_iterator II = insInfoj->RC_begin(),
        EE = insInfoj->RC_end(); II != EE; II++) {
      if (sameValues(DEFi, *II)) {
        isect_nonempty = true;
        break;
      }
    }
  }

  if (isect_nonempty) {
    insInfoi->deslice();
#ifdef DEBUG_SLICING
    errs() << "XXXXXXXXXXXXXY ";
    i->print(errs());
    errs() << '\n';
#endif
  }
}

void FunctionStaticSlicer::computeSC() {
  for (inst_iterator I = inst_begin(fun), E = inst_end(fun); I != E; I++) {
    const Instruction *i = &*I;
    SuccList succList = getSuccList(i);
    for (SuccList::const_iterator II = succList.begin(), EE = succList.end();
        II != EE; II++)
      computeSCi(i, *II);
  }
}

bool FunctionStaticSlicer::computeBC() {
  bool changed = false;
#ifdef DEBUG_BC
  errs() << __func__ << " ============ BEG\n";
#endif
  PostDominanceFrontier &PDF = MP->getAnalysis<PostDominanceFrontier>(fun);
  for (inst_iterator I = inst_begin(fun), E = inst_end(fun); I != E; I++) {
    Instruction *i = &*I;
    const InsInfo *ii = getInsInfo(i);
    if (ii->isSliced())
      continue;
    BasicBlock *BB = i->getParent();
#ifdef DEBUG_BC
    errs() << "  ";
    i->print(errs());
    errs() << " -> bb=" << BB->getName() << '\n';
#endif
    PostDominanceFrontier::const_iterator frontier = PDF.find(BB);
    if (frontier == PDF.end())
      continue;
    changed |= updateRCSC(frontier->second.begin(), frontier->second.end());
  }
#ifdef DEBUG_BC
  errs() << __func__ << " ============ END\n";
#endif
  return changed;
}

bool FunctionStaticSlicer::updateRCSC(
    PostDominanceFrontier::DomSetType::const_iterator start,
    PostDominanceFrontier::DomSetType::const_iterator end) {
  bool changed = false;
#ifdef DEBUG_RC
  errs() << __func__ << " ============ BEG\n";
#endif
  for (; start != end; start++) {
    const BasicBlock *BB = *start;
    const Instruction &i = BB->back();
    InsInfo *ii = getInsInfo(&i);
    /* SC = BC \cup ... */
#ifdef DEBUG_SLICING
    errs() << "XXXXXXXXXXXXXX " << BB->getName() << " ";
    i.print(errs());
    errs() << '\n';
#endif
    ii->deslice();
    /* RC = ... \cup \cup(b \in BC) RB */
    for (ValSet::const_iterator II = ii->REF_begin(), EE = ii->REF_end();
        II != EE; II++)
      if (ii->addRC(*II)) {
        changed = true;
#ifdef DEBUG_RC
        errs() << "  added " << (*II)->getName() << "\n";
#endif
      }
  }
#ifdef DEBUG_RC
  errs() << __func__ << " ============ END: changed=" << changed << "\n";
#endif
  return changed;
}

static bool canSlice(const Instruction &i) {
  switch (i.getOpcode()) {
    case Instruction::Alloca:
    case Instruction::Ret:
    case Instruction::Unreachable:
    case Instruction::Br:
    case Instruction::Switch:
      return false;
  }
  return true;
}


// a simple function 'prototype' printer
void printFuncProtoType(Function *F)
{
  F->getReturnType()->dump();
  errs() << " " << F->getName() << "(";
  FunctionType * FTY = F->getFunctionType();
  unsigned params = FTY->getNumParams();
  unsigned i;
  for (i = 0; i < params; i++) {
    FTY->getParamType(i)->dump();
    if (i != params - 1)
      errs() << ",";
  }
  errs() << ")";
}

void FunctionStaticSlicer::dump(Matcher &matcher, bool outputline) {
  errs() << "slice in '";
  printFuncProtoType(&fun);
  /*
  MDNode * md = matcher.getFunctionMD(&fun);
  if (md == NULL) {
    printFuncProtoType(&fun);
  }
  else {
    DISubprogram S(md);
    errs() << S.getLinkageName();
  }
  */
  errs() << "': ";
  if (outputline)
    errs() << "[";
  std::set<unsigned> lines;
  for (inst_iterator I = inst_begin(fun), E = inst_end(fun); I != E; I++) {
    const Instruction &i = *I;
    const InsInfo *ii = getInsInfo(&i);
    if (ii->isSliced()) // skip removed inst
      continue;
    if (outputline) {
      unsigned line = ScopeInfoFinder::getInstLine(&i);
      if (line != 0) {
        lines.insert(line);
      }
    }
    else
      i.dump();
#ifdef DEBUG_DUMP
    errs() << "    DEF:\n";
    for (ValSet::const_iterator II = ii->DEF_begin(), EE = ii->DEF_end();
        II != EE; II++) {
      errs() << "      ";
      (*II)->dump();
    }
    errs() << "    REF:\n";
    for (ValSet::const_iterator II = ii->REF_begin(), EE = ii->REF_end();
        II != EE; II++) {
      errs() << "      ";
      (*II)->dump();
    }
    errs() << "    RC:\n";
    for (ValSet::const_iterator II = ii->RC_begin(), EE = ii->RC_end();
        II != EE; II++) {
      errs() << "      ";
      (*II)->dump();
    }
#endif
  }
  if (outputline) {
    std::set<unsigned>::iterator li; 
    for (li = lines.begin(); li != lines.end();) {
      errs() << *li;
      if (++li != lines.end())
        errs() << ",";
    }
    errs() << "]\n";
  }
}

/**
 * this method calculates the static slice for the CFG
 */
void FunctionStaticSlicer::calculateStaticSlice() {
#ifdef DEBUG_SLICE
  errs() << " ============ BEG: slicing '" << fun.getName() << "'\n";
#endif
  do {
#ifdef DEBUG_SLICE
    errs() << " ------- step 1: compute RC...\n";
#endif
    computeRC();
#ifdef DEBUG_SLICE
    errs() << " ------- step 2: compute SC...\n";
#endif
    computeSC();

#ifdef DEBUG_SLICE
    errs() << " ------- step 3: compute BC...\n";
#endif
  } while (computeBC());

#ifdef DEBUG_SLICE
  errs() << " ============ END: slicing '" << fun.getName() << "'\n";
#endif
}

bool FunctionStaticSlicer::slice() {
#ifdef DEBUG_SLICE
  errs() << __func__ << " ============ BEG\n";
#endif
  bool removed = false;
  for (inst_iterator I = inst_begin(fun), E = inst_end(fun); I != E;) {
    Instruction &i = *I;
    InsInfoMap::iterator ii_iter = insInfoMap.find(&i);
    assert(ii_iter != insInfoMap.end());
    const InsInfo *ii = ii_iter->second;
    ++I;
    if (ii->isSliced() && canSlice(i)) {
#ifdef DEBUG_SLICE
      errs() << "  removing:";
      i.print(errs());
      errs() << " from " << i.getParent()->getName() << '\n';
#endif
      i.replaceAllUsesWith(UndefValue::get(i.getType()));
      i.eraseFromParent();
      insInfoMap.erase(ii_iter);
      delete ii;

      removed = true;
    }
  }
  return removed;
}

/**
 * removeUndefBranches -- remove branches with undef condition
 *
 * These are irrelevant to the code, so may be removed completely with their
 * bodies.
 */
void FunctionStaticSlicer::removeUndefBranches(ModulePass *MP, Function &F) {
#ifdef DEBUG_SLICE
  errs() << __func__ << " ============ Removing unused branches\n";
#endif
  PostDominatorTree &PDT = MP->getAnalysis<PostDominatorTree>(F);
  typedef llvm::SmallVector<const BasicBlock *, 10> Unsafe;
  Unsafe unsafe;

  for (Function::iterator I = F.begin(), E = F.end(); I != E; ++I) {
    BasicBlock &bb = *I;
    if (std::distance(succ_begin(&bb), succ_end(&bb)) <= 1)
      continue;
    Instruction &back = bb.back();
    if (back.getOpcode() != Instruction::Br &&
        back.getOpcode() != Instruction::Switch)
      continue;
    const Value *cond = back.getOperand(0);
    if (cond->getValueID() != Value::UndefValueVal)
      continue;
    DomTreeNode *node = PDT.getNode(&bb);
    if (!node) /* this bb is unreachable */
      continue;
    DomTreeNode *idom = node->getIDom();
    assert(idom);
    /*    if (!idom)
          continue;*/
    BasicBlock *dest = idom->getBlock();
    if (!dest) /* TODO when there are nodes with noreturn calls */
      continue;
#ifdef DEBUG_SLICE
    errs() << "  considering branch: " << bb.getName() << '\n';
    errs() << "  dest=" << dest->getName() << "\n";
#endif
    if (PHINode *PHI = dyn_cast<PHINode>(&dest->front()))
      if (PHI->getBasicBlockIndex(&bb) == -1) {
        /* TODO this is unsafe! */
        unsafe.push_back(&bb);
        PHI->addIncoming(Constant::getNullValue(PHI->getType()), &bb);
      }
    BasicBlock::iterator ii(back);
    Instruction *newI = BranchInst::Create(dest);
    ReplaceInstWithInst(bb.getInstList(), ii, newI);
  }
  for (Unsafe::const_iterator I = unsafe.begin(), E = unsafe.end();
      I != E; ++I) {
    const BasicBlock *bb = *I;
    if (std::distance(pred_begin(bb), pred_end(bb)) > 1)
      errs() << "WARNING: PHI node with added value which is zero\n";
  }
#ifdef DEBUG_SLICE
  errs() << __func__ << " ============ END\n";
#endif
}

/**
 * removeUndefCalls -- remove calls with undef function
 *
 * These are irrelevant to the code, so may be removed completely.
 */
void FunctionStaticSlicer::removeUndefCalls(ModulePass *MP, Function &F) {
  for (inst_iterator I = inst_begin(F), E = inst_end(F); I != E;) {
    CallInst *CI = dyn_cast<CallInst>(&*I);
    ++I;
    if (CI && isa<UndefValue>(CI->getCalledValue())) {
      CI->replaceAllUsesWith(UndefValue::get(CI->getType()));
      CI->eraseFromParent();
    }
  }
}

void FunctionStaticSlicer::removeUndefs(ModulePass *MP, Function &F)
{
  removeUndefBranches(MP, F);
  removeUndefCalls(MP, F);
}

static bool handleAssert(Function &F, FunctionStaticSlicer &ss,
    const CallInst *CI) {

  const char *ass_file = getenv("SLICE_ASSERT_FILE");
  const char *ass_line = getenv("SLICE_ASSERT_LINE");
  const ConstantExpr *fileArg = dyn_cast<ConstantExpr>(CI->getArgOperand(1));
  const ConstantInt *lineArg = dyn_cast<ConstantInt>(CI->getArgOperand(2));

  if (ass_file && ass_line) {
    if (fileArg && fileArg->getOpcode() == Instruction::GetElementPtr &&
        lineArg) {
      const GlobalVariable *strVar =
        dyn_cast<GlobalVariable>(fileArg->getOperand(0));
      assert(strVar && strVar->hasInitializer());
      const ConstantArray *str =
        dyn_cast<ConstantArray>(strVar->getInitializer());
      assert(str && str->isCString());
      /* trim the NUL terminator */
      StringRef tmpStr(str->getAsString());
      StringRef fileArgStr = tmpStr.substr(0,tmpStr.size() - 1);
      const int ass_line_int = atoi(ass_line);

      errs() << "ASSERT at " << fileArgStr << ":" << lineArg->getValue() << "\n";

      if (fileArgStr.equals(ass_file) && lineArg->equalsInt(ass_line_int)) {
        errs() << "\tMATCH\n";
        goto count;
      }
    }
    ss.addSkipAssert(CI);
    return false;
  }

count:
#ifdef DEBUG_INITCRIT
  errs() << "    adding\n";
#endif
  ss.addInitialCriterion(CI,
      F.getParent()->getGlobalVariable("__ai_init_functions", true));
  return true;
}

bool llvm::slicing::findInitialCriterion(Function &F,
    FunctionStaticSlicer &ss,
    bool starting) {
  bool added = false;
#ifdef DEBUG_INITCRIT
  errs() << __func__ << " ============ BEGIN\n";
#endif

  for (inst_iterator I = inst_begin(F), E = inst_end(F); I != E; ++I) {
    const Instruction *i = &*I;
    i->dump();
    if (const StoreInst *SI = dyn_cast<StoreInst>(i)) {
      const Value *LHS = SI->getPointerOperand();
      if (LHS->hasName() && LHS->getName().startswith("product")) {
#ifdef DEBUG_INITCRIT
        errs() << "    adding\n";
#endif
        ss.addInitialCriterion(SI, LHS);
      }
    /* } else if (const CallInst *CI = dyn_cast<CallInst>(i)) {
      Function *callie = CI->getCalledFunction();
      if (callie == F__assert_fail) {
        added = handleAssert(F, ss, CI);
      }
      */
    } else if (const ReturnInst *RI = dyn_cast<ReturnInst>(i)) {
      if (starting) {
        const Module *M = F.getParent();
        for (Module::const_global_iterator II = M->global_begin(),
            EE = M->global_end(); II != EE; ++II) {
          const GlobalVariable &GV = *II;
          if (!GV.hasName() || !GV.getName().startswith("__ai_state_"))
            continue;
#ifdef DEBUG_INITCRIT
          errs() << "adding " << GV.getName() << " into " << F.getName() <<
            " to \n";
          RI->dump();
#endif
          ss.addInitialCriterion(RI, &GV, false);
        }
      }
    }
  }
#ifdef DEBUG_INITCRIT
  errs() << __func__ << " ============ END\n";
#endif
  return added;
}

bool FunctionSlicer::runOnFunction(Function &F, const ptr::PointsToSets &PS,
    const mods::Modifies &MOD) {
  FunctionStaticSlicer ss(F, this, PS, MOD);

  findInitialCriterion(F, ss);

  ss.calculateStaticSlice();

  bool sliced = ss.slice();
  if (sliced)
    FunctionStaticSlicer::removeUndefs(this, F);

  return sliced;
}

bool FunctionSlicer::runOnModule(Module &M) {
  ptr::PointsToSets PS;
  {
    ptr::ProgramStructure P(M);
    computePointsToSets(P, PS);
  }

  callgraph::Callgraph CG(M, PS);

  mods::Modifies MOD;
  {
    mods::ProgramStructure P1(M);
    computeModifies(P1, CG, PS, MOD);
  }

  bool modified = false;
  for (Module::iterator I = M.begin(), E = M.end(); I != E; ++I) {
    Function &F = *I;
    if (!F.isDeclaration())
      modified |= runOnFunction(F, PS, MOD);
  }
  return modified;
}
