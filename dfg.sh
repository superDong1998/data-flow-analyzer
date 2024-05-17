# llvm-compiled gs executable file path
GS_EXE=

CLANG=clang
LLVM-DIS=llvm-dis
OPT=opt
LLC=llc

rm -rf dfg
mkdir dfg && cd dfg

if [ $# -eq 0 ]; then
# try to generate gs function dfg
    cmake -DDFG=ON -DGS_ONLY=ON .. && make
    $OPT -loop-simplify -mergereturn $GS_EXE.bc -o $GS_EXE.opt.bc

    $OPT -load ./src/DFGPass.so -DFGPass $GS_EXE.opt.bc -o $GS_EXE.final.bc

    cd ..
    dot dfg/0.dot -Tpdf -o dfg.pdf
else
    PREFIX=$1

    cmake -DDFG=ON .. && make

    # compile
    $CLANG -g -c -emit-llvm -O1 -o $PREFIX.bc $PREFIX.cpp
    $OPT -loop-simplify -mergereturn $PREFIX.bc -o $PREFIX.opt.bc

    $OPT -load ./src/DFGPass.so -DFGPass $PREFIX.opt.bc -o $PREFIX.final.bc

    cd ..
    dot dfg/0.dot -Tpdf -o dfg.pdf
fi

