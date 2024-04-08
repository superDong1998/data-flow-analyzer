DFG_DIR=/home/jinyuyang/PACMAN_PROJECT/huawei21/data-flow-analyzer

LLVM_VERSION=                                                                                                                                                
                                                                                                                                                                  
CLANG=clang$LLVM_VERSION                                                                                                                                          
LLVM_DIS=llvm-dis$LLVM_VERSION                                                                                                                                    
OPT=opt$LLVM_VERSION                                                                                                                                              
LLC=llc$LLVM_VERSION                                                                                                                                              
#CC=mpicxx                                                                                                                                                         

PREFIX=wrf_new
                                                                                                                                                          
# $CLANG -g -c -emit-llvm -O1 -o $PREFIX.bc $DFG_DIR/example/test/$PREFIX.cpp                                                                                                                  
# $LLVM_DIS $PREFIX.bc                                                                                                                                                 
                                                                                                                                                                  
# $OPT -loop-simplify -mergereturn $PREFIX.bc -o $PREFIX.opt.bc                                                                                                         
# $LLVM_DIS $PREFIX.opt.bc                                                                                                                                           
                                                                                                                                                                  
# $OPT -load ./src/DFGPass.so -DFGPass $PREFIX.opt.bc -enable-new-pm=0 -o $PREFIX.final.bc                                                                            
$OPT -load ./src/DFGPass.so -DFGPass $DFG_DIR/example/wrf/$PREFIX.bc -o $PREFIX.final.bc                                                                            
# $LLVM_DIS $PREFIX.final.bc   