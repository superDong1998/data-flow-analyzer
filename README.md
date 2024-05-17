# data-flow-analyzer
Analyze data flow graph for C/C++/Fortran (maybe) programs.

# Environment
Please make sure you have ```gcc```, ```python```, ```cmake```, and ```LLVM==10.0.0```

# Build app with LLVM
you can use provided ```Makefile``` at ```app_Makefile/``` directory, and please modify the target executable name at line 191
you can find the default as follow:
```
	$(LD) $(OBJFILES) -o Geodynamics_cpu.mpi $(FFTLIB)  $(LIB)
```

# Analysis app and example/test 

there are four supported modules: generate CFG/DFG, inter loop analysis and intra loop analysis, thus there are four scripts you can use: ```cfg.sh```, ```dfg.sh```, ```inter.sh``` and ```intra.sh```.

you can use all  scripts with correct configures of above envs to run and analyze.

To be specific:
   the LLVM compiler location and the compiled app executable file(```GS_EXE```) should be changed correctly
   for inter/intra anlysis, the python env should be checked

## Analysis App
### CFG
```
./cfg.sh
```
and you will find a pdf file named ```cfg.pdf``` in this directory

### DFG
```
./dfg.sh
```
and you will find a pdf file named ```dfg.pdf``` in this directory
### inter analysis
```
./inter.sh
```
and you will get the result at the standard output

### intra analysis
```
./intra.sh
```
and you will get the generated dependence graphs named ```loop1.png``` as so on.

## Analysis example/test
for cfg, dfg, and inter analysis, you only need to add the source code location without the file postfix, such as:
```
./cfg.sh ~/data-flow-analyzer/example/test/00test
```

for intra analysis, you need an extra parameter for the generated png name without the postfix, such as:
```
./intra.sh ~/data-flow-analyzer/example/test/00test 00test
```
and you can find ```00test.png``` in this directory
