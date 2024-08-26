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

//swap function to swap two specific entries of an array
void swap(struct Entry* e1, struct Entry* e2)
{
    struct Entry temp = *e1;
    *e1 = *e2;
    *e2 = temp;
}

void merge1(struct Entry* vot, int left, int right, int mid)
{
    int i, j, k;
    int size1 = mid - left + 1;
    int size2 = right - mid;

    //array_l is the left half of the vot array
    struct Entry *array_l = malloc(size1*sizeof(struct Entry));

    //array_r is the right half of the vot array
    struct Entry *array_r = malloc(size2*sizeof(struct Entry));

    for(i = 0; i < size1; i++){
        array_l[i] = vot[left + i];
    } 

    for(j = 0; j < size2; j++){
        array_r[j] = vot[mid + 1 + j];
    }

    i = 0;
    j = 0;
    k = left;

    while(i < size1 && j < size2){

        //store the output of strcmp into a variable so that i dont call the strcmp function again later
        int result = strcmp(array_l[i].lname, array_r[j].lname);

        if(result < 0){
            vot[k] = array_l[i];
            i += 1;
        }
        else if(result == 0){
            int result2 = strcmp(array_l[i].fname, array_r[j].fname);
            if(result2 < 0){
                vot[k] = array_l[i];
                i += 1;
            }
            else if(result2 == 0){
                if(array_l[i].AM < array_r[j].AM){
                    vot[k] = array_l[i];
                    i += 1;
                }
                else{
                    vot[k] = array_r[j];
                    j += 1;
                }
            }
            else{
                vot[k] = array_r[j];
                j += 1;
            }
        }
        else{
            vot[k] = array_r[j];
            j += 1;
        }
        k += 1;
    }

    while(i < size1){
        vot[k] = array_l[i];
        i += 1;
        k += 1;
    }

    while(j < size2){
        vot[k] = array_r[j];
        j += 1;
        k += 1;
    }
    free(array_l);
    free(array_r);
}

void mergesort(struct Entry *vot, int left, int right)
{
    if(left < right){
        int mid = left + (right - left)/2;
        mergesort(vot, left, mid);
        mergesort(vot, mid+1, right);
        merge1(vot, left, right , mid);
    }
}

int main(int argc, char* argv[])
{
    //variables for the timer
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

    while(count < num_of_entries && (read(fd, &Entries_array[count], sizeof(struct Entry))) == sizeof(struct Entry)) count++;
    
    close(fd);
    
    int count2 = 0, i = from;
    while(count2 < (to-from) && i < to){
        sorted_array[count2] = Entries_array[i];
        count2 += 1;
        i += 1;   
    }

    mergesort(sorted_array, 0, count2-1);

    //write struct by struct into the pipe
    for(int i = 0; i < (to-from); i++) write(fd_write, &sorted_array[i], sizeof(struct Entry));
    
    t2 = ( double ) times (&tb2);
    cpu_time = ( double ) (( tb2 . tms_utime + tb2 . tms_stime ) - ( tb1 . tms_utime + tb1 . tms_stime ));
    tim.wall_time = (t2-t1) / ticspersec;
    tim.cpu_time = cpu_time / ticspersec;

    //write the timer  struct 
    write(fd_write, &tim, sizeof(struct Timer));

    close(fd_write);

    free(sorted_array);
    free(Entries_array);

    //send signal to the root that sorter has finished its job
    kill(root_pid, SIGUSR2);
    
    return 1;
}