# Compile sources to LLVM bytecode

clang++ -std=c++11 -S -emit-llvm functions.cpp -I ~/grpc/googleapis/gens -DDEBUG=false
clang++ -std=c++11 -S -emit-llvm user.cpp
clang++ -std=c++11 -S -emit-llvm heap_alloc.cpp -DDEBUG=false

# Link to a single bytecode file

~/build/bin/llvm-link -S -o single.ll user.ll functions.ll heap_alloc.ll

~/build/bin/opt -load build/passes/libTranslationPass.so -full-translation single.ll -S -o single-optimised.ll

~/build/bin/llc single-optimised.ll

clang++ -std=c++11 -pthread -L/usr/local/lib -lgrpc -lgrpc++ -lgrpc++_cronet -lprotobuf -I ~/grpc/include single-optimised.s ~/grpc/googleapis/gens/*.o -o run

