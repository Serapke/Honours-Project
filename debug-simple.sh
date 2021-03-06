# Compile sources to LLVM bytecode

clang++ -std=c++11 -S -emit-llvm functions.cpp -I ~/grpc/googleapis/gens -DDEBUG=true
clang++ -std=c++11 -S -emit-llvm user.cpp
clang++ -std=c++11 -S -emit-llvm memops.cpp
clang++ -std=c++11 -S -emit-llvm heap-alloc-simple.cpp -DDEBUG=true

# Link to a single bytecode file

# ~/build/bin/llvm-link -S -v -o single.ll memops.ll user.ll functions.ll heap_alloc_simple.ll

~/build/bin/llvm-link -S -v -o single.ll user.ll functions.ll heap-alloc-simple.ll

~/build/bin/opt -load build/passes/libTranslationPass.so -full-translation single.ll -S -o single-optimised.ll

~/build/bin/llc single-optimised.ll

clang++ -std=c++11 -pthread -L/usr/local/lib -lgrpc -lgrpc++ -lgrpc++_cronet -lprotobuf -I ~/grpc/include single-optimised.s ~/grpc/googleapis/gens/*.o -o run
