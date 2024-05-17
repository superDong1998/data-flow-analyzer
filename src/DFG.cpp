#include <llvm/Pass.h>

#include "llvm/IR/DerivedUser.h"
#include "llvm/IR/GetElementPtrTypeIterator.h"
#include "llvm/IR/IRBuilder.h"
#include "llvm/IR/InstIterator.h"
#include "llvm/IR/IntrinsicInst.h"
#include <llvm/IR/BasicBlock.h>
#include <llvm/IR/Constant.h>
#include <llvm/IR/Function.h>
#include <llvm/IR/Instruction.h>
#include <llvm/IR/Instructions.h>
#include <llvm/IR/Module.h>
#include <llvm/IR/Operator.h>
#include <llvm/IR/Use.h>
#include <llvm/IR/User.h>
#include <llvm/IR/Value.h>

#include <llvm/Analysis/CFG.h>
#include <llvm/Analysis/CallGraphSCCPass.h>
#include <llvm/Analysis/LoopInfo.h>
#include <llvm/Analysis/LoopPass.h>
#include <llvm/Analysis/ScalarEvolution.h>
#include <llvm/Analysis/DependenceAnalysis.h>

#include <llvm/Support/raw_ostream.h>
//#include <llvm/DebugInfo.h>

#include "dbg.h"
#include "pattern.h"
#include "loop_mem_pat_node.h"
#include "loop_unroll_analysis.h"
#include <list>
#include <set>
#include <map>
#include <unordered_map>
#include <sstream>
#include <string>

// #define DEBUG

using namespace llvm;
namespace {

struct LoopNodes {
    Loop *loop;
    std::vector<LoopMemPatNode *> nodes; // nodes in the loop
    std::vector<LoopNodes> sub_loops;
};

// counter for loop, the subloops are also included
int counter = 0;

// for guass_sediel: please define the correct loop start source line
std::string while_loop = "1275"; // while loop start
std::string loop0 = "1213"; // [target loop 8] 
std::string loop1 = "1297"; // [target loop 1]
std::string loop2 = "1331"; // [target loop 9]
std::string loop3 = "1342"; // [target loop 2]
std::string loop4 = "1370"; // [target loop 3]
std::string loop5 = "1391"; // [target loop 4]
std::string loop6 = "1411"; // [target loop 5]
std::string loop7 = "1441"; // [target loop 6]
std::string loop8 = "1477"; // [target loop 7]

struct DFGPass : public ModulePass {
public:
  static char ID;

  typedef std::pair<Value *, std::string> node;
  typedef std::pair<node, node> edge;
  typedef std::list<node> node_list;
  typedef std::list<edge> edge_list;
  typedef std::vector<LoopNodes> loop_list;

  std::map<Value *, std::string> variant_value;
  std::vector<Loop *> loop_stack;

  // std::error_code error;
  edge_list inst_edges; // control flow
  edge_list edges;      // data flow
  node_list nodes;      // instruction
  loop_list loops;      // nodes find in loop
  std::unordered_map<Value*, std::string> value_to_name;

  int num;
  int func_id = 0;
  DFGPass() : ModulePass(ID) { num = 0; }

  void getAnalysisUsage(AnalysisUsage &AU) const override {
    // AU.addRequired<CFG>();
    AU.setPreservesCFG();
    AU.addRequired<LoopInfoWrapperPass>();
    AU.addRequired<ScalarEvolutionWrapperPass>();
    AU.addRequired<DependenceAnalysisWrapperPass>();
    // AU.addRequired<RegionInfo>();
    ModulePass::getAnalysisUsage(AU);
  }

  void dumpGraph(raw_fd_ostream &file, Function *F) {
    // errs() << "Write\n";
    file << "digraph \"DFG for'" + F->getName() + "\' function\" {\n";

    // dump node
    for (node_list::iterator node = nodes.begin(), node_end = nodes.end();
         node != node_end; ++node) {
      // errs() << "Node First:" << node->first << "\n";
      // errs() << "Node Second:" << node-> second << "\n";
      if (dyn_cast<Instruction>(node->first)) {
        // file << "\tNode" << node->second << "[shape=record, label=\""
        //      << *(node->first) << "\"];\n";
        file << "\tNode" << node->first << "[shape=record, label=\""
             << node->second.c_str() << "\"];\n";
      } else {
        file << "\tNode" << node->first << "[shape=record, label=\""
             << node->second.c_str() << "\"];\n";
      }
    }

    //  dump instruction edges
#ifdef CFG
    file << "edge [color=blue]"
            << "\n";
    for (edge_list::iterator edge = inst_edges.begin(),
                             edge_end = inst_edges.end();
         edge != edge_end; ++edge) {
      file << "\tNode" << edge->first.first << " -> Node" <<
      edge->second.first << "\n";
    }
#endif

    // dump data flow edges
  #ifdef DFG
    file << "edge [color=red]"
         << "\n";
    for (edge_list::iterator edge = edges.begin(), edge_end = edges.end();
         edge != edge_end; ++edge) {
      file << "\tNode" << edge->first.first << " -> Node" << edge->second.first
           << "\n";
    }
  #endif

    file << "}\n";
  }

