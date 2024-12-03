#ifndef DATA_STRUCTURES_H
#define DATA_STRUCTURES_H

#include <pthread.h>

#define FILE_NAME_SIZE 256

typedef struct {
    char *word;
    int file_id;
} word_file_pair_t;

typedef struct {
    char **file_list;
    int total_files;
    int *current_file_index; // Shared index
    pthread_mutex_t *mutex;  // Mutex to protect current_file_index
    int mapper_id;           // For error debugging purposes
    int total_mappers;
    // For partial result
    int partial_result_size;
    int partial_result_capacity;
    word_file_pair_t *partial_results;
    pthread_barrier_t *barrier;
} mapper_args_t;

typedef struct {
    int reducer_id;
    int total_reducers;
    mapper_args_t *mapper_args; // Array of mapper_args_t
    int mapper_count;
    pthread_barrier_t *barrier;
} reducer_args_t;

// Define word_entry_t for storing aggregated results
typedef struct {
    char *word;
    int *file_ids;
    int file_ids_size;
    int file_ids_capacity;
} word_entry_t;

#endif
