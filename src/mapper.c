#include "mapper.h"

void *mapper_thread(void *arg) {
    mapper_args_t *args = (mapper_args_t *)arg;
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
            break; // No more files to process
        }
        file_index = *current_file_index;
        (*current_file_index)++;
        pthread_mutex_unlock(mutex);

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


        // Free unique_words array (but not the strings themselves, as they are now in partial_results)
        free(unique_words);
    } // End of while(1)

    pthread_barrier_wait(args->barrier);

    pthread_exit(NULL);
}
