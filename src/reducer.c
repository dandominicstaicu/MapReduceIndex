#include "reducer.h"

void *reducer_thread(void *arg) {
    reducer_args_t *args = (reducer_args_t *)arg;
    int reducer_id = args->reducer_id;
    int total_reducers = args->total_reducers;
    mapper_args_t *mapper_args = args->mapper_args;
    int mapper_count = args->mapper_count;
    pthread_barrier_t *barrier = args->barrier;


    // Wait at the barrier until all Mappers are done
    pthread_barrier_wait(barrier);

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

    pthread_exit(NULL);
}
