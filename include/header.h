#define MAX_SIZE 100    //max string size 

#define error_exit(msg)    do { perror(msg); exit(0); \
                            } while (0)

bool is_integer(char* value);
struct Entry* merge(struct Entry* array1, struct Entry* array2, int size1, int size2, struct Entry* merged_array);
void array_copy(struct Entry** array1, struct Entry** array2, int size2);
void array_copy2(struct Timer** array1, struct Timer** array2, int size2);