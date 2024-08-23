# GraphingCalc
 The ti84 @ home that mom always talks about, free for all.
 
 ![CalcCalc GUI](CalcCalc.png)
 
# Build
gcc -o CalcCalc calcCalc.c `pkg-config --cflags --libs gtk+-3.0` -lm

