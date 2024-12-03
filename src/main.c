#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <pthread.h>
#include <ctype.h>

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
    int mapper_id;           // For debugging purposes
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
    // You can add more fields if necessary
} reducer_args_t;

// Function to read input files
void read_input_files(const char *input_filename, char ***file_list, int *total_files) {
    // printf("Reading input files from: %s\n", input_filename);
    FILE *fp = fopen(input_filename, "r");
    if (!fp) {
        perror("Failed to open input file");
        exit(EXIT_FAILURE);
    }

    fscanf(fp, "%d", total_files); // Update total_files
    // printf("Total files to process: %d\n", *total_files);
    *file_list = (char **)malloc(*total_files * sizeof(char *)); // Allocate memory for file_list
    for (int i = 0; i < *total_files; i++) {
        (*file_list)[i] = (char *)malloc(FILE_NAME_SIZE * sizeof(char)); // Allocate memory for each file name
        fscanf(fp, "%s", (*file_list)[i]);
        // printf("File %d: %s\n", i + 1, (*file_list)[i]);
    }

    fclose(fp);
    // printf("Finished reading input files.\n");
}

void *mapper_thread(void *arg) {
    mapper_args_t *args = (mapper_args_t *)arg;
    // printf("Mapper %d: Started\n", args->mapper_id);
    char **file_list = args->file_list;
    int total_files = args->total_files;
    int *current_file_index = args->current_file_index;
    pthread_mutex_t *mutex = args->mutex;
    int mapper_id = args->mapper_id;

    // Initialize partial result list
    args->partial_result_size = 0;
    args->partial_result_capacity = 100; // Initial capacity
    args->partial_results = malloc(args->partial_result_capacity * sizeof(word_file_pair_t));
    if (!args->partial_results) {
        fprintf(stderr, "Mapper %d: Failed to allocate memory for partial results\n", mapper_id);
        pthread_exit(NULL);
    }

    while (1) {
        int file_index;

        // Get the next file to process
        pthread_mutex_lock(mutex);
        if (*current_file_index >= total_files) {
            pthread_mutex_unlock(mutex);
            // printf("Mapper %d: No more files to process. Exiting loop.\n", mapper_id);
            break; // No more files to process
        }
        file_index = *current_file_index;
        (*current_file_index)++;
        pthread_mutex_unlock(mutex);

        // printf("Mapper %d: Processing file %s\n", mapper_id, file_list[file_index]);

        // Process the file
        char *file_name = file_list[file_index];
        int file_id = file_index + 1; // IDs start from 1

        // Open the file
        FILE *fp = fopen(file_name, "r");
        if (!fp) {
            fprintf(stderr, "Mapper %d: Failed to open file %s\n", mapper_id, file_name);
            continue; // Skip this file
        }

        // Read the file, extract unique words
        int unique_words_size = 0;
        int unique_words_capacity = 100;
        char **unique_words = malloc(unique_words_capacity * sizeof(char *));
        if (!unique_words) {
            fprintf(stderr, "Mapper %d: Failed to allocate memory for unique words\n", mapper_id);
            fclose(fp);
            continue;
        }

        char word_buffer[256];
        while (fscanf(fp, "%255s", word_buffer) == 1) {
            // Process the word: convert to lowercase, remove non-alphabetic characters
            char processed_word[256];
            int idx = 0;
            for (int i = 0; word_buffer[i] != '\0'; i++) {
                if (isalpha((unsigned char)word_buffer[i])) {
                    processed_word[idx++] = tolower((unsigned char)word_buffer[i]);
                }
            }
            processed_word[idx] = '\0';

            if (idx == 0) {
                // No valid characters in the word
                continue;
            }

            // Check if the word is already in unique_words
            int found = 0;
            for (int i = 0; i < unique_words_size; i++) {
                if (strcmp(unique_words[i], processed_word) == 0) {
                    found = 1;
                    break;
                }
            }

            if (!found) {
                // Add to unique_words
                if (unique_words_size == unique_words_capacity) {
                    unique_words_capacity *= 2;
                    unique_words = realloc(unique_words, unique_words_capacity * sizeof(char *));
                    if (!unique_words) {
                        fprintf(stderr, "Mapper %d: Failed to reallocate memory for unique words\n", mapper_id);
                        break;
                    }
                }
                unique_words[unique_words_size] = strdup(processed_word);
                if (!unique_words[unique_words_size]) {
                    fprintf(stderr, "Mapper %d: Failed to duplicate word\n", mapper_id);
                    break;
                }
                unique_words_size++;
            }
        }

        fclose(fp);

        // Now, for each unique word, add (word, file_id) to partial results
        for (int i = 0; i < unique_words_size; i++) {
            if (args->partial_result_size == args->partial_result_capacity) {
                args->partial_result_capacity *= 2;
                args->partial_results = realloc(args->partial_results, args->partial_result_capacity * sizeof(word_file_pair_t));
                if (!args->partial_results) {
                    fprintf(stderr, "Mapper %d: Failed to reallocate memory for partial results\n", mapper_id);
                    break;
                }
            }
            args->partial_results[args->partial_result_size].word = unique_words[i]; // Transfer ownership
            args->partial_results[args->partial_result_size].file_id = file_id;
            args->partial_result_size++;
        }

        // printf("Mapper %d: Finished processing file %s. Unique words found: %d\n", mapper_id, file_name, unique_words_size);

        // Free unique_words array (but not the strings themselves, as they are now in partial_results)
        free(unique_words);
    } // End of while(1)

    // printf("Mapper %d: Reaching barrier\n", mapper_id);
    pthread_barrier_wait(args->barrier);
    // printf("Mapper %d: Exiting\n", mapper_id);

    pthread_exit(NULL);
}

// Define word_entry_t for storing aggregated results
typedef struct {
    char *word;
    int *file_ids;
    int file_ids_size;
    int file_ids_capacity;
} word_entry_t;

// Comparison function for sorting word entries
int compare_word_entries(const void *a, const void *b) {
    word_entry_t *entry_a = (word_entry_t *)a;
    word_entry_t *entry_b = (word_entry_t *)b;

    if (entry_b->file_ids_size != entry_a->file_ids_size) {
        return entry_b->file_ids_size - entry_a->file_ids_size; // Descending order
    } else {
        return strcmp(entry_a->word, entry_b->word); // Ascending order
    }
}

// Comparison function for sorting integers
int compare_ints(const void *a, const void *b) {
    int int_a = *(const int *)a;
    int int_b = *(const int *)b;

    return int_a - int_b;
}

void *reducer_thread(void *arg) {
    reducer_args_t *args = (reducer_args_t *)arg;
    int reducer_id = args->reducer_id;
    int total_reducers = args->total_reducers;
    mapper_args_t *mapper_args = args->mapper_args;
    int mapper_count = args->mapper_count;
    pthread_barrier_t *barrier = args->barrier;

    // printf("Reducer %d: Started\n", reducer_id);

    // Wait at the barrier until all Mappers are done
    pthread_barrier_wait(barrier);
    // printf("Reducer %d: Passed barrier\n", reducer_id);

    // Initialize an array of word_entry_t
    int word_entries_size = 0;
    int word_entries_capacity = 100;
    word_entry_t *word_entries = malloc(word_entries_capacity * sizeof(word_entry_t));
    if (!word_entries) {
        fprintf(stderr, "Reducer %d: Failed to allocate memory for word entries\n", reducer_id);
        pthread_exit(NULL);
    }

    // Process words from mappers
    for (int m = 0; m < mapper_count; m++) {
        mapper_args_t *mapper = &mapper_args[m];
        for (int i = 0; i < mapper->partial_result_size; i++) {
            char *word = mapper->partial_results[i].word;
            int file_id = mapper->partial_results[i].file_id;

            // Get first letter
            char first_letter = word[0];
            if (first_letter < 'a' || first_letter > 'z') {
                continue; // Ignore words not starting with a lowercase letter
            }

            // Check if this word should be processed by this reducer
            if (reducer_id != (first_letter - 'a') % total_reducers) {
                continue; // Not assigned to this reducer
            }

            // Check if the word is already in word_entries
            int found = 0;
            for (int w = 0; w < word_entries_size; w++) {
                if (strcmp(word_entries[w].word, word) == 0) {
                    found = 1;
                    // Check if file_id is already in file_ids
                    int file_id_found = 0;
                    for (int f = 0; f < word_entries[w].file_ids_size; f++) {
                        if (word_entries[w].file_ids[f] == file_id) {
                            file_id_found = 1;
                            break;
                        }
                    }
                    if (!file_id_found) {
                        // Add file_id to file_ids array
                        if (word_entries[w].file_ids_size == word_entries[w].file_ids_capacity) {
                            word_entries[w].file_ids_capacity *= 2;
                            word_entries[w].file_ids = realloc(word_entries[w].file_ids, word_entries[w].file_ids_capacity * sizeof(int));
                            if (!word_entries[w].file_ids) {
                                fprintf(stderr, "Reducer %d: Failed to reallocate memory for file_ids\n", reducer_id);
                                pthread_exit(NULL);
                            }
                        }
                        word_entries[w].file_ids[word_entries[w].file_ids_size] = file_id;
                        word_entries[w].file_ids_size++;
                    }
                    break;
                }
            }
            if (!found) {
                // Add new word_entry_t
                if (word_entries_size == word_entries_capacity) {
                    word_entries_capacity *= 2;
                    word_entries = realloc(word_entries, word_entries_capacity * sizeof(word_entry_t));
                    if (!word_entries) {
                        fprintf(stderr, "Reducer %d: Failed to reallocate memory for word entries\n", reducer_id);
                        pthread_exit(NULL);
                    }
                }
                word_entry_t new_entry;
                new_entry.word = strdup(word);
                if (!new_entry.word) {
                    fprintf(stderr, "Reducer %d: Failed to duplicate word\n", reducer_id);
                    pthread_exit(NULL);
                }
                new_entry.file_ids_capacity = 10;
                new_entry.file_ids = malloc(new_entry.file_ids_capacity * sizeof(int));
                if (!new_entry.file_ids) {
                    fprintf(stderr, "Reducer %d: Failed to allocate memory for file_ids\n", reducer_id);
                    free(new_entry.word);
                    pthread_exit(NULL);
                }
                new_entry.file_ids[0] = file_id;
                new_entry.file_ids_size = 1;

                word_entries[word_entries_size++] = new_entry;
            }
        }
    }

    // Sort file_ids array for each word_entry_t
    for (int w = 0; w < word_entries_size; w++) {
        qsort(word_entries[w].file_ids, word_entries[w].file_ids_size, sizeof(int), compare_ints);
    }

    // Now sort the word_entries array
    qsort(word_entries, word_entries_size, sizeof(word_entry_t), compare_word_entries);

    // Write the words and their file IDs to the output file
    // The output file is named '<letter>.txt', where <letter> is the first letter of the words

    FILE *output_files[26] = {NULL}; // One for each letter

    for (int w = 0; w < word_entries_size; w++) {
        char first_letter = word_entries[w].word[0]; // 'a' to 'z'

        int letter_index = first_letter - 'a';
        if (!output_files[letter_index]) {
            // Open the output file for this letter
            char output_filename[10];
            snprintf(output_filename, sizeof(output_filename), "%c.txt", first_letter);

            output_files[letter_index] = fopen(output_filename, "w");
            if (!output_files[letter_index]) {
                fprintf(stderr, "Reducer %d: Failed to open output file %s\n", reducer_id, output_filename);
                continue;
            }
        }

        // Write the word and its file IDs to the output file
        fprintf(output_files[letter_index], "%s:[", word_entries[w].word);
        for (int f = 0; f < word_entries[w].file_ids_size; f++) {
            fprintf(output_files[letter_index], "%d", word_entries[w].file_ids[f]);
            if (f < word_entries[w].file_ids_size - 1) {
                fprintf(output_files[letter_index], " ");
            }
        }
        fprintf(output_files[letter_index], "]\n");
    }

    // Close the output files
    for (int i = 0; i < 26; i++) {
        if (output_files[i]) {
            fclose(output_files[i]);
        }
    }

    // Free allocated memory
    for (int w = 0; w < word_entries_size; w++) {
        free(word_entries[w].word);
        free(word_entries[w].file_ids);
    }
    free(word_entries);

    // printf("Reducer %d: Exiting\n", reducer_id);
    pthread_exit(NULL);
}

int main(int argc, char *argv[]) {
    if (argc != 4) {
        printf("Usage: %s <mappers_count> <reducers_count> <input_file>\n", argv[0]);
        exit(EXIT_FAILURE);
    }

    // printf("Starting program with %s mappers, %s reducers, and input file: %s\n", argv[1], argv[2], argv[3]);

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
        // printf("Mapper thread %d created\n", i);
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
        // printf("Reducer thread %d created\n", i);
    }

    // Join all threads
    for (int i = 0; i < mapper_count; i++) {
        pthread_join(mapper_threads[i], NULL);
        // printf("Mapper thread %d joined\n", i);
    }

    for (int i = 0; i < reducer_count; i++) {
        pthread_join(reducer_threads[i], NULL);
        // printf("Reducer thread %d joined\n", i);
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

    // printf("Program completed successfully.\n");

    return 0;
}