  // Convert instruction to string
  std::string convertIns2Str(Instruction *ins) {
    std::string temp_str;
    raw_string_ostream os(temp_str);
    ins->print(os);
    return os.str();
  }

  template <class ForwardIt, class T>
  ForwardIt remove(ForwardIt first, ForwardIt last, const T &value) {
    first = std::find(first, last, value);
    if (first != last)
      for (ForwardIt i = first; ++i != last;)
        if (!(*i == value))
          *first++ = std::move(*i);
    return first;
  }

  // If v is variable, then use the name.
  // If v is instruction, then use the content.
  std::string getValueName(Value *v) {
    std::string temp_result = "#val";
    if (!v) {
      return "undefined";
    } 
    if (value_to_name.find(v) != value_to_name.end()) {
      return value_to_name[v];
    }
    if (v->getName().empty()) {
      if (isa<ConstantInt>(v)) {
        auto constant_v = dyn_cast<ConstantInt>(v);
        temp_result = std::to_string(constant_v->getSExtValue());
      } else {
        temp_result += std::to_string(num);
        num++;
      }
      // errs() << temp_result << "\n";
    } else {
      temp_result = v->getName().str();
      // errs() << temp_result << "\n";
    }
    value_to_name[v] = std::string(temp_result);
    return temp_result;
  }

  // const MDNode *findVar(const Value *V, const Function *F) {
  //   for (const_inst_iterator Iter = inst_begin(F), End = inst_end(F);
  //        Iter != End; ++Iter) {
  //     const Instruction *I = &*Iter;
  //     if (const DbgDeclareInst *DbgDeclare = dyn_cast<DbgDeclareInst>(I)) {
  //       if (DbgDeclare->getAddress() == V) {
  //         return DbgDeclare->getVariable();
  //         // return DbgDeclare->getOperand(1);
  //       }
  //     } else if (const DbgValueInst *DbgValue = dyn_cast<DbgValueInst>(I)) {
  //       // errs() << "\nvalue:" <<DbgValue->getValue()  <<
  //       // *(DbgValue->getValue()) << "\nV:" << V << *V<< "\n";
  //       if (DbgValue->getValue() == V) {
  //         return DbgValue->getVariable();
  //         // return DbgValue->getOperand(1);
  //       }
  //     }
  //   }
  //   return nullptr;
  // }

  // std::string getDbgName(const Value *V, Function *F) {
  //   // TODO handle globals as well

  //   // const Function* F = findEnclosingFunc(V);
  //   if (!F)
  //     return V->getName().str();

  //   const MDNode *Var = findVar(V, F);
  //   if (!Var)
  //     return "tmp";

  //   // MDString * mds = dyn_cast_or_null<MDString>(Var->getOperand(0));
  //   // //errs() << mds->getString() << '\n';
  //   // if(mds->getString().str() != std::string("")) {
  //   // return mds->getString().str();
  //   // }else {
  //   // 	return "##";
  //   // }

  //   auto var = dyn_cast<DIVariable>(Var);
  //   // DIVariable *var(Var);

  //   return var->getName().str();
  // }

  bool endWith(const std::string &str, const std::string &tail) {
    return str.compare(str.size() - tail.size(), tail.size(), tail) == 0;
  }

  bool startWith(const std::string &str, const std::string &head) {
    return str.compare(0, head.size(), head) == 0;
  }

  Value *getLoopIndvar(Loop *L, ScalarEvolution &SE) {
    // auto phi = dyn_cast<PHINode>(L);
    PHINode *indvar_phinode = L->getInductionVariable(SE);
    // errs() << " phi: "<< indvar_phinode << '\n';
    // Value *indvar = dyn_cast<Value>(indvar_phinode);
    // return indvar;
    return indvar_phinode;
  }

  bool isLoopIndVar(Value *v) {
    auto iter = variant_value.find(v);
    if (iter != variant_value.end()) {
      return true;
    }
    return false;
  }

  Value *getLoopInitVar(Loop *L, ScalarEvolution &SE) {
    auto loop_bound = L->getBounds(SE);
    if (loop_bound) {
      return &(loop_bound->getInitialIVValue());
    } else {
      return nullptr;
    }
  }

  Value *getLoopStepVar(Loop *L, ScalarEvolution &SE) {
    auto loop_bound = L->getBounds(SE);
    if (loop_bound) {
      return loop_bound->getStepValue();
    } else {
      return nullptr;
    }
  }

  Value *getLoopEndVar(Loop *L, ScalarEvolution &SE) {
    auto loop_bound = L->getBounds(SE);
    if (loop_bound) {
      return &(loop_bound->getFinalIVValue());
    } else {
      return nullptr;
    }
  }

  PatNode *getGEPPattern(GetElementPtrInst *gep_inst, DataLayout *DL) {
    GEPOperator *gep_op = dyn_cast<GEPOperator>(gep_inst);
    Value *obj = gep_op->getPointerOperand();
    // errs() << getValueName(obj) << '\n';

    PatNode **patnode_array = new PatNode *[loop_stack.size()];

    for (int l = loop_stack.size() - 1; l >= 0; l--) {
      PatNode *gep_node = new PatNode(gep_inst, GEP_INST, getValueName(obj));
      auto LL = loop_stack[l];
      int num_operand = gep_inst->getNumOperands();
      for (int i = 1; i < num_operand; i++) {
        Value *idx = gep_inst->getOperand(i);
        PatNode *op_node = getOpPattern(idx, LL);
        gep_node->addChild(op_node);
      }
      // dumpPattern(gep_node, 0);
      patnode_array[l] = gep_node;
    }

    // PatNode *gep_node1 = new PatNode(gep_inst, GEP_INST, getValueName(obj));

    // // int num_operand = gep_inst->getNumOperands();
    // for (int i = 1; i < num_operand; i++) {
    //   Value *idx = gep_inst->getOperand(i);
    //   PatNode *op_node = getOpPattern(idx, loop_stack[loop_stack.size() -
    //   2]); gep_node1->addChild(op_node);
    // }
    // dumpPattern(gep_node1, 0);

    // PatNode *gep_node2 = new PatNode(gep_inst, GEP_INST, getValueName(obj));

    // // int num_operand = gep_inst->getNumOperands();
    // for (int i = 1; i < num_operand; i++) {
    //   Value *idx = gep_inst->getOperand(i);
    //   PatNode *op_node = getOpPattern(idx, loop_stack[loop_stack.size() -
    //   3]); gep_node2->addChild(op_node);
    // }
    // dumpPattern(gep_node2, 0);

    return patnode_array[0];
  }

  PatNode *getBinaryOpPattern(BinaryOperator *bin_op, Loop *L) {
    auto opcode = bin_op->getOpcode();
    // errs() << opcode << '\n';
    std::ostringstream oss;
    switch (opcode) {
    case 11:
    case llvm::Instruction::Add:
      oss << "+";
      break;
    case llvm::Instruction::Sub:
      oss << '-';
      break;
    case llvm::Instruction::UDiv:
      oss << '/';
      break;
    case llvm::Instruction::SDiv:
      oss << '/';
      break;
    case llvm::Instruction::Mul:
      oss << '*';
      break;
    case llvm::Instruction::FAdd:
      oss << "+";
      break;
    case llvm::Instruction::FSub:
      oss << '-';
      break;
    case llvm::Instruction::FDiv:
      oss << '/';
      break;
    case llvm::Instruction::FMul:
      oss << '*';
      break;
    case llvm::Instruction::Shl:
      oss << "<<";
      break;
    case llvm::Instruction::LShr:
      oss << ">>";
      break;
    default:
      oss << "opcode: " << opcode;
      break;
    }

    PatNode *bin_node = new PatNode(bin_op, BIN_OP, oss.str());
#ifdef DEBUG
    errs() << "Process binary op " << getValueName(bin_op) << ": bin op is "
           << oss.str() << '\n';
#endif

    // Traverse all operands
    int num_operands = bin_op->getNumOperands();
    for (int i = 0; i < num_operands; i++) {
      auto operand = bin_op->getOperand(i);
      // auto operand = dyn_cast<Instruction>(operandi);
#ifdef DEBUG
      errs() << "  Process binary op operand " << getValueName(operand) << ": "
             << getValueName(operand) << '\n';
#endif
      if (!L->isLoopInvariant(operand)) {
#ifdef DEBUG
        errs() << "  variant vars: " << getValueName(operand) << '\n';
#endif
        auto child = getOpPattern(dyn_cast<Instruction>(operand), L);
        bin_node->addChild(child);
      } else if (isa<ConstantInt>(operand)) {
#ifdef DEBUG
        errs() << "  invariant vars: " << getValueName(operand) << '\n';
#endif
        auto child = getOpPattern(operand, L);
        bin_node->addChild(child);
      } else {
        PatNode *invar_var =
            new PatNode(operand, OTHERS, getValueName(operand));
        bin_node->addChild(invar_var);
      }
    }

    return bin_node;
  }

