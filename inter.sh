# llvm-compiled gs executable file path
GS_EXE=

CLANG=clang
LLVM-DIS=llvm-dis
OPT=opt
LLC=llc

rm -rf inter
mkdir inter && cd inter

if [ $# -eq 0 ]; then
    cmake -DINTER_ANALYSIS=ON -DGS_ONLY .. && make
    $OPT -load ./src/DFGPass.so -DFGPass $GS_EXE.opt.bc -o $GS_EXE.final.bc
    # generate result
    $OPT -load ./src/DFGPass.so -DFGPass $PREFIX.opt.bc -o $PREFIX.final.bc
else
    PREFIX=$1
    cmake -DINTER_ANALYSIS=ON .. && make

    # compile
    $CLANG -g -c -emit-llvm -O1 -o $PREFIX.bc $PREFIX.cpp
    $OPT -loop-simplify -mergereturn $PREFIX.bc -o $PREFIX.opt.bc

    # generate result
    $OPT -load ./src/DFGPass.so -DFGPass $PREFIX.opt.bc -o $PREFIX.final.bc
fi