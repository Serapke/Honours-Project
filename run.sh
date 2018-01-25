cd build
make
cd ..

rm *.ll
rm *.s

clang -S -emit-llvm *.c
../ug3-ct/build/bin/llvm-link -S -v -o single.ll *.ll
../ug3-ct/build/bin/opt -load build/skeleton/libSkeletonPass.so -skeletonpass single.ll -o optimised.ll
../ug3-ct/build/bin/llc optimised.ll
clang optimised.s