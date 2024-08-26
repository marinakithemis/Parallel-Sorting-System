#include <stdio.h>
#include <math.h>
#include <stdbool.h>
#include <stdlib.h>
#include <string.h>
#include <ctype.h>
#include <unistd.h>
#include <fcntl.h>
#include <sys/stat.h>
#include <sys/wait.h>
#include <signal.h>
#include "../include/structs.h"
#include "../include/header.h"

int count_sig1 = 0;    //counts the SIGUSR1 signals we receive
int count_sig2 = 0;    //counts the SIGUSR2 signals we receive 

void signal1_handler(int signal)      //signal handler for SIGUSR1 signal, this signal comes when a merger has finished its job
{
    if(signal == SIGUSR1){
        count_sig1 += 1;
    }
}

void signal2_handler(int signal)     //signal handler for SIGUSR2  signal, this signal comes when a sorter has finished its job
{
    if(signal == SIGUSR2){
        count_sig2 += 1;
    }
}

int main(int argc, char* argv[])
{    
    struct stat st;   //we need this to find the size of the file
    int NumofChildren;   //number of mergers
    char *DataFile, *sorting1, *sorting2;

    if (argc != 9) error_exit("Wrong number of arguments\n");

    ////check the flags in the arguments////
    for(int i = 1; i < argc ; i+=2){
        if(strcmp(argv[i], "-k") == 0){
            if(i+1 < argc){
                if(is_integer(argv[i+1])){       //we have to check if after flag -k we have an integer 
                    NumofChildren = atoi(argv[i+1]);
                }
                else  error_exit("Wrong arguments\n");
            }
        }
        else if(strcmp(argv[i], "-i") == 0){
            if(i + 1 < argc){
                DataFile = argv[i+1];
            }
        }
        else if(strcmp(argv[i], "-e1") == 0){
            if(i + 1 < argc){
                sorting1 = argv[i+1];
            }
        }
        else if(strcmp(argv[i], "-e2") == 0){
            if(i + 1 < argc){
                sorting2 = argv[i+1];
            }
        }
        else error_exit("Wrong flags\n");
    }

    int fd = open(DataFile, O_RDONLY);

    if(fd == -1) error_exit("Error during file opening\n");
  
    stat(DataFile, &st);
    int size = st.st_size;    //size of file
    int num_of_entries = size / 52;   //each entry is 52 bytes, so dividing the size of the file with 52 we get the number of entries
    int root_pid = getpid();          //I need the root pid later in the program
    
    close(fd);

    //array where i store the sizes of the arrays that each merger sorts
    int *merger_sizes =(int*) calloc(NumofChildren,sizeof(int));

    //in the merger array i store the arrays that each merger sorted
    struct Entry** merger_array = malloc(NumofChildren*sizeof(struct Entry*));

    //contains the timers of each sorter of each merger
    struct Timer** final_timers = malloc(NumofChildren*sizeof(struct Timer*));

    if(merger_sizes == NULL || merger_array == NULL || final_timers == NULL) error_exit("Problem with memory allocation\n");
      
    for(int i = 0; i < NumofChildren; i++){
        int merger_from = ceil((num_of_entries/NumofChildren))*i;    //find the exact starting position
        int merger_to = merger_from + num_of_entries/NumofChildren;  //until which part of the array

        if(i == NumofChildren - 1){        //if the division is not exact, we leave the rest of the entries for the last merger to sort
            merger_to = num_of_entries;
        }

        merger_sizes[i] = merger_to - merger_from;
        merger_array[i] = malloc(merger_sizes[i]*sizeof(struct Entry));
        final_timers[i] = malloc((NumofChildren-i)*sizeof(struct Timer));

        if(merger_array[i] == NULL || final_timers[i] == NULL) error_exit("Problem with memory allocation\n");
    }

    //final array is the final sorted array that the root will print (contains the result of merging all the merger_arrays)
    struct Entry* final_array = malloc(num_of_entries*sizeof(struct Entry));

    //convert the root_pid and numofentries to string to pass it later as an argument in exec
    char* rootpid_str = malloc(MAX_SIZE*sizeof(char));
    char* num_of_entries_str = malloc(MAX_SIZE*sizeof(char));

    if(final_array == NULL || rootpid_str == NULL || num_of_entries_str == NULL) error_exit("Problem with memory allocation\n");

    snprintf(rootpid_str, MAX_SIZE, "%d", root_pid);
    snprintf(num_of_entries_str, MAX_SIZE, "%d", num_of_entries);

    for(int i = 0; i < NumofChildren; i++){   //coordinator/root

        signal(SIGUSR1, signal1_handler);
        int fd1[2];     //pipe to write in merger process and read from root process

        //pipe to write in merger/splitter processes and read in coordinator/root processes
        if(pipe(fd1) < 0) error_exit("Error during pipe\n");

        //we have to find the section of the array that the merger will give to the sorters to sort
        int merger_from = ceil((num_of_entries/NumofChildren))*i;    //find the exact starting position
        int merger_to = merger_from + num_of_entries/NumofChildren;  //until which part of the array

        //if the division is not exact, we leave the rest of the entries for the last merger to sort
        if(i == NumofChildren - 1) merger_to = num_of_entries;
        
        int pid = fork();
        signal(SIGUSR2, signal2_handler);

        if(pid == 0){     //we are in splitter/merger process

            //array where I store the timers of each sorter of a specific merger
            struct Timer* sorter_timers = malloc((NumofChildren-i)*sizeof(struct Timer)); 

            if(sorter_timers == NULL) error_exit("Error during memory allocation\n");

            close(fd1[0]);   //we don't need the read end of this pipe

            struct Entry** sorted_splitter_part = malloc((NumofChildren-i)*sizeof(struct Entry*));  //2d array to store later all the sorted arrays returned from the sorters
            int *sizes = (int*) calloc(NumofChildren-i,sizeof(int));    //i will store here the size of each subarray (each sorted array that a splitter returns)
            
            if(sorted_splitter_part == NULL || sizes == NULL) error_exit("Error during memory allocation\n");

            for(int j = 0; j < (NumofChildren - i); j++){

                int fd[2];       
                //string arrays to be able to give int arguments into the execlp syscall in string form
                char *from = malloc(MAX_SIZE*sizeof(char));     //from which part of the array will the sorter sort
                char *to = malloc(MAX_SIZE*sizeof(char));       //until which part of the array will the sorter sort
                char *fd_str = malloc(sizeof(int));

                if(from == NULL || to == NULL || fd_str == NULL) error_exit("Error during memory allocation\n");

                //we have to find which section of the array the sorter will sort
                int sorter_sorts = ceil((num_of_entries/NumofChildren)/(NumofChildren - i));   //number of voters that the sorter will sort
                int sorter_from = merger_from + j*sorter_sorts;
                int sorter_to = sorter_from + sorter_sorts;
                
                //if the division is not exact, we leave the rest of the entries for the last sorter 
                if(j == NumofChildren - i - 1) sorter_to = merger_to;
                
                sizes[j] = sorter_to - sorter_from;    //size of the array that the sorter j gets to sort
                sorted_splitter_part[j] = malloc(sizes[j]*sizeof(struct Entry));

                if(sorted_splitter_part[j] == NULL) error_exit("Error during memory allocation\n");

                //i have to transform the int variables to string form to be able to give them as arguments in exec
                snprintf(from, MAX_SIZE, "%d", sorter_from);
                snprintf(to, MAX_SIZE, "%d", sorter_to);

                //pipe to write in sorter processes and read in merger/splitter processes
                if(pipe(fd) < 0) error_exit("Error during pipe\n");
                
                snprintf(fd_str, 2, "%d", fd[1]);  //i'm gonna give the fd[1] as an argument to exec, so that the sorter will know where to write
                
                int pid2 = fork();

                if(pid2 == 0){     //we are in sorter process

                    close(fd[0]); //we don't need the read end

                    if(j % 2 == 0){
                        execlp(sorting1, sorting1, DataFile, num_of_entries_str, from, to, fd_str, rootpid_str, NULL);
                    }
                    else{
                        execlp(sorting2, sorting2, DataFile, num_of_entries_str, from, to, fd_str, rootpid_str, NULL);
                    }
    
                }
                else{
                    
                    free(from);
                    free(to);
                    free(fd_str);

                    close(fd[1]); //we don't need the write end
                    struct Entry* sorted_part = malloc((sorter_to-sorter_from)*sizeof(struct Entry));  //here I will store the part of the array that the specific sorter sorted

                    if(sorted_part == NULL) error_exit("Error during memory allocation\n");
                    
                    for(int i = 0; i < (sorter_to-sorter_from); i++){    //we read the entries one by one and store them into the sorted_part array
                        struct Entry buffer;
                        read(fd[0], &buffer, sizeof(buffer));
                        sorted_part[i] = buffer;
                    }
                    
                    //we also have to read how much time did it took the sorter to finish its job
                    struct Timer t;
                    read(fd[0], &t, sizeof(struct Timer));
                    close(fd[0]); 

                    sorter_timers[j] = t;      //store the time of the sorter in the sorter_timers array
                    array_copy(&sorted_splitter_part[j], &sorted_part, (sorter_to-sorter_from));  //store the sorted_part array of splitter j into the sorted_splitter_part array
                    free(sorted_part);    
                }
            }

            int sorter_num = NumofChildren - i;   //number of sorters
            
            //final_merger_array is the array that contains all the sorted entries that the merger had to sort
            struct Entry* final_merger_array = malloc((merger_to-merger_from)*sizeof(struct Entry));

            if(final_merger_array == NULL) error_exit("Error during memory allocation\n");

            if(sorter_num == 1){   //we already have a sorted array so we pass it to the coordinator through a pipe
                array_copy(&final_merger_array, &sorted_splitter_part[0], sizes[0]);
            }

            else{     //we have to merge the arrays
                
                if(sorter_num == 2){   //if we only have 2 sorters, we are done if we just merge them
                    merge(sorted_splitter_part[0], sorted_splitter_part[1], sizes[0], sizes[1], final_merger_array);
                }
                else{       //else we have to get the merged_array and merge it with the next array, take the result and merge it again with the next until we are done

                    int size = sizes[0] + sizes[1];  //current size of the final_merger_array
                    merge(sorted_splitter_part[0], sorted_splitter_part[1], sizes[0], sizes[1], final_merger_array);

                    for(int p = 2; p < sorter_num; p++){
                        struct Entry* final_merged = malloc((sizes[p]+size)*sizeof(struct Entry));

                        if(final_merged == NULL) error_exit("Error during memory allocation\n");

                        merge(sorted_splitter_part[p], final_merger_array, sizes[p], size, final_merged);
                        array_copy(&final_merger_array, &final_merged, (sizes[p]+size));
                        size += sizes[p];
                        free(final_merged);
                    }
                }
            }

            for(int k = 0; k < NumofChildren - i; k++) free(sorted_splitter_part[k]);
            
            free(sorted_splitter_part);
            free(sizes);
            
            //we send/write the final_merger_array to the root process, entry by entry
            for(int b = 0; b < (merger_to-merger_from); b++) write(fd1[1], &final_merger_array[b], sizeof(struct Entry));
            
            //we send/write the sorter_timers to the root process, timer by timer
            for(int b = 0; b < NumofChildren-i; b++) write(fd1[1], &sorter_timers[b], sizeof(struct Timer));
            
            free(final_merger_array);
            free(sorter_timers);

            //send signal to root that the merger has finished its job
            kill(getppid(), SIGUSR1);
            break;
        }

        else{   //we are in coordinator/root process and we read

            close(fd1[1]);
            struct Entry* array = malloc((merger_to-merger_from)*sizeof(struct Entry));   //array with the sorted entries of a splitter/merger

            if(array == NULL) error_exit("Error during memory allocation\n");

            for(int k = 0; k < (merger_to-merger_from); k++){
                struct Entry buffer;
                read(fd1[0], &buffer, sizeof(buffer));
                array[k] = buffer;
            }

            struct Timer* tim_array = malloc((NumofChildren-i)*sizeof(struct Timer));   //array with the sorter timers of a splitter/merger

            if(tim_array == NULL) error_exit("Error during memory allocation\n");

            for(int k = 0; k < NumofChildren-i; k++){
                struct Timer tim;
                read(fd1[0], &tim, sizeof(tim));
                tim_array[k] = tim;
            }
            
            close(fd1[0]);
            array_copy(&merger_array[i], &array, (merger_to-merger_from));
            array_copy2(&final_timers[i], &tim_array, (NumofChildren-i));
            free(array);
            free(tim_array);
        }
    }
    
    int paidi = getpid();
    if(paidi == root_pid){   //check if we are in the root process
        
        int merger_num = NumofChildren;
        
        if(merger_num == 1){   //we already have a sorted array so we pass it to the coordinator through a pipe
            array_copy(&final_array, &merger_array[0], merger_sizes[0]);
        }
        
        else{     //we have to merge the arrays
            //i do the same thing as before (when I merged the sorter arrays)
            if(merger_num == 2 ) merge(merger_array[0], merger_array[1], merger_sizes[0], merger_sizes[1], final_array);
            
            else{
                int size = merger_sizes[0] + merger_sizes[1];
                merge(merger_array[0], merger_array[1], merger_sizes[0], merger_sizes[1], final_array);
                for(int p = 2; p < merger_num; p++){
                    struct Entry *final_merged = malloc((merger_sizes[p]+size)*sizeof(struct Entry));
                    if(final_merged == NULL) error_exit("Error during memory allocation\n"); 
                    merge(merger_array[p], final_array, merger_sizes[p], size, final_merged);
                    array_copy(&final_array, &final_merged, (merger_sizes[p] + size));
                    size += merger_sizes[p];
                    free(final_merged);
                }
            }
        }
        
        //print the final array
        for(int b = 0; b < num_of_entries; b++){
            printf("%-12s %-12s %6d %s\n", final_array[b].lname, final_array[b].fname, final_array[b].AM, final_array[b].post);
            fflush(stdout);
        }

        //print the timers
        for(int i = 0; i < NumofChildren; i++){
            for(int j = 0; j < (NumofChildren-i); j++){
                printf("Merger %d sorter %d has CPU time: %lf and Wall time: %lf\n",i, j,  final_timers[i][j].cpu_time, final_timers[i][j].wall_time);
                fflush(stdout);
            }
        }
        //print the amount of signals the root received
        printf("USR1 signals: %d , USR2 signals: %d\n", count_sig1, count_sig2);
        fflush(stdout);
    }

    free(merger_sizes);
    free(final_array);
    free(num_of_entries_str);
    free(rootpid_str);
    
    for(int i = 0; i < NumofChildren; i++){
        free(merger_array[i]);
        free(final_timers[i]);
    }
    free(merger_array);
    free(final_timers);
}

//function to check if a string is an integer
bool is_integer(char* value) 
{
    int i = 0;
    while(value[i] != '\0'){
        if(isdigit(value[i]) == 0){
            return false;
        }
        i++;
    }
    return true;
}

//function that merges two given arrays (array1, array2) into one (merge_array)
struct Entry* merge(struct Entry* array1, struct Entry* array2, int size1, int size2, struct Entry* merge_array)
{
    int i = 0, j = 0, k = 0;
    
    while(i < size1 && j < size2){

        int result = strcmp(array1[i].lname, array2[j].lname);

        if(result < 0){
            merge_array[k] = array1[i];
            i += 1;
        }
        else if(result == 0){
            int result2 = strcmp(array1[i].fname, array2[j].fname);
            if(result2 < 0){
                merge_array[k] = array1[i];
                i += 1;
            }
            else if(result2 == 0){
                if(array1[i].AM < array2[j].AM){
                    merge_array[k] = array1[i];
                    i += 1;
                }
                else{
                    merge_array[k] = array2[j];
                    j += 1;
                }
            }
            else{
                merge_array[k] = array2[j];
                j += 1;
            }
        }
        else{
            merge_array[k] = array2[j];
            j += 1;
        }
        k += 1;
    }

    while(i < size1){
        merge_array[k] = array1[i];
        i += 1;
        k += 1;
    }

    while(j < size2){
        merge_array[k] = array2[j];
        j += 1;
        k += 1;
    }
    return merge_array;
}

//functions to copy an array into another (copy array2 into array1)
void array_copy(struct Entry** array1, struct Entry** array2, int size2)
{
    for(int i = 0; i < size2; i++){
        (*array1)[i] = (*array2)[i];
    }
}

void array_copy2(struct Timer** array1, struct Timer** array2, int size2)
{
    for(int i = 0; i < size2; i++){
        (*array1)[i] = (*array2)[i];
    }
}