  void getSExtPattern(SExtInst *sext_inst) { 
#ifdef DEBUG  
    errs() << '=' << '\n'; 
#endif
  }
  PatNode *getCastPattern(CastInst *sext_inst, Loop *L) {
    PatNode *cast_node = new PatNode(sext_inst, CAST_INST,
                                     getValueName(sext_inst->getOperand(0)));
#ifdef DEBUG
    errs() << "Process cast " << getValueName(sext_inst) << '\n';
#endif
    // traverse operand
    auto operand0 = sext_inst->getOperand(0);
    auto operand = dyn_cast<Instruction>(operand0);
#ifdef DEBUG
    errs() << "  Process cast operand " << getValueName(operand) << '\n';
#endif
    if (!L->isLoopInvariant(operand)) {
#ifdef DEBUG
      errs() << "  variant vars " << getValueName(operand) << '\n';
#endif
      auto child = getOpPattern(operand, L);

      cast_node->addChild(child);
    } else if (isa<ConstantInt>(operand)) {
#ifdef DEBUG
      errs() << "  invariant vars " << getValueName(operand) << '\n';
#endif
      auto child = getOpPattern(operand, L);

      cast_node->addChild(child);
    }

    return cast_node;
  }

  PatNode *getConstPattern(ConstantInt *const_v) {
#ifdef DEBUG
    errs() << "Process constant " << getValueName(const_v)
           << ": value = " << const_v->getSExtValue() << '\n';
#endif
    std::string temp_result = std::to_string(const_v->getSExtValue());
    PatNode *const_node = new PatNode(const_v, CONSTANT, temp_result);
    return const_node;
  }

  PatNode *getOpPattern(Instruction *curII, Loop *L) {
    if (isLoopIndVar(curII)) {
      PatNode *indvar_node =
          new PatNode(curII, LOOP_IND_VAR, getValueName(curII));
      return indvar_node;
    } else if (isa<BinaryOperator>(curII)) {
      auto bin_op = dyn_cast<BinaryOperator>(curII);
      return getBinaryOpPattern(bin_op, L);
    } else if (isa<CastInst>(curII)) {
      auto sext_inst = dyn_cast<CastInst>(curII);
      return getCastPattern(sext_inst, L);
    } else if (isa<ConstantInt>(curII)) {
      auto constant_v = dyn_cast<ConstantInt>(curII);
      return getConstPattern(constant_v);
    } else if (isa<PHINode>(curII)) {
      PatNode *phi_node = new PatNode(curII, OTHERS, getValueName(curII));
      return phi_node;
    } else {
      // end node for start loop: by default: 20
      // std::cout << "checking " << curII->getOpcodeName() << std::endl;
      PatNode *const_node = new PatNode(curII, CONSTANT, std::to_string(8));
      return const_node;
    }

    return nullptr;
  }

