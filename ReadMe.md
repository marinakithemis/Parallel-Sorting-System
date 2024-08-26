
# Parallel-Sorting-System

This program implements a parallel sorting system using a master-worker architecture. 
The mysort.c file acts as the coordinator, managing child processes that perform sorting and merging tasks. It divides the data file into segments, 
spawns merger processes to handle sorting using the sorting1 (QuickSort) and sorting2 (MergeSort) executables, and then merges the results. 
The sorting1.c and sorting2.c files define the sorting algorithms, each working on a segment of data and communicating results back to the coordinator. 
The system effectively parallelizes sorting and merging to handle large datasets efficiently.

## How to Compile and Run

- Use the command `make` to build the executable files.
- Use the command `make clean` to remove the object files.
- Use the command `make valgrind` to run the program with Valgrind using default values:
  ```bash
  ./mysort -i testing_files/voters100000.bin -k 6 -e1 ./sorting1 -e2 ./sorting2
  ```
- To run the program without Valgrind, use:
  ```bash
  ./mysort -i <DataFile> -k <NumOfChildren> -e1 ./sorting1 -e2 ./sorting2
  ```

## `mysort.c` File

- This file contains the implementation of the coordinator and the splitters/mergers.
- After verifying that the user provides the correct arguments, the first task is to calculate the size of the file to determine the number of entries. It is assumed that each entry is 52 bytes.
- Next, dynamic memory allocation is performed for the required arrays (described in comments). The main processing loop is then entered.
- The loop iterates as many times as needed to create the child processes (mergers) using `fork()`.
- Concurrently, a pipe (`fd1`) is opened to facilitate communication between the root and each merger, and the segment of the array for each merger is calculated.
- If `pid == 0`, we are in a merger process. A second loop is started within which the merger's children (sorters) are created using `fork()`.
- Another pipe (`fd`) is opened for communication between the sorters and their merger.
- The write end of this pipe, as well as other variables, is converted to a string format to pass them as arguments to `execlp`.
- If `pid2 == 0`, we are in a sorter process. Depending on this, `execlp` is used to call either `sorting1` or `sorting2` (alternating for each child of the merger).
- If `pid2 != 0`, we are in a merger process. The merger reads entry structs written by the sorter from the pipe and stores them in an array.
- After reading all entry structs and the timer struct (which records the time taken by the sorter), the merger performs the merge operation on the sorted segments.
- The merge is performed in pairs: the results of the first two children are merged first, and then this result is merged with the sorted array of the next child.
- When the merging is complete, the merger writes each entry struct and timer struct to `fd1` until all entries assigned to it are sorted.
- If `pid != 0`, we are at the root level and read each entry and timer struct written by the mergers.
- After exiting the first loop, we have the sorted arrays from each merger. The same merging process is applied to combine them until we obtain the final sorted array with all records.

## `sorting1.c` and `sorting2.c` Files

- The files implement the sorting algorithms QuickSort (`sorting1.c`) and MergeSort (`sorting2.c`), respectively.
- The structure of these files is similar: each implements the sorting algorithm and includes a `main` function.
- The `main` function accepts arguments: the name of the binary file with entries, the number of entries calculated by the root, the sorting range for the sorter, the write end of the pipe (`fd`), and the PID of the root.
- After opening the file and reading all entries into an array, a second array is created containing the segment of the initial array to be sorted based on the provided range.
- The sorting algorithm (QuickSort/MergeSort) is then called, and the results are written to the pipe (struct by struct). Timing is also measured, and a timer struct is written to the pipe.
- Finally, a `SIGUSR2` signal is sent to the root to notify it that the sorter has completed its task.

## Header Files

- Two header files are used: one containing the time and entry structs (`struct.h`) and another with function declarations for `mysort` (`header.h`).
- The maximum string size is considered to be 100 characters.

