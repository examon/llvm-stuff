/* Created by Tomas Meszaros (exo at tty dot com, tmeszaro at redhat dot com)
 *
 * Published under Apache 2.0 license.
 * See LICENSE for details.
 */

// What is this:
// This is header file for apex.cpp

#pragma once

#include <string>
#include <vector>

#include <llvm/Analysis/CallGraph.h>
#include <llvm/IR/LegacyPassManager.h>
#include <llvm/Transforms/IPO/PassManagerBuilder.h>

// We need this for dg integration.
#include "analysis/PointsTo/PointsToFlowInsensitive.h"
#include "llvm/LLVMDependenceGraph.h"
#include "llvm/analysis/DefUse.h"

using namespace llvm;
using namespace dg;

bool _LOG_VERBOSE = true;

// This is user input. TODO: move this out
std::string _SOURCE = "main";
std::string _TARGET = "y";


struct APEXDependencyNode {
    /* Node is usually line instruction of IR. Sometimes whole function. */
    Value *value;
    std::vector<LLVMNode *> control_depenencies;
    std::vector<LLVMNode *> rev_control_depenencies;
    std::vector<LLVMNode *> data_dependencies;
    std::vector<LLVMNode *> rev_data_dependencies;
};


struct APEXDependencyFunction {
    /* Function (can contain multiple basic blocks). */
    Value *value;
    std::vector<APEXDependencyNode> nodes;
};


struct APEXDependencyGraph {
    /* Graph: consists of nodes that are functions. */
    std::vector<APEXDependencyFunction> functions;
};


class APEXPass : public ModulePass {
    /* Actual APEX pass. */
public:
    static char ID;
    APEXPass() : ModulePass(ID) {}
    bool runOnModule(Module &M) override;

private:
    /* Logging utilities. */
    void logPrint(const std::string &message);
    void logPrintUnderline(const std::string &message);
    void logPrintFlat(const std::string &message);
    void logDumpModule(const Module &M);

    /* Function utilities. */
    void functionVectorFlatPrint(const std::vector<Function *> &functions);
    int functionRemoveCalls(const Function *F);
    int functionRemove(Function *F);
    int functionGetCallers(const Function *F, std::vector<Function *> &callers);
    int functionGetCallees(const Function *F, std::vector<Function *> &callees);

    /* Callgraph utilities. */
    int createCallGraph(
            const Module &M, const std::string &root,
            std::vector<std::pair<Function *, std::vector<Function *>>> &callgraph);
    void printCallGraph(
            const std::vector<std::pair<Function *, std::vector<Function *>>>
            &callgraph);
    int findPath(const std::vector<std::pair<Function *, std::vector<Function *>>>
                 &callgraph,
                 const std::string &start, const std::string &end,
                 std::vector<Function *> &final_path);
    void printPath(const std::vector<Function *> &path);

    /* dg utilities. */
    void dgInit(Module &M, LLVMDependenceGraph &dg);

    /* apex dg utilities. */
    void apexDgInit(APEXDependencyGraph &apex_dg);
    void apexDgGetBlockNodeInfo(APEXDependencyNode &apex_node, LLVMNode *node);
    void apexDgPrint(APEXDependencyGraph &dg, bool verbose);
};


/* Registering our own pass, so it can be ran via opt.
 */
char APEXPass::ID = 0;
static RegisterPass<APEXPass> X("apex", "Just a test pass. Work in progress.",
                                false /* Only looks at CFG */,
                                false /* Analysis Pass */);