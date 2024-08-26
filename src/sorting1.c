#include <stdio.h>
#include <stdbool.h>
#include <stdlib.h>
#include <string.h>
#include <ctype.h>
#include <unistd.h>
#include <fcntl.h>
#include <sys/times.h>
#include <signal.h>
#include "structs.h"
#include "header.h"

//swap function to swap two specific entries of the array
void swap(struct Entry* e1, struct Entry* e2)
{
    struct Entry temp = *e1;
    *e1 = *e2;
    *e2 = temp;
}

//partition function used in quicksort
int partition(struct Entry *vot, int low, int high)
{
    struct Entry pivot = vot[high];
    int left = low - 1;

    for(int i = low ; i <= high; i++){

        //i store the result of strcmp into a variable so that i don't call the strcmp function again later
        int result = strcmp(vot[i].lname,pivot.lname);

        //if result < 0, vot[i].lname is alphabetically smaller than pivot.lname 
        if(result < 0){
            left += 1;
            swap(&vot[i], &vot[left]);
        }
        else if(result == 0){
            int result2 = strcmp(vot[i].fname,pivot.fname);
            if(result2 < 0){
                left += 1;
                swap(&vot[i], &vot[left]);
                
            }
            else if(result2 == 0){
                if(vot[i].AM < pivot.AM){
                    left += 1;
                    swap(&vot[i], &vot[left]);
                    
                }
            }
        }
    }

    swap(&vot[high], &vot[left+1]);
    return (left+1);

}

void quicksort(struct Entry *vot, int low, int high)
{
    if(low < high){
        int pivot = partition(vot, low, high);
        quicksort(vot, low, pivot - 1);
        quicksort(vot, pivot + 1, high);
    }
}


int main(int argc, char* argv[])
{
    //timer variables
    struct Timer tim;
    double t1 , t2 , cpu_time;
    struct tms tb1 , tb2;
    double ticspersec;

    ticspersec = ( double ) sysconf ( _SC_CLK_TCK );
    t1 = ( double ) times (& tb1 );

    //fd_write is the end of the pipe where the sorter writes
    int fd_write = atoi(argv[5]);
    char* DataFile = argv[1];
    int from = atoi(argv[3]);
    int to = atoi(argv[4]);
    int root_pid = atoi(argv[6]);
    int fd = open(DataFile, O_RDONLY), count = 0;
    int num_of_entries = atoi(argv[2]);

    //In the entries_array i store all the entries of the input file
    struct Entry* Entries_array = malloc(num_of_entries*sizeof(struct Entry));

    if(Entries_array == NULL) error_exit("Error during memory allocation\n");

    //in the sorted_array I store the part of the array that the sorter will eventually sort
    struct Entry* sorted_array = malloc((to-from) * sizeof(struct Entry));
    if(sorted_array == NULL) error_exit("Error during memory allocation\n");

    while(count < num_of_entries && (read(fd, &Entries_array[count], sizeof(struct Entry))) == sizeof(struct Entry) ) count++;
    
    close(fd);

    int count2 = 0, i = from;
    while(count2 < (to-from) && i < to){
        sorted_array[count2] = Entries_array[i];
        count2 += 1;
        i += 1;   
    }

    quicksort(sorted_array, 0, count2 - 1);

    //write struct by struct into the pipe
    for(int i = 0; i < (to-from); i++) write(fd_write, &sorted_array[i], sizeof(struct Entry));
    
    free(sorted_array);
    free(Entries_array);

    t2 = ( double ) times (&tb2);
    cpu_time = ( double ) (( tb2 . tms_utime + tb2 . tms_stime ) - ( tb1 . tms_utime + tb1 . tms_stime ));
    tim.wall_time = (t2-t1) / ticspersec;
    tim.cpu_time = cpu_time / ticspersec;

    //write the timer struct 
    write(fd_write, &tim, sizeof(struct Timer));

    close(fd_write);
    
    //send signal to the root that sorter has finished its job
    kill(root_pid, SIGUSR2);
    
    return 1;
}