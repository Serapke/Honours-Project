# LLVM Translation Library

Uses Bigtable as main memory.

Translates load and store instructions to get and put instructions on Bigtable.

Library consists of two passes:<br/>
- heap-translation. 
    Translates only heap load and store instructions in user.cpp file.
- full-translation. 
    Translates all (both heap and stack) load and store instructions in user.cpp file.

Build:

    $ cd honours-project
    $ mkdir build
    $ cd build
    $ cmake ..
    $ make
    $ cd ..

Different .sh files provide a simple way to instrument the code and make an executable `run`.
Script files starting with `debug` builds an executable in debug mode.


Instrument:

    $ ./debug-heap.sh
            or
    $ ./debug-full.sh 
            or  
    $ ./build-heap.sh
            or
    $ ./build-full.sh  


Run:

    $ ./run

    
