#include <stdio.h>
#include <stdlib.h>

#include "reducer.h"
#include "mapper.h"
#include "utils.h"
#include "data_structures.h"

int main(int argc, char *argv[]) {
    if (argc != 4) {
        printf("Usage: %s <mappers_count> <reducers_count> <input_file>\n", argv[0]);
        exit(EXIT_FAILURE);
    }


    char **file_list; // Pointer to hold list of file names
    int total_files;  // Variable to hold total number of files

    read_input_files(argv[3], &file_list, &total_files);

    int mapper_count = atoi(argv[1]);    // Number of Mapper threads
    int reducer_count = atoi(argv[2]);   // Number of Reducer threads

    // Initialize the mutex and shared index
    pthread_mutex_t mutex;
    pthread_mutex_init(&mutex, NULL);

    int current_file_index = 0;

    // Initialize the barrier
    pthread_barrier_t barrier;
    int total_threads = mapper_count + reducer_count;
    pthread_barrier_init(&barrier, NULL, total_threads);

    // Create Mapper threads
    pthread_t *mapper_threads = malloc(mapper_count * sizeof(pthread_t));
    mapper_args_t *mapper_args = malloc(mapper_count * sizeof(mapper_args_t));

    for (int i = 0; i < mapper_count; i++) {
        mapper_args[i].file_list = file_list;
        mapper_args[i].total_files = total_files;
        mapper_args[i].current_file_index = &current_file_index;
        mapper_args[i].mutex = &mutex;
        mapper_args[i].mapper_id = i;
        mapper_args[i].total_mappers = mapper_count;
        // Initialize partial result fields
        mapper_args[i].partial_result_size = 0;
        mapper_args[i].partial_result_capacity = 0;
        mapper_args[i].partial_results = NULL;
        mapper_args[i].barrier = &barrier;

        int rc = pthread_create(&mapper_threads[i], NULL, mapper_thread, &mapper_args[i]);
        if (rc) {
            fprintf(stderr, "Failed to create Mapper thread %d\n", i);
            exit(EXIT_FAILURE);
        }
    }

    // Create Reducer threads
    pthread_t *reducer_threads = malloc(reducer_count * sizeof(pthread_t));
    reducer_args_t *reducer_args = malloc(reducer_count * sizeof(reducer_args_t));

    for (int i = 0; i < reducer_count; i++) {
        reducer_args[i].reducer_id = i;
        reducer_args[i].total_reducers = reducer_count;
        reducer_args[i].mapper_args = mapper_args; // So reducers can access mappers' partial results
        reducer_args[i].mapper_count = mapper_count;
        reducer_args[i].barrier = &barrier;

        int rc = pthread_create(&reducer_threads[i], NULL, reducer_thread, &reducer_args[i]);
        if (rc) {
            fprintf(stderr, "Failed to create Reducer thread %d\n", i);
            exit(EXIT_FAILURE);
        }
    }

    // Join all threads
    for (int i = 0; i < mapper_count; i++) {
        pthread_join(mapper_threads[i], NULL);
    }

    for (int i = 0; i < reducer_count; i++) {
        pthread_join(reducer_threads[i], NULL);
    }

    // Destroy the mutex and barrier
    pthread_mutex_destroy(&mutex);
    pthread_barrier_destroy(&barrier);

    // Free allocated memory
    for (int i = 0; i < mapper_count; i++) {
        // Free partial_results
        for (int j = 0; j < mapper_args[i].partial_result_size; j++) {
            free(mapper_args[i].partial_results[j].word);
        }
        free(mapper_args[i].partial_results);
    }
    free(mapper_args);
    free(mapper_threads);

    free(reducer_args);
    free(reducer_threads);

    for (int i = 0; i < total_files; i++) {
        free(file_list[i]);
    }
    free(file_list);

    return 0;
}
