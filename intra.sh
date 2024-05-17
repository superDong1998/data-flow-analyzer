#!/bin/bash
# llvm-compiled gs executable file path
GS_LOOP1_EXE=
GS_LOOP2_EXE=
GS_LOOP3_EXE=
GS_LOOP4_EXE=
GS_LOOP5_EXE=
GS_LOOP6_EXE=
GS_LOOP7_EXE=
GS_LOOP8_EXE=
GS_LOOP9_EXE=

CLANG=clang
LLVM-DIS=llvm-dis
OPT=opt
LLC=llc

#prepare python env
spack load python@3.9.12%gcc@=11.3.0
source ~/frontend-venv/bin/activate
PYTHON=python


rm -rf intra
mkdir intra && cd intra

GS_LOOP_EXE=(\
    "$GS_LOOP1_EXE" \
    "$GS_LOOP2_EXE" \
    "$GS_LOOP3_EXE" \
    "$GS_LOOP4_EXE" \
    "$GS_LOOP5_EXE" \
    "$GS_LOOP6_EXE" \
    "$GS_LOOP7_EXE" \
    "$GS_LOOP8_EXE" \
    "$GS_LOOP9_EXE" \
)

if [ $# -eq 0 ]; then
    cmake -DINTRA_ANALYSIS=ON -DGS_ONLY .. && make
    
    for i in ${!GS_LOOP_EXE[@]}; do
        loop_num=$((i + 1))
        GS_LOOP_EXE_VAR=${GS_LOOP_EXE[$i]}
        $OPT -loop-simplify -mergereturn $GS_LOOP_EXE_VAR.bc -o $GS_LOOP_EXE_VAR.opt.bc

        # generate result
        output=$($OPT -load ./src/DFGPass.so -DFGPass $GS_LOOP_EXE_VAR.opt.bc -o $GS_LOOP_EXE_VAR.final.bc)
        cleaned_output=$(echo "$output" | tr -d '\n' | sed 's/,$//')

        $PYTHON draw.py "$cleaned_output" loop_$loop_num
    done
else
    PREFIX=$1
    graph=$2
    cmake -DINTRA_ANALYSIS=ON .. && make

    # compile
    $CLANG -g -c -emit-llvm -O1 -o $PREFIX.bc $PREFIX.cpp
    $OPT -loop-simplify -mergereturn $PREFIX.bc -o $PREFIX.opt.bc

    # generate result
    output=$($OPT -load ./src/DFGPass.so -DFGPass $PREFIX.opt.bc -o $PREFIX.final.bc)
    cleaned_output=$(echo "$output" | tr -d '\n' | sed 's/,$//')

    $PYTHON draw.py "$cleaned_output" $graph
fi
