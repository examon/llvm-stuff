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

#include "llvm/IR/DIBuilder.h"
#include "llvm/IR/DebugInfo.h"
#include "llvm/IR/Metadata.h" // For StorageType
#include <llvm/ADT/APInt.h>
#include <llvm/Analysis/CallGraph.h>
#include <llvm/IR/DebugInfoMetadata.h>
#include <llvm/IR/IRBuilder.h>
#include <llvm/IR/InstIterator.h>
#include <llvm/IR/Instructions.h>
#include <llvm/IR/LLVMContext.h>
#include <llvm/IR/LegacyPassManager.h>
#include <llvm/Support/CommandLine.h>
#include <llvm/Transforms/IPO/PassManagerBuilder.h>
#include <llvm/Transforms/Utils/ValueMapper.h>

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

/// Exit code when we want to successfully exit.
int APEX_DONE = 0;

/// Function LLVMNodes connected via the data dependencies.
using DependencyBlock = std::vector<LLVMNode *>;

/// Protected functions IDs. These will not be removed by APEXPass.
std::vector<std::string> PROTECTED_FCNS = {
    // These belong to the _apex_exit function.
    "_apex_exit",
    "exit",

    // These belong to the _apex_extract_int function.
    "_apex_extract_int",
    "abs",
    "log10",
    "floor",
    "fopen",
    "fputs",
    "fclose",
    "exit",
    "sprintf",

    // Keep printf calls.
    "printf",

    // Introduced with debug symbols.
    "llvm.dbg.declare",
    "llvm.stackrestore",
    "llvm.stacksave",
};

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
  // Target instructions that correspond to the user input.
  std::vector<const Instruction *> target_instructions_;

  // Logging utilities.
  void logPrint(const std::string &message);
  void logPrintUnderline(const std::string &message);
  void logPrintFlat(const std::string &message);
  void logDumpModule(const Module &M);

  // Function utilities.
  bool functionIsProtected(const Function *F);

  // dg utilities.
  void dgInit(Module &M, LLVMDependenceGraph &dg);

  // apex dg utilities.
  void apexDgInit(APEXDependencyGraph &apex_dg);
  void apexDgGetBlockNodeInfo(APEXDependencyNode &apex_node, LLVMNode *node);
  void apexDgPrintDataDependenciesCompact(APEXDependencyGraph &apex_dg);

  void apexDgFindDataDependencies(
      APEXDependencyGraph &apex_dg, LLVMNode &node,
      std::vector<LLVMNode *> &dependencies,
      std::vector<LLVMNode *> &rev_data_dependencies) const;
  void apexDgGetNodeOrDie(const APEXDependencyGraph &apex_dg,
                          const Instruction *const I, APEXDependencyNode &node);
  void apexDgComputeFunctionDependencyBlocks(
      const Module &M, APEXDependencyGraph &apex_dg,
      std::map<const Function *, std::vector<std::vector<LLVMNode *>>>
          &function_dependency_blocks);
  void apexDgPrintFunctionDependencyBlocks(
      const std::map<const Function *, std::vector<std::vector<LLVMNode *>>>
          &function_dependency_blocks);
  void apexDgConstructBlocksFunctionsCallgraph(
      const std::map<const Function *, std::vector<std::vector<LLVMNode *>>>
          &function_dependency_blocks,
      std::map<std::vector<LLVMNode *>, std::vector<const Function *>>
          &blocks_functions_callgraph);
  void apexDgPrintBlocksFunctionsCallgraph(
      const std::map<std::vector<LLVMNode *>, std::vector<const Function *>>
          &blocks_functions_callgraph);

  // Other utilities.
  void moduleParseCmdLineArgsOrDie(void);
  void moduleFindTargetInstructionsOrDie(Module &M, const std::string &file,
                                         const std::string &line);
  void moduleInjectExitExtract(Module &M,
                               const std::string &target_function_id);

  void findPath(Module &M,
                std::map<DependencyBlock, std::vector<const Function *>>
                    &blocks_functions_callgraph,
                std::map<const Function *, std::vector<DependencyBlock>>
                    &function_dependency_blocks,
                const std::string &source_function_id,
                const std::string &target_function_id,
                std::vector<DependencyBlock> &path);
  void printPath(const std::vector<DependencyBlock> &path);
  void
  removeUnneededStuff(const Module &M, const std::vector<DependencyBlock> &path,
                      std::map<DependencyBlock, std::vector<const Function *>>
                          &blocks_functions_callgraph,
                      std::map<const Function *, std::vector<DependencyBlock>>
                          &function_dependency_blocks);
  void stripAllDebugSymbols(Module &M);
};

/// Registering our own pass, so it can be ran via opt.
char APEXPass::ID = 0;
static RegisterPass<APEXPass> X("apex", "Active code Path EXtractor.",
                                false /* Only looks at CFG */,
                                false /* Analysis Pass */);