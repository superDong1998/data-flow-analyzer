# llvm-compiled gs executable file path
GS_EXE=~/analysis/app/geodynamics-loop/src_cpu/Geodynamics_cpu.loop8

CLANG=clang
LLVM_DIS=llvm-dis
OPT=opt
LLC=llc

rm -rf inter
mkdir inter && cd inter

if [ $# -eq 0 ]; then
    cmake -DINTER_ANALYSIS=ON -DGS_ONLY=ON .. && make
    $OPT -loop-simplify -mergereturn $GS_EXE.bc -o $GS_EXE.opt.bc
    $OPT -load ./src/DFGPass.so -DFGPass $GS_EXE.opt.bc -o $GS_EXE.final.bc
else
    PREFIX=$1
    cmake -DINTER_ANALYSIS=ON .. && make

    # compile
    $CLANG -g -c -emit-llvm -O1 -o $PREFIX.bc $PREFIX.cpp
    $OPT -loop-simplify -mergereturn $PREFIX.bc -o $PREFIX.opt.bc

    # generate result
    $OPT -load ./src/DFGPass.so -DFGPass $PREFIX.opt.bc -o $PREFIX.final.bc
fi