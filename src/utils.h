#ifndef UTILS_H
#define UTILS_H

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "data_structures.h"

// Function to read input files
void read_input_files(const char *input_filename, char ***file_list, int *total_files);

// Comparison function for sorting word entries
int compare_word_entries(const void *a, const void *b);

// Comparison function for sorting integers
int compare_ints(const void *a, const void *b);

#endif
