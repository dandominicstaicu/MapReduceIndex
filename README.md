## Introduction

This program implements a parallel inverted index generator using the MapReduce model with Pthreads in C. The goal is to process a set of input text files and produce an inverted index that lists all unique words along with the files in which they appear.

## Implementation Details
### Overview

The program simulates the MapReduce paradigm by dividing the workload among multiple Mapper and Reducer threads. The Mappers read and process the input files to extract unique words, and the Reducers aggregate the partial results to build the final inverted index.
### Mapper Threads

- Dynamic File Allocation: Mappers dynamically receive files to process using a shared index protected by a mutex. This ensures an efficient and balanced distribution of files among the Mappers.

- Unique Word Extraction: Each Mapper reads its assigned files and extracts unique words per file. Words are processed to be in lowercase and stripped of non-alphabetic characters.

- Partial Results Storage: Mappers store their results as pairs of (word, file_id) in a dynamically resizable array.

- Synchronization: A barrier is used to ensure that all Mappers complete their processing before the Reducers start.

### Reducer Threads

- Aggregation of Results: Reducers collect the partial results from all Mappers and aggregate them to form the inverted index.

- Word Assignment: Each Reducer is responsible for words starting with certain letters, determined by the formula reducer_id == (first_letter - 'a') % total_reducers.

- Building Inverted Index: For each word, Reducers maintain a list of unique file IDs where the word appears. Duplicate file IDs are avoided.

- Sorting:
    - Words are sorted in descending order based on the number of files they appear in.
    - In case of a tie, they are sorted alphabetically.

- Output Generation: Reducers write the sorted words and their associated file IDs to output files named "letter.txt", where "letter" is the starting letter of the words.

### Data Structures

- word_file_pair_t: Used by Mappers to store pairs of words and file IDs.

- word_entry_t: Used by Reducers to store words along with an array of file IDs.

- Dynamic Arrays: Utilized throughout the program to handle varying sizes of data efficiently.

### Synchronization Mechanisms

- Mutex (pthread_mutex_t): Protects access to the shared file index among Mappers.

- Barrier (pthread_barrier_t): Ensures that all Mappers finish before Reducers start processing.
