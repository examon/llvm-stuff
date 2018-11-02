// Created by Tomas Meszaros (exo at tty dot com, tmeszaro at redhat dot com)
//
// Published under Apache 2.0 license.
// See LICENSE for details.

// What is this?
// This is header file for apex.cpp

#pragma once

#include <list>
#include <map>
#include <string>
#include <vector>

#include "llvm/ADT/APInt.h"
#include "llvm/IR/DebugInfoMetadata.h"
#include "llvm/IR/IRBuilder.h"
#include "llvm/IR/InstIterator.h"
#include "llvm/IR/LLVMContext.h"
#include <llvm/Analysis/CallGraph.h>
#include <llvm/IR/Instructions.h>
#include <llvm/IR/LegacyPassManager.h>
#include <llvm/Support/CommandLine.h>
#include <llvm/Transforms/IPO/PassManagerBuilder.h>

// We need this for dg integration.
#include "analysis/PointsTo/PointsToFlowInsensitive.h"
#include "llvm/LLVMDependenceGraph.h"
#include "llvm/analysis/DefUse.h"

using namespace llvm;
using namespace dg;

/// For printing additional debug information.
bool VERBOSE_DEBUG = false;

/// Error code when we want to crash APEXPass.
int FATAL_ERROR = -1;

/// Protected functions IDs. These will not be removed by APEXPass.
std::vector<std::string> PROTECTED_FCNS = {"lib_test", "lib_exit", "exit",
                                           "printf", "llvm.dbg.declare"};

/// Command line arguments for opt.
cl::opt<std::string> ARG_FILE(
    "file", cl::desc("Filename in relative path from to the launching script."),
    cl::value_desc("file name (e.g. code/main.c)"));

cl::opt<std::string> ARG_LINE("line",
                              cl::desc("Number representing line in the file."),
                              cl::value_desc("Source code line number."));

/// Node is usually line instruction of IR. Sometimes whole function.
struct APEXDependencyNode {
  LLVMNode *node;
  Value *value;
  std::vector<LLVMNode *> control_depenencies;
  std::vector<LLVMNode *> rev_control_depenencies;
  std::vector<LLVMNode *> data_dependencies;
  std::vector<LLVMNode *> rev_data_dependencies;
};

/// Function (can contain multiple basic blocks).
struct APEXDependencyFunction {
  Value *value;
  std::vector<APEXDependencyNode> nodes;
};

/// Graph: consists of nodes that are functions.
struct APEXDependencyGraph {
  std::vector<APEXDependencyFunction> functions;
  // Mapping of nodes to dependencies & functions.
  std::map<LLVMNode *, std::vector<LLVMNode *>> node_data_dependencies_map;
  std::map<LLVMNode *, std::vector<LLVMNode *>> node_rev_data_dependencies_map;
  std::map<LLVMNode *, APEXDependencyFunction *> node_function_map;
};

/// Actual APEX pass.
class APEXPass : public ModulePass {
public:
  static char ID;
  APEXPass() : ModulePass(ID) {}
  bool runOnModule(Module &M) override;

private:
  // Logging utilities.
  void logPrint(const std::string &message);
  void logPrintUnderline(const std::string &message);
  void logPrintFlat(const std::string &message);
  void logDumpModule(const Module &M);

  // Function utilities.
  void functionVectorFlatPrint(const std::vector<Function *> &functions);
  int functionGetCallers(const Function *F, std::vector<Function *> &callers);
  int functionGetCallees(const Function *F, std::vector<Function *> &callees);
  void functionCollectDependencies(APEXDependencyGraph &apex_dg,
                                   std::string function_nam,
                                   std::vector<LLVMNode *> &dependencies);
  bool functionIsProtected(Function *F);
  bool functionInPath(Function *F, const std::vector<Function *> &path);

  // Callgraph utilities.
  int createCallGraphOrDie(
      const Module &M, const std::string &root,
      std::vector<std::pair<Function *, std::vector<Function *>>> &callgraph);
  void printCallGraph(
      const std::vector<std::pair<Function *, std::vector<Function *>>>
          &callgraph);
  int findPathOrDie(
      const std::vector<std::pair<Function *, std::vector<Function *>>>
          &callgraph,
      const std::string &source, const std::string &target,
      std::vector<Function *> &final_path);
  void printPath(const std::vector<Function *> &path, const std::string &source,
                 const std::string &target);

  // dg utilities.
  void dgInit(Module &M, LLVMDependenceGraph &dg);

  // apex dg utilities.
  void apexDgInit(APEXDependencyGraph &apex_dg);
  void apexDgGetBlockNodeInfo(APEXDependencyNode &apex_node, LLVMNode *node);
  void apexDgPrint(APEXDependencyGraph &apex_dg, bool verbose);
  void apexDgPrintDataDependeniesCompact(APEXDependencyGraph &apex_dg);
  void apexDgNodeResolveDependencies(std::vector<Function *> &path,
                                     APEXDependencyGraph &apex_dg,
                                     const APEXDependencyNode &node);
  void apexDgNodeResolveRevDependencies(std::vector<Function *> &path,
                                        APEXDependencyGraph &apex_dg,
                                        const APEXDependencyNode &node);
  void updatePathAddDependencies(const std::vector<LLVMNode *> &dependencies,
                                 std::vector<Function *> &path);

  void
  apexDgFindDataDependencies(APEXDependencyGraph &apex_dg, LLVMNode &node,
                             std::vector<LLVMNode *> &dependencies,
                             std::vector<LLVMNode *> &rev_data_dependencies);

  // Module utilities.
  void moduleParseCmdLineArgsOrDie(void);
  std::vector<Instruction *>
  moduleFindTargetInstructionsOrDie(Module &M, const std::string &file,
                                    const std::string &line);
  void moduleRemoveFunctionsNotInPath(Module &M, APEXDependencyGraph &apex_dg,
                                      std::vector<Function *> &path);
  void moduleInsertExitAfterTarget(Module &M,
                                   const std::vector<Function *> &path,
                                   const std::string &target_id);
};

/// Registering our own pass, so it can be ran via opt.
char APEXPass::ID = 0;
static RegisterPass<APEXPass> X("apex", "Just a test pass. Work in progress.",
                                false /* Only looks at CFG */,
                                false /* Analysis Pass */);