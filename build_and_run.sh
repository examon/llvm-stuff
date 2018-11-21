#!/bin/bash

# Created by Tomas Meszaros (exo at tty dot com, tmeszaro at redhat dot com)
#
# Published under Apache 2.0 license.
# See LICENSE for details.

# What is this:
# Script for building APEX as LLVM pass and then generating bytecode from
# input before and after running APEX (+running code formatting,
# generating images, etc.)


# TODO: This should be part of user input and not hardcoded.
INPUT_LIB="c-code/lib.c"
INPUT_SOURCE="c-code/test_dependencies_minimal.c"
FILE="c-code/test_dependencies_minimal.c"
LINE=27
#INPUT_SOURCE="c-code/callgraph.c"
#FILE="c-code/callgraph.c"
#LINE=11

# Generated bytecodes during all stages of processing.
BYTECODE_FROM_INPUT="bytecode_from_input.bc" # No optimizations.
#BYTECODE_FROM_INPUT_BASIC_OPTS="bytecode_from_input_basic_opts.bc" # Some basic optimizations (see BASIC_OPTS).
BYTECODE_FROM_LIB="bytecode_from_lib.bc" # Compiled libraries from ${INPUT_LIB}
BYTECODE_FROM_LINKED_INPUT_LIB="bytecode_from_linked.bc" # Linked input and lib bytecode. APEXPass will work with this.
LINKED_DISASSEMBLY="bytecode_from_linked.ll"
BYTECODE_FROM_APEX="bytecode_from_apex.bc" # APEXPass bytecode.

# Optimizations that are ran before APEXPass
BASIC_OPTS="-dce -loop-simplify -simplifycfg"


# =============================================================================
# "So it begins..." ===========================================================

echo "========= Building dg."

# Build dg
cd dg
rm -rf build
mkdir build
cd build
cmake ..
make -j9 # compiling really fast here dude
cd ../..

echo "========= Building apex."

# Build APEX
rm -rf build
mkdir build
cd build
#cmake .. -LAH # verbose
cmake ..
make -j9
cd ..


# =============================================================================
# Running code formatter ======================================================

echo "========= Running clang-format."

clang-format -style=llvm -i apex/*.cpp apex/*.h


# =============================================================================
# Compile to bytecode and run basic opts and than our opt =====================

echo "========= Compiling input."

# compile external libraries
clang -O0 -lm -g -c -emit-llvm ${INPUT_LIB} -o build/${BYTECODE_FROM_LIB}

# compile without anything
clang -O0 -g -c -emit-llvm ${INPUT_SOURCE} -o build/${BYTECODE_FROM_INPUT}

llvm-link build/${BYTECODE_FROM_LIB} build/${BYTECODE_FROM_INPUT} -S -o=build/${LINKED_DISASSEMBLY}
llvm-as build/${LINKED_DISASSEMBLY} -o build/${BYTECODE_FROM_LINKED_INPUT_LIB}

echo "========= Running APEXPass."

# run some basic optimizations, so the IR is cleaner
#opt -o build/${BYTECODE_FROM_INPUT_BASIC_OPTS} ${BASIC_OPTS} build/${BYTECODE_FROM_INPUT} > /dev/null

# run APEXPass without additional opts
opt -o build/${BYTECODE_FROM_APEX} -load build/apex/libAPEXPass.so -apex -file=${FILE} -line=${LINE} < build/${BYTECODE_FROM_LINKED_INPUT_LIB}  > /dev/null
# run APEXPass with ${BASIC_OPTS}
#opt -o build/${BYTECODE_FROM_APEX} -load build/apex/libAPEXPass.so -apex ${BASIC_OPTS} -source=${SOURCE_FCN} -target=${TARGET_FCN} < build/${BYTECODE_FROM_LINKED_INPUT_LIB}  > /dev/null


# ==============================================================================
# Generate callgraphs before and after =========================================

echo "========= Exporting callgraphs."

# run callgraph exporter
cd build
rm -rf callgraphs
mkdir callgraphs
cd callgraphs

opt -dot-callgraph ../${BYTECODE_FROM_INPUT} > /dev/null
mv callgraph.dot callgraph_no_opt.dot
dot callgraph_no_opt.dot -Tsvg -O

#opt -dot-callgraph ../${BYTECODE_FROM_INPUT_BASIC_OPTS} > /dev/null
#mv callgraph.dot callgraph_default_opt.dot
#dot callgraph_default_opt.dot -Tsvg -O

opt -dot-callgraph ../${BYTECODE_FROM_APEX} > /dev/null
mv callgraph.dot callgraph_apex_opt.dot
dot callgraph_apex_opt.dot -Tsvg -O

cd ../..


# ==============================================================================
# DG tools output ==============================================================

echo "========= Running dg tools."

cd build
rm -rf dg_tools
mkdir dg_tools
cd dg_tools


#../../dg/build/tools/llvm-dg-dump ../${BYTECODE_FROM_INPUT} > llvm_dg_dump_no_opt.dot
#dot llvm_dg_dump_no_opt.dot -Tpdf -O

../../dg/build/tools/llvm-dg-dump -no-control ../${BYTECODE_FROM_INPUT} > llvm_dg_dump_no_control_no_opt.dot
dot llvm_dg_dump_no_control_no_opt.dot -Tpdf -O

#../../dg/build/tools/llvm-dg-dump -no-data ../${BYTECODE_FROM_INPUT} > llvm_dg_dump_no_data_no_opt.dot
#dot llvm_dg_dump_no_data_no_opt.dot -Tpdf -O

#../../dg/build/tools/llvm-dg-dump -bb-only ../${BYTECODE_FROM_INPUT} > llvm_dg_dump_bb_only_no_opt.dot
#dot llvm_dg_dump_bb_only_no_opt.dot -Tpdf -O

#../../dg/build/tools/llvm-ps-dump -dot ../${BYTECODE_FROM_INPUT} > llvm_ps_dump_no_opt.dot
#dot llvm_ps_dump_no_opt.dot -Tsvgz -O

#../../dg/build/tools/llvm-rd-dump -dot ../${BYTECODE_FROM_INPUT} > llvm_rd_dump_no_opt.dot
#dot llvm_rd_dump_no_opt.dot -Tpdf -O

cd ../..


# ==============================================================================
# Disassemble bytecode so We can look inside ===================================

echo "========= Disassembling llvm bytecode."

llvm-dis build/${BYTECODE_FROM_INPUT}
#llvm-dis build/${BYTECODE_FROM_INPUT_BASIC_OPTS}
llvm-dis build/${BYTECODE_FROM_APEX}
llvm-dis build/${BYTECODE_FROM_LIB}


# ==============================================================================
# Run generated bytecodes ======================================================

# run bytecode after basic opts
echo "========= Running ${BYTECODE_FROM_INPUT}"
lli build/${BYTECODE_FROM_INPUT}

# run bytecode after basic opts
#echo "========= Running ${BYTECODE_FROM_INPUT_BASIC_OPTS}"
#lli build/${BYTECODE_FROM_INPUT_BASIC_OPTS}

# run pass and execute modified bytecode
echo "========= Running ${BYTECODE_FROM_APEX}"
lli build/${BYTECODE_FROM_APEX}