  void handleLoop(Loop *L, LoopInfo &LI, DataLayout *DL, ScalarEvolution &SE,
                  Function *F, LoopMemPatNode* parent_node, LoopNodes &loopnodes) {
    loop_stack.push_back(L);
    Value *indvar = getLoopIndvar(L, SE);
    Value *loop_init_var = getLoopInitVar(L, SE);
    Value *loop_step_var = getLoopStepVar(L, SE);
    Value *loop_end_var = getLoopEndVar(L, SE);
    // getValueName(indvar, F)

    variant_value.insert(make_pair(indvar, std::string("xx")));

    std::string loop_ind_var_str = getValueName(indvar);
    std::string loop_init_var_str = getValueName(loop_init_var);
    std::string loop_step_var_str = getValueName(loop_step_var);
    std::string loop_end_var_str = getValueName(loop_end_var);
    // dbg(loop_init_var_str);
    // dbg(loop_step_var_str);
    // dbg(loop_end_var_str);
    PatNode* loop_init_var_pat_node = getOpPattern(loop_init_var, L);
    PatNode* loop_step_var_pat_node = getOpPattern(loop_step_var, L);
    PatNode* loop_end_var_pat_node = getOpPattern(loop_end_var, L);


    // LoopPat* loop_pat = new LoopPat(loop_ind_var_str);
    LoopPat* loop_pat = new LoopPat(loop_ind_var_str, loop_init_var_pat_node, loop_end_var_pat_node, loop_step_var_pat_node);
    LoopMemPatNode* loop_node = new LoopMemPatNode(LOOP_NODE, loop_pat);
    parent_node->addChild(loop_node);
    loopnodes.nodes.push_back(loop_node);

    // dbg(indvar); 
    // errs() << "Loop index var:" << getValueName(indvar) << "\n\n";

    for (Loop::block_iterator BB = L->block_begin(), BEnd = L->block_end();
         BB != BEnd; ++BB) {
      BasicBlock *curBB = *BB;
      // check if bb belongs to L or inner loop, traverse the BB of Loop
      // itself.
      if (L != LI.getLoopFor(curBB)) {
        continue;
      }
      for (BasicBlock::iterator II = curBB->begin(), IEnd = curBB->end();
           II != IEnd; ++II) {

        Instruction *curII = &*II;

        if (isa<GetElementPtrInst>(curII)) {
          // errs() << *(curII) << "\n";
          GetElementPtrInst *gepinst = dyn_cast<GetElementPtrInst>(curII);

          //  auto user = dyn_cast<User>(curII);
          auto gep_pat = getGEPPattern(gepinst, DL);

          auto val = dyn_cast<Value>(curII);
          // auto use = dyn_cast<User>(val);
          auto users = val->users();
          
          for(auto user: users) {
            Instruction *next = dyn_cast<Instruction>(user);
            int mode;
            if (isa<LoadInst>(next)) {
              mode = READ;
            } else if (isa<StoreInst>(next)) {
              mode = WRITE;
            } else {
              mode = 0;
            }
            dbg(mode);
            MemAcsPat* mem_acs_pat = new MemAcsPat(gep_pat, mode);
            LoopMemPatNode* mem_acs_node = new LoopMemPatNode(MEM_ACS_NODE, mem_acs_pat);
            loop_node->addChild(mem_acs_node);
            loopnodes.nodes.push_back(mem_acs_node);
          }
          
          

          // // old version for simple pattern A[i]
          // GEPOperator *gepop = dyn_cast<GEPOperator>(curII);
          // Value *obj = gepop->getPointerOperand();
          // Value *idx = gepinst->getOperand(1);
          // errs() << getOriginalName(obj, F) << "[" << getOriginalName(idx, F)
          //        << "]" << '\n';
        }

        switch (curII->getOpcode()) {
        // Load and store instruction
        case llvm::Instruction::Load: {
          LoadInst *linst = dyn_cast<LoadInst>(curII);
          Value *loadValPtr = linst->getPointerOperand();
          edges.push_back(edge(node(loadValPtr, getValueName(loadValPtr)),
                               node(curII, getValueName(curII))));
          break;
        }
        case llvm::Instruction::Store: {
          StoreInst *sinst = dyn_cast<StoreInst>(curII);
          Value *storeValPtr = sinst->getPointerOperand();
          Value *storeVal = sinst->getValueOperand();
          edges.push_back(edge(node(storeVal, getValueName(storeVal)),
                               node(curII, getValueName(curII))));
          edges.push_back(edge(node(curII, getValueName(curII)),
                               node(storeValPtr, getValueName(storeValPtr))));
          break;
        }
        default: {
          for (Instruction::op_iterator op = curII->op_begin(),
                                        opEnd = curII->op_end();
               op != opEnd; ++op) {
            Instruction *tempIns;
            if (dyn_cast<Instruction>(*op)) {
              edges.push_back(edge(node(op->get(), getValueName(op->get())),
                                   node(curII, getValueName(curII))));
            }
          }
          break;
        }
        }
        BasicBlock::iterator next = II;
        nodes.push_back(node(curII, getValueName(curII)));
        ++next;
        if (next != IEnd) {
          inst_edges.push_back(edge(node(curII, getValueName(curII)),
                                    node(&*next, getValueName(&*next))));
        }
      }

      Instruction *terminator = curBB->getTerminator();
      for (BasicBlock *sucBB : successors(curBB)) {
        Instruction *first = &*(sucBB->begin());
        inst_edges.push_back(edge(node(terminator, getValueName(terminator)),
                                  node(first, getValueName(first))));
      }
    }

    // traverse the inner Loops by recursive method
    std::vector<Loop *> subLoops = L->getSubLoops();
    Loop::iterator SL, SLE;
    for (SL = subLoops.begin(), SLE = subLoops.end(); SL != SLE; ++SL) {
      LoopNodes sub_LN;
      sub_LN.loop = *SL;
      DebugLoc Loc = (*SL)->getStartLoc();
      if (!(*SL)->getHeader()->hasName()) {
        (*SL)->getHeader()->setName("loop" + std::to_string(counter) + " line:" + std::to_string(Loc.getLine()));
        counter++;
      }
      handleLoop(*SL, LI, DL, SE, F, loop_node, sub_LN);
      for (auto sub = sub_LN.nodes.begin(), sub_end = sub_LN.nodes.end(); sub != sub_end; ++sub) {
        loopnodes.nodes.push_back(*sub);
      }
      loopnodes.sub_loops.push_back(sub_LN);
    }
    loop_stack.pop_back();
  }

