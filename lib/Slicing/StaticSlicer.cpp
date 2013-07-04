// This file is distributed under the University of Illinois Open Source
// License. See LICENSE.TXT for details.

#include "llvm/Instructions.h"
#include "llvm/Function.h"
#include "llvm/Pass.h"
#include "llvm/Value.h"
#include "llvm/Support/CommandLine.h"

#include "Callgraph.h"
#include "Modifies.h"
#include "PointsTo.h"
#include "StaticSlicer.h"
#include "Matcher.h"

using namespace llvm;

static cl::opt<std::string>
FileName("criterion-file", cl::desc("File name of slicing criterion"));

static cl::opt<int>
LineNumber("criterion-line", cl::desc("Line number of slicing criterion"));

static cl::opt<std::string>
SlicingVariable("criterion-variable", cl::desc("Variable name of slicing criterion"));

namespace llvm { namespace slicing { namespace detail {

  void fillParamsToArgs(CallInst const* const C,
      Function const* const F,
      ParamsToArgs& toArgs)
  {
    Function::const_arg_iterator p = F->arg_begin();
    std::size_t a = 0;
    for ( ; a < C->getNumArgOperands(); ++a, ++p)
    {
      Value const* const P = &*p;
      Value const* const A = C->getArgOperand(a);
      if (!isConstantValue(A))
        toArgs[P] = A;
    }
  }

}}}

namespace llvm { namespace slicing {
  
  void StaticSlicer::parseInitialCriterion()
  {

  }

  void StaticSlicer::buildDicts(const ptr::PointsToSets &PS)
  {
    typedef Module::iterator FunctionsIter;
    for (FunctionsIter f = module.begin(); f != module.end(); ++f)
      if (!f->isDeclaration() && !memoryManStuff(&*f))
        for (inst_iterator i = inst_begin(*f);
            i != inst_end(*f); i++)
          if (CallInst const* c =
              dyn_cast<CallInst const>(&*i)) {
            if (isInlineAssembly(c)) {
              errs() << "ERROR: Inline assembler detected in " <<
                f->getName() << ", skipping\n";
              continue;
            }
            typedef std::vector<const Function *> FunCon;
            FunCon G;
            getCalledFunctions(c, PS, std::back_inserter(G));

            for (FunCon::const_iterator g = G.begin();
                g != G.end(); ++g) {
              Function const* const h = *g;
              if (!memoryManStuff(h) && !h->isDeclaration()) {
                funcsToCalls.insert(std::make_pair(h, c));
                callsToFuncs.insert(std::make_pair(c, h));
              }
            }
          }
  }

  StaticSlicer::StaticSlicer(ModulePass *MP, Module &M,
      const ptr::PointsToSets &PS,
      const callgraph::Callgraph &CG,
      const mods::Modifies &MOD) : mp(MP), module(M),
slicers(), initFuns(), funcsToCalls(), callsToFuncs(), ps(PS), cg(CG), mod(MOD) {
    Matcher matcher(M);
    Matcher::sp_iterator si = matcher.setSourceFile(FileName);
    if (si == matcher.sp_end()) {
      errs() << "No matching subprogram at " << FileName << ":" << LineNumber << " found\n";
      return;
    }
    Scope scope(LineNumber, LineNumber);
    bool multiple;
    Function * F = matcher.matchFunction(si, scope, multiple); 
    if (F == NULL) {
      errs() << "No matching function at " << FileName << ":" << LineNumber << " found\n";
      return;
    }
    errs() << "Matching function: " << F->getName() << "\n";
    FunctionStaticSlicer *FSS = new FunctionStaticSlicer(*F, mp, ps, mod);
    Instruction *inst;
    inst_iterator ii = inst_begin(F);
    bool found = false;
    while ((inst = matcher.matchInstruction(ii, F, scope)) != NULL) {
      const Value *LHS = NULL;
      if (const LoadInst *LI = dyn_cast<LoadInst>(inst)) {
        LHS = LI->getPointerOperand();
      } else if (const StoreInst * SI = dyn_cast<StoreInst>(inst)) {
        LHS = SI->getPointerOperand();
      }
      if (LHS && LHS->hasName() && LHS->getName().equals_lower(SlicingVariable)) {
        errs() << "Matching instruction: \n";
        inst->dump();
        FSS->addInitialCriterion(inst, LHS);
        found = true;
      }
    }
    if (!found) {
      errs() << "No matching instruction for variable " << SlicingVariable << "\n";
      return;
    }
    initFuns.push_back(F);
    slicers.insert(Slicers::value_type(F, FSS));
    buildDicts(ps);
  }

  StaticSlicer::~StaticSlicer() {
    for (Slicers::const_iterator I = slicers.begin(), E = slicers.end();
        I != E; ++I)
      delete I->second;
  }

  FunctionStaticSlicer * StaticSlicer::getFSS(const Function * F) {
    Slicers::iterator si;
    si = slicers.find(F);
    if (si == slicers.end()) {
      Function *f = const_cast<Function *>(F);
      FunctionStaticSlicer *FSS = new FunctionStaticSlicer(*f, mp, ps, mod);
      slicers.insert(Slicers::value_type(F, FSS));
      return FSS;
    }
    return si->second;
  }

  void StaticSlicer::computeSlice() {
    typedef SmallVector<const Function *, 20> WorkSet;
    WorkSet Q(initFuns);

    while (!Q.empty()) {
      for (WorkSet::iterator f = Q.begin(); f != Q.end(); ++f) {
        FunctionStaticSlicer *fss = getFSS(*f);
        fss->calculateStaticSlice();
      }

      WorkSet tmp;
      for (WorkSet::iterator f = Q.begin(); f != Q.end(); ++f) {
        emitToCalls(*f, std::inserter(tmp, tmp.end()));
        emitToExits(*f, std::inserter(tmp, tmp.end()));
      }
      std::swap(tmp,Q);
    }
  }

  bool StaticSlicer::sliceModule() {
    bool modified = false;
    for (Slicers::iterator s = slicers.begin(); s != slicers.end(); ++s)
      modified |= s->second->slice();
    if (modified)
      for (Module::iterator I = module.begin(), E = module.end(); I != E; ++I)
        if (!I->isDeclaration())
          FunctionStaticSlicer::removeUndefs(mp, *I);
    return modified;
  }
}}

namespace {
  class Slicer : public ModulePass {
    public:
      static char ID;

      Slicer() : ModulePass(ID) {}

      virtual bool runOnModule(Module &M);

      void getAnalysisUsage(AnalysisUsage &AU) const {
        AU.addRequired<PostDominatorTree>();
        AU.addRequired<PostDominanceFrontier>();
      }
  };
}

static RegisterPass<Slicer> X("slice-inter", "Slices the code interprocedurally");
char Slicer::ID;

bool Slicer::runOnModule(Module &M) {
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

  slicing::StaticSlicer SS(this, M, PS, CG, MOD);
  SS.computeSlice();
  return false;
  // return SS.sliceModule();
}