# HEFT and CPOP Simulator

A simulator that takes in directed acyclic graph representation of an application and simulates HEFT or CPOP heterogeneous task scheduling algorithms

## Compilation instructions

- This program requires `g++` compiler with `C++ 11` support
- To compile the program, run `g++ -std=c++11 -o main main.cpp`

## Running instructions

1. Compile the program based on the instructions above
2. Run the program in the following format: `./main <ALGORITHM ID> <INPUT FILE PATH>`, where `<ALGORITHM ID>` is `1 (HEFT)` or `1 (CPOP)`; *IMPORTANT*: input files must have `.in` extension
3. The output file will be generated in the same directory where the input file is located with `--heft.out` or `--cpop.out` extension, depending on the algorithm

## Input file format

- Input file must have `.in` extension
- The input file describes the application DAG, processor count, execution cost per processor, communication costs, transfer rates between processors
- The DAG vertices in the input file are indexed from `1`
- The processors in the input file are indexed from `1`
- DAG must have one input task and one exit task
- The input file must follow the following structure:
```
- first line, first number is V task count (vertex in DAG count)
- second number is E edge count (all pairs of edges in the DAG)
- third number is P processor count
- the following E lines are: <from> <to> <weight>
  - these E lines stand for the edge pairs in the graph and the communication cost between them (in bytes)
- the following V lines are of the format: <p_1> ... <p_P> (from 1 to P)
  - p_i stands for computation cost of task j on processor i
- the following (p^2 - p) / 2 lines stand are of the format: <from> <to> <rate>
  - each line describes the data transfer rate between processor <from> and <to> with rate being bytes/second
```
- Example input file:
```
4
4
3

1 2 10
1 3 11
2 4 8
3 4 12

3 4 1
8 10 9
7 6 3
1 1 2

1 2 4
2 3 3
1 3 4
```

## Output file format

- The program will generate the output files with `--heft.out` or `--cpop.out` extensions appended to the input file path
- For example, if one runs the program with arguments `./main 1 ./example.in`, the output will be generated in the file `./example--heft.out`
- The output file will include the start and end times of every scheduled task, as well as the processor load and the time taken to execute the application (maximum finish time)

## Additional scripts

- There is also a `shell` script `run.sh` that will compile and execute the program in a given directory
- For example, if there is a directory `./experiments` that contains multiple `.in` properly configured input files, one can run the shell script as follows: `./run.sh <ALGORITHM ID> <TARGET DIRECTORY>`
- The script will compile the binary of the program and run the executable with every input file in the target directory
