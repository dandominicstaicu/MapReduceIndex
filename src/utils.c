#include "utils.h"

void read_input_files(const char *input_filename, char ***file_list, int *total_files) {
    FILE *fp = fopen(input_filename, "r");
    if (!fp) {
        perror("Failed to open input file");
        exit(EXIT_FAILURE);
    }

    fscanf(fp, "%d", total_files); // Update total_files
    *file_list = (char **)malloc(*total_files * sizeof(char *)); // Allocate memory for file_list
    for (int i = 0; i < *total_files; i++) {
        (*file_list)[i] = (char *)malloc(FILE_NAME_SIZE * sizeof(char)); // Allocate memory for each file name
        fscanf(fp, "%s", (*file_list)[i]);
    }

    fclose(fp);
}

int compare_word_entries(const void *a, const void *b) {
    word_entry_t *entry_a = (word_entry_t *)a;
    word_entry_t *entry_b = (word_entry_t *)b;

    if (entry_b->file_ids_size != entry_a->file_ids_size) {
        return entry_b->file_ids_size - entry_a->file_ids_size; // Descending order
    } else {
        return strcmp(entry_a->word, entry_b->word); // Ascending order
    }
}

int compare_ints(const void *a, const void *b) {
    int int_a = *(const int *)a;
    int int_b = *(const int *)b;

    return int_a - int_b;
}