  void loopDepAnalysis(LoopMemPatNode* n) {
    auto type = n->getType();
    auto has_loop_child = n->hasLoopChild();
    if(type == LOOP_NODE && has_loop_child == false) {
      LoopUnrollAnalysis* loop_unroll_analysis = new LoopUnrollAnalysis(n);
      loop_unroll_analysis->checkDependence();
    }
    
    auto children = n->getChildren();
    for(auto child: children) {
      loopDepAnalysis(child);
    }

  }


  std::string filter(PatNode* node) {
    if (node->getType() == CAST_INST) {
      return node->getOp();
    } else if (node->getType() == BIN_OP) {
      return "";
    }

    if (node->getType() != CONSTANT){
      return node->getValueName();
    }
    
    return "";
  }

  std::vector<std::string> filter_nodes(LoopMemPatNode &node) {
    auto type = node.getType();
    std::set<std::string> result;
   
    if (type == LOOP_NODE) {
      auto loop_pat = node.getLoopPat();
      auto start_pat = loop_pat->getStartNode();
      auto end_pat = loop_pat->getEndNode();
      auto step_pat = loop_pat->getStepNode();
      result.insert(filter(start_pat));
      result.insert(filter(end_pat));
      result.insert(filter(step_pat));
      for (auto &child : start_pat->getChildren()) {
        result.insert(filter(child));
      }
      for (auto &child : end_pat->getChildren()) {
        result.insert(filter(child));
      }
      for (auto &child : step_pat->getChildren()) {
        result.insert(filter(child));
      }
    } else if (type == MEM_ACS_NODE) {
      auto mem_acs_pat = node.getMemAcsPat();
      auto pat = mem_acs_pat->getPatNode();
      auto mode = mem_acs_pat->getAccessMode();
      std::string value = filter(pat);
      value += "_";
      value += std::to_string(mode);
      result.insert(value);
      for (auto &child : pat->getChildren()) {
        result.insert(filter(child));
      }
    } else{
      //do nothing
    }

    std::vector<std::string> result_convert(result.begin(), result.end());
    return result_convert;
  }

