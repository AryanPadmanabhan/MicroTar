#include "minitar.h"

#include <stdio.h>
#include <string.h>
#include "file_list.h"

int main(int argc, char **argv) {
    if (argc < 4) {
        printf("Usage: %s -c|a|t|u|x -f ARCHIVE [FILE...]\n", argv[0]);
        return 0;
    }

    file_list_t files;
    file_list_init(&files);

    // Create operation
    if (strcmp(argv[1], "-c") == 0) {
        // Check for -f flag
        if (strcmp(argv[2], "-f") == 0) {
            // Add file names to the file list
            for (int i = 4; i < argc; i++) {
                file_list_add(&files, argv[i]);
            }

            // Create the archive with the given file list
            if (create_archive(argv[3], &files) == -1) {
                printf("Error with create archive function");
                file_list_clear(&files);
                return 1;
            }
        } else {
            printf("Error: Expected -f flag before the archive name.\n");
            file_list_clear(&files);
            return 1;
        }
    }

    // Append operation
    else if (strcmp(argv[1], "-a") == 0) {
        // Check for -f flag
        if (strcmp(argv[2], "-f") == 0) {
            // Add file names to the file list
            for (int i = 4; i < argc; i++) {
                file_list_add(&files, argv[i]);
            }

            // Append to the archive
            if (append_files_to_archive(argv[3], &files) == -1) {
                printf("Error with append function");
                file_list_clear(&files);
                return 1;
            }
        } else {
            printf("Error: Expected -f flag before the archive name.\n");
            file_list_clear(&files);
            return 1;
        }
    }

    // Update operation
    else if (strcmp(argv[1], "-u") == 0) {
        // Check for -f flag
        if (strcmp(argv[2], "-f") == 0) {
            // Add all remaining arguments (which are file names) to the file list
            for (int i = 4; i < argc; i++) {
                file_list_add(&files, argv[i]);
            }

            file_list_t existing_files;
            file_list_init(&existing_files);
            // Populate existing files list with the files in the archive
            if (get_archive_file_list(argv[3], &existing_files) == -1) {
                printf("Error with list function");
                file_list_clear(&existing_files);
                file_list_clear(&files);
                return 1;
            }
            // Initializing pointers for traversal of the files
            node_t *current_input_file = files.head;
            node_t *current_existing_file = existing_files.head;

            // Outer loop goes through input files, inner loop is the existing files
            while (current_input_file != NULL) {
                int found = 0;
                // Resetting to the start of the files
                current_existing_file = existing_files.head;

                while (current_existing_file != NULL) {
                    if (strcmp(current_existing_file->name, current_input_file->name) == 0) {
                        // The input file exists within the existing files
                        found = 1;
                    }
                    current_existing_file = current_existing_file->next;
                }

                if (!found) {
                    // Exit if a file requested to be updated was not found in the existing files
                    printf("Error: One or more of the specified files is not already present in archive");
                    file_list_clear(&existing_files);
                    file_list_clear(&files);
                    return 1;
                }
                current_input_file = current_input_file->next;
            }

            file_list_clear(&existing_files);

            // Append files to the archive
            if (append_files_to_archive(argv[3], &files) == -1) {
                printf("Error with append function");
                file_list_clear(&files);
                return 1;
            }
        } else {
            printf("Error: Expected -f flag before the archive name.\n");
            file_list_clear(&files);
            return 1;
        }
    }

    // List operator
    else if (strcmp(argv[1], "-t") == 0) {
        // Check for -f flag
        if (strcmp(argv[2], "-f") == 0) {

            // Populate files with archive header names
            if (get_archive_file_list(argv[3], &files) == -1) {
                printf("Error with list function");
                file_list_clear(&files);
                return 1;
            }
            // Print the files
            node_t *current_file = files.head;
            while (current_file != NULL) {
                printf("%s\n", current_file->name);
                current_file = current_file->next;
            }
        } else {
            printf("Error: Expected -f flag before the archive name.\n");
            file_list_clear(&files);
            return 1;
        }
    }

    // Extract operator
    else if (strcmp(argv[1], "-x") == 0) {
        // Check for -f flag
        if (strcmp(argv[2], "-f") == 0) {

            // Extract the files from an archive
            if (extract_files_from_archive(argv[3]) == -1) {
                printf("Error with extract function");
                file_list_clear(&files);
                return 1;
            }
        } else {
            printf("Error: Expected -f flag before the archive name.\n");
            file_list_clear(&files);
            return 1;
        }
    }

    file_list_clear(&files);
    return 0;
}
