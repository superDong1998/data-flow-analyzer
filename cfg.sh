# llvm-compiled gs executable file path
GS_EXE=~/analysis/app/geodynamics-loop/src_cpu/Geodynamics_cpu.loop8

CLANG=clang
LLVM_DIS=llvm-dis
OPT=opt
LLC=llc

rm -rf cfg
mkdir cfg && cd cfg

if [ $# -eq 0 ]; then
# try to generate gs function cfg
    cmake -DCFG=ON -DGS_ONLY=ON .. && make
    $OPT -loop-simplify -mergereturn $GS_EXE.bc -o $GS_EXE.opt.bc

    $OPT -load ./src/DFGPass.so -DFGPass $GS_EXE.opt.bc -o $GS_EXE.final.bc

    cd ..
    dot cfg/0.dot -Tpdf -o cfg.pdf
else
    PREFIX=$1

    cmake -DCFG=ON .. && make

    # compile
    $CLANG -g -c -emit-llvm -O1 -o $PREFIX.bc $PREFIX.cpp
    $OPT -loop-simplify -mergereturn $PREFIX.bc -o $PREFIX.opt.bc

    $OPT -load ./src/DFGPass.so -DFGPass $PREFIX.opt.bc -o $PREFIX.final.bc

    cd ..
    dot cfg/0.dot -Tpdf -o cfg.pdf
fi