  void interLoopAnalysis(loop_list &loops, DependenceInfo &dep) {
    std::vector<std::string> dependences;
     for (size_t i = 0; i < loops.size(); ++i) {
        for (size_t j = i + 1; j < loops.size(); ++j) {
            const auto &loop1 = loops[i];
            const auto &loop2 = loops[j];
            bool foundCommonNode = false;
            for (auto &node1 : loop1.nodes) {
                for (auto &node2 : loop2.nodes) {
                    // ignore constant value
                    auto f_node1 = filter_nodes(*node1);
                    auto f_node2 = filter_nodes(*node2);

                    for (auto f_sub1 : f_node1) {
                      for (auto f_sub2 : f_node2) {
                        if (!f_sub1.empty() && !f_sub2.empty()) {
                          if ((f_sub1.find("_200") == std::string::npos && f_sub1.find("_201") == std::string::npos) &&
                              (f_sub2.find("_200") == std::string::npos && f_sub2.find("_201") == std::string::npos)) {
                            if (f_sub1 == f_sub2) {
                              dependences.push_back("dependence between loops "
                                          + loop1.loop->getName().str() + " and " + loop2.loop->getName().str() + " at "
                                          + f_sub1 + " and " + f_sub2);
                                foundCommonNode = true;
                                break;
                            }
                          } else {
                            if ((f_sub1.find("_200") != std::string::npos && f_sub2.find("_201") !=          std::string::npos) ||
                                (f_sub1.find("_201") != std::string::npos && f_sub2.find("_200") != std::string::npos) || 
                                (f_sub1.find("_201") != std::string::npos && f_sub2.find("_201") != std::string::npos)) {
                                  std::string prefix1 = f_sub1.substr(0, f_sub1.find("_"));
                                  std::string prefix2 = f_sub2.substr(0, f_sub2.find("_"));
                                  if (prefix1 == prefix2) {
                                    dependences.push_back("dependence between loops "
                                          + loop1.loop->getName().str() + " and " + loop2.loop->getName().str() + " at "
                                          + f_sub1 + " and " + f_sub2);
                                    foundCommonNode = true;
                                    break;
                                  }
                            }
                        }
                          
                        }
                      }
                      if (foundCommonNode) break;
                    }

                    if (foundCommonNode) break;  
                }
                if (foundCommonNode) break;
            }
        }
      }
    for (std::string out: dependences) {
      if (out.find(loop2) != std::string::npos && out.find(loop5) != std::string::npos) {
        continue;
      }
      if (out.find(loop2) != std::string::npos && out.find(loop6) != std::string::npos) {
        continue;
      }
      if (out.find(loop2) != std::string::npos && out.find(loop0) != std::string::npos) {
        continue;
      }
      if (out.find(loop3) != std::string::npos && out.find(loop6) != std::string::npos) {
        continue;
      }
      std::cout<< out << std::endl;
    }
  }

  bool filter_same_loop(loop_list& loops, const std::string& substr) {
    for (const auto& temp : loops) {
        if (temp.loop->getName().find(substr) != std::string::npos) {
            return true;
        }
    }
    return false;
  }

  void funcDFG(Function *F, Module &M) {
    LoopInfo &LI = getAnalysis<LoopInfoWrapperPass>(*F).getLoopInfo();
    ScalarEvolution &SE = getAnalysis<ScalarEvolutionWrapperPass>(*F).getSE();
    std::error_code error;
    enum sys::fs::OpenFlags F_None;
    // errs() << func_id << ": " << F->getName().str() << "\n";
    StringRef fileName(std::to_string(func_id) + ".dot");
    func_id++;
    raw_fd_ostream file(fileName, error, F_None);

    edges.clear();
    nodes.clear();
    inst_edges.clear();
    loops.clear();

    DataLayout *DL = new DataLayout(&M);

    DependenceInfo &dep =  getAnalysis<DependenceAnalysisWrapperPass>(*F).getDI();

    LoopMemPatNode* func_node = new LoopMemPatNode(FUNC_NODE, F->getName().str());
    for (LoopInfo::iterator LL = LI.begin(), LEnd = LI.end(); LL != LEnd;
         ++LL) {
      Loop *L = *LL;
      LoopNodes LN;
      LN.loop = L;
      
      DebugLoc Loc = L->getStartLoc();
      if (Loc) {
          unsigned Line = Loc.getLine();
      }
      if (!L->getHeader()->hasName()) {
        L->getHeader()->setName("loop" + std::to_string(counter) + " line:" + std::to_string(Loc.getLine()));
        counter++;
      }
      handleLoop(L, LI, DL, SE, F, func_node, LN);
      loops.push_back(LN);
    }

    #if defined(CFG) || defined(DFG)
      dumpGraph(file, F);
    #endif

    // dumpLoopMemPatTree(func_node, 0);
    
    loop_list analyzed_loops;
    // inter analysis for the inner while loop at line 1275 
    for (auto loop = loops.begin(), l_end = loops.end(); loop != l_end; loop++) {
      
      if (loop->loop->getName().str().find(loop0) != std::string::npos) {
        analyzed_loops.push_back(*loop);
        std::cout << "0: " << loop->loop->getName().str() << std::endl;
      }
      if (loop->loop->getName().str() == (std::string("loop2 line:") + while_loop)) {
        // std::cout << "loop has " << loop->sub_loops.size() << " subloops" << std::endl;
        // f_sub1.find("_200") != std::string::npos
        
        for (size_t i = 0; i < loop->sub_loops.size(); ++i) {
          auto temp = loop->sub_loops[i];
          if (!filter_same_loop(analyzed_loops, loop1) && temp.loop->getName().str().find(loop1) != std::string::npos) {
            analyzed_loops.push_back(temp);
            // std::cout << "1: " << temp.loop->getName().str() << std::endl;
          }
          if (!filter_same_loop(analyzed_loops, loop2) && temp.loop->getName().str().find(loop2) != std::string::npos) {
            analyzed_loops.push_back(temp);
            // std::cout << "2: " << temp.loop->getName().str() << std::endl;
          }
          if (!filter_same_loop(analyzed_loops, loop3) && temp.loop->getName().str().find(loop3) != std::string::npos) {
            analyzed_loops.push_back(temp);
            // std::cout << "3: " << temp.loop->getName().str() << std::endl;
          }
          if (!filter_same_loop(analyzed_loops, loop4) && temp.loop->getName().str().find(loop4) != std::string::npos) {
            analyzed_loops.push_back(temp);
            // std::cout << "4: " << temp.loop->getName().str() << std::endl;
          }
          if (!filter_same_loop(analyzed_loops, loop5) && temp.loop->getName().str().find(loop5) != std::string::npos) {
            analyzed_loops.push_back(temp);
            // std::cout << "5: " << temp.loop->getName().str() << std::endl;
          }
          if (!filter_same_loop(analyzed_loops, loop6) && temp.loop->getName().str().find(loop6) != std::string::npos) {
            analyzed_loops.push_back(temp);
            // std::cout << "6: " << temp.loop->getName().str() << std::endl;
          }
          if (!filter_same_loop(analyzed_loops, loop7) && temp.loop->getName().str().find(loop7) != std::string::npos) {
            analyzed_loops.push_back(temp);
            // std::cout << "7: " << temp.loop->getName().str() << std::endl;
          }
          if (!filter_same_loop(analyzed_loops, loop8) && temp.loop->getName().str().find(loop8) != std::string::npos) {
            analyzed_loops.push_back(temp);
            // std::cout << "8: " << temp.loop->getName().str() << std::endl;
          }
        }
      }
    }
    #if defined(INTER_ANALYSIS) && defined(GS_ONLY)
    std::cout << "analyzed loop has " << analyzed_loops.size() << " subloops" << std::endl;
    std::cout << "dependence as follow: " << std::endl;
    interLoopAnalysis(analyzed_loops, dep);
    #endif
    #if defined(INTER_ANALYSIS) && !defined(GS_ONLY)
    std::cout << "analyzed loop has " << loops.size() << " subloops" << std::endl;
    std::cout << "dependence as follow: " << std::endl;
    interLoopAnalysis(loops, dep);
    #endif

    #ifdef INTRA_ANALYSIS && defined(GS_ONLY)
    for (size_t i = 0; i < analyzed_loops.size(); ++i) {
      const auto &temp = analyzed_loops[i];
      // std::cout << "intra dependence of " << temp.loop->getName().str() << " as follow: " << std::endl;
      for (auto &node : temp.nodes) {
        if (node->getType() == LOOP_NODE) {
          loopDepAnalysis(node);
          break;
        }
      }
    }
    #endif

    #ifdef INTRA_ANALYSIS && !defined(GS_ONLY)
    loopDepAnalysis(func_node);
    #endif

    file.close();

    return;
  }

  bool runOnModule(Module &M) override {
    // Mark all recursive functions
    // unordered_map<CallGraphNode *, Function *> callGraphNodeMap;
    // auto &cg = getAnalysis<CallGraph>();
    // for (auto &f: M) {
    // 	auto cgn = cg[&f];
    // 	callGraphNodeMap[cgn] = &f;
    // }
    // scc_iterator<CallGraph *> cgSccIter = scc_begin(&cg);
    // while (!cgSccIter.isAtEnd()) {
    // 	if (cgSccIter.hasLoop()) {
    // 		const vector<CallGraphNode*>& nodeVec = *cgSccIter;
    // 		for (auto cgn: nodeVec) {
    // 			auto f = callGraphNodeMap[cgn];
    // 			recSet.insert(f);
    // 		}
    // 	}
    // 	++cgSccIter;
    // }

    for (auto &F : M) {
      if (!(F.isDeclaration())) {
        #ifdef GS_ONLY
        if (F.getName() == "gauss_seidel") {
          // std::cout << F.getName().str() << std::endl;
          funcDFG(&F, M);
        }
        #else
          // std::cout << F.getName().str() << std::endl;
          funcDFG(&F, M);
        #endif
      }
    }
    return true;
  }
};
} // namespace

char DFGPass::ID = 0;
static RegisterPass<DFGPass> X("DFGPass", "DFG Pass Analyze", false, false);