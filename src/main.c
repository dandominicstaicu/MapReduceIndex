#include <stdio.h>
#include <stdlib.h>

#define FILE_NAME_SIZE 256


// Function to read input files
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

int main(int argc, char *argv[]) {
    if (argc != 4) {
        printf("Usage: %s <mappers_count> <reducers_count> <input_file>\n", argv[0]);
        exit(EXIT_FAILURE);
    }

    char **file_list; // Pointer to hold list of file names
    int total_files; // Variable to hold total number of files

    read_input_files(argv[3], &file_list, &total_files);

    int mapper_count = atoi(argv[1]);    // Number of Mapper threads
    int reducer_count = atoi(argv[2]);   // Number of Reducer threads

    for (int i = 0; i < total_files; i++) {
        free(file_list[i]);
    }
    free(file_list);

    return 0;
}
