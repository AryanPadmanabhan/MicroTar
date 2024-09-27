#include "minitar.h"

#include <errno.h>
#include <fcntl.h>
#include <grp.h>
#include <math.h>
#include <pwd.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#include <sys/sysmacros.h>
#include <sys/types.h>
#include <unistd.h>

#define NUM_TRAILING_BLOCKS 2
#define MAX_MSG_LEN 128
#define BLOCK_SIZE 512

// Constants for tar compatibility information
#define MAGIC "ustar"

// Constants to represent different file types
// We'll only use regular files in this project
#define REGTYPE '0'
#define DIRTYPE '5'

/*
 * Helper function to compute the checksum of a tar header block
 * Performs a simple sum over all bytes in the header in accordance with POSIX
 * standard for tar file structure.
 */
void compute_checksum(tar_header *header) {
    // Have to initially set header's checksum to "all blanks"
    memset(header->chksum, ' ', 8);
    unsigned sum = 0;
    char *bytes = (char *) header;
    for (int i = 0; i < sizeof(tar_header); i++) {
        sum += bytes[i];
    }
    snprintf(header->chksum, 8, "%07o", sum);
}

/*
 * Populates a tar header block pointed to by 'header' with metadata about
 * the file identified by 'file_name'.
 * Returns 0 on success or -1 if an error occurs
 */
int fill_tar_header(tar_header *header, const char *file_name) {
    memset(header, 0, sizeof(tar_header));
    char err_msg[MAX_MSG_LEN];
    struct stat stat_buf;
    // stat is a system call to inspect file metadata
    if (stat(file_name, &stat_buf) != 0) {
        snprintf(err_msg, MAX_MSG_LEN, "Failed to stat file %s", file_name);
        perror(err_msg);
        return -1;
    }

    strncpy(header->name, file_name, 100);    // Name of the file, null-terminated string
    snprintf(header->mode, 8, "%07o",
             stat_buf.st_mode & 07777);    // Permissions for file, 0-padded octal

    snprintf(header->uid, 8, "%07o", stat_buf.st_uid);    // Owner ID of the file, 0-padded octal
    struct passwd *pwd = getpwuid(stat_buf.st_uid);       // Look up name corresponding to owner ID
    if (pwd == NULL) {
        snprintf(err_msg, MAX_MSG_LEN, "Failed to look up owner name of file %s", file_name);
        perror(err_msg);
        return -1;
    }
    strncpy(header->uname, pwd->pw_name, 32);    // Owner name of the file, null-terminated string

    snprintf(header->gid, 8, "%07o", stat_buf.st_gid);    // Group ID of the file, 0-padded octal
    struct group *grp = getgrgid(stat_buf.st_gid);        // Look up name corresponding to group ID
    if (grp == NULL) {
        snprintf(err_msg, MAX_MSG_LEN, "Failed to look up group name of file %s", file_name);
        perror(err_msg);
        return -1;
    }
    strncpy(header->gname, grp->gr_name, 32);    // Group name of the file, null-terminated string

    snprintf(header->size, 12, "%011o",
             (unsigned) stat_buf.st_size);    // File size, 0-padded octal
    snprintf(header->mtime, 12, "%011o",
             (unsigned) stat_buf.st_mtime);    // Modification time, 0-padded octal
    header->typeflag = REGTYPE;                // File type, always regular file in this project
    strncpy(header->magic, MAGIC, 6);          // Special, standardized sequence of bytes
    memcpy(header->version, "00", 2);          // A bit weird, sidesteps null termination
    snprintf(header->devmajor, 8, "%07o",
             major(stat_buf.st_dev));    // Major device number, 0-padded octal
    snprintf(header->devminor, 8, "%07o",
             minor(stat_buf.st_dev));    // Minor device number, 0-padded octal

    compute_checksum(header);
    return 0;
}

/*
 * Removes 'nbytes' bytes from the file identified by 'file_name'
 * Returns 0 upon success, -1 upon error
 * Note: This function uses lower-level I/O syscalls (not stdio), which we'll learn about later
 */
int remove_trailing_bytes(const char *file_name, size_t nbytes) {
    char err_msg[MAX_MSG_LEN];

    struct stat stat_buf;
    if (stat(file_name, &stat_buf) != 0) {
        snprintf(err_msg, MAX_MSG_LEN, "Failed to stat file %s", file_name);
        perror(err_msg);
        return -1;
    }

    off_t file_size = stat_buf.st_size;
    if (nbytes > file_size) {
        file_size = 0;
    } else {
        file_size -= nbytes;
    }

    if (truncate(file_name, file_size) != 0) {
        snprintf(err_msg, MAX_MSG_LEN, "Failed to truncate file %s", file_name);
        perror(err_msg);
        return -1;
    }
    return 0;
}

int add_files_to_tarfile(FILE *tarfile, const file_list_t *files){ // Helper function to add files to the end of an existing tarfile (can be used in create, append, and update)
    node_t *current_file = files->head;

    // Iterate over all the input files
    while (current_file != NULL) {
        FILE *input_file = fopen(current_file->name, "rb");
        if (!input_file) {
            perror("Error opening file");
            fclose(tarfile);
            return -1;
        }

        // Get the file size
        if (fseek(input_file, 0, SEEK_END) == -1) {
            perror("Error seeking in file");
            return -1;
        }
        int filesize = ftell(input_file);
        if (filesize == -1) {
            perror("ftell");
            return -1;
        }
        // Point back to start of file
        if (fseek(input_file, 0, SEEK_SET) == -1) {
            perror("Error seeking in file");
            return -1;
        }

        // Prepare the TAR header
        tar_header *header = malloc(sizeof(tar_header));
        if (header == NULL) {
            perror("Memory allocation failed for tar header");
            fclose(input_file);
            fclose(tarfile);
            return -1;
        }

        // Fill TAR header
        if (fill_tar_header(header, current_file->name) == -1) {
            perror("Error filling tar header");
            fclose(input_file);
            fclose(tarfile);
            return -1;
        }

        // Write the TAR header to the archive
        if (fwrite(header, 1, BLOCK_SIZE, tarfile) != BLOCK_SIZE) {
            perror("Error writing header");
            fclose(input_file);
            fclose(tarfile);
            return -1;
        }

        // Write the file data to the archive
        char buffer[BLOCK_SIZE];
        size_t bytes_read;
        while ((bytes_read = fread(buffer, 1, sizeof(buffer), input_file)) > 0) {
            if (fwrite(buffer, 1, bytes_read, tarfile) != bytes_read) {
                perror("Error writing contents");
                fclose(input_file);
                fclose(tarfile);
                return -1;
            }
        }
        if (ferror(input_file)) {
            perror("Error reading elements");
            fclose(input_file);
            fclose(tarfile);
            return -1;
        }

        // Add padding to align to the 512-byte boundary
        int padding_size = BLOCK_SIZE - (filesize % BLOCK_SIZE);
        if (padding_size != BLOCK_SIZE) {
            char padding[BLOCK_SIZE] = {0};
            if (fwrite(padding, 1, padding_size, tarfile) != padding_size) {
                perror("Error writing padding");
                fclose(input_file);
                fclose(tarfile);
                return -1;
            }
        }

        if (fclose(input_file) == EOF) {
            perror("fclose()");
            return -1;
        }
        // Move to the next file
        current_file = current_file->next;
        free(header);
    }

    // Write two 512-byte blocks of zeros to mark the end of the archive
    char end_block[BLOCK_SIZE] = {0};
    if (fwrite(end_block, 1, BLOCK_SIZE, tarfile) != BLOCK_SIZE) {
        perror("Error writing zero-block");
        fclose(tarfile);
        return -1;
    }
    if (fwrite(end_block, 1, BLOCK_SIZE, tarfile) != BLOCK_SIZE) {
        perror("Error writing zero-block");
        fclose(tarfile);
        return -1;
    }

    return 0;
}

int create_archive(const char *archive_name, const file_list_t *files) {
    // Open the tarfile
    FILE *tarfile = fopen(archive_name, "wb");
    if (!tarfile) {
        perror("Error creating tar file");
        return -1;
    }

    // Use helper function to add the files to the tarfile
    if (add_files_to_tarfile(tarfile, files) == -1) {
        printf("Error adding files to tarfile");
        fclose(tarfile);
        return -1;
    }

    if (fclose(tarfile) == EOF) {
        perror("fclose()");
        return -1;
    }
    return 0;
}

int append_files_to_archive(const char *archive_name, const file_list_t *files) {
    // Open the tar file
    FILE *tarfile = fopen(archive_name, "a");
    if (!tarfile) {
        perror("Error creating tar file");
        return 1;
    }

    // Removing the footer, this will be added back once we append the files
    if (remove_trailing_bytes(archive_name, 1024) == -1) {
        perror("Error removing tar footer before append");
        fclose(tarfile);
        return 1;
    }

    // Use helper function to add the files to the tarfile
    if (add_files_to_tarfile(tarfile, files) == -1) {
        printf("Error adding files to tarfile");
        fclose(tarfile);
        return -1;
    }

    if (fclose(tarfile) == EOF) {
        perror("fclose()");
        return -1;
    }
    return 0;
}

int get_archive_file_list(const char *archive_name, file_list_t *files) {
    // Open the tar file
    FILE *tarfile = fopen(archive_name, "rb");
    if (!tarfile) {
        perror("Error creating tar file");
        return -1;
    }

    tar_header header;
    int bytes_read;

    // Iterate over all the headers in the tarfile
    while ((bytes_read = fread(&header, sizeof(tar_header), 1, tarfile)) == 1) {
        if (file_list_add(files, header.name) != 0) {
            perror("Error adding file to list");
            fclose(tarfile);
            return -1;
        }

        // Convert size to int
        char *endptr;
        errno = 0;
        int file_size = strtol(header.size, &endptr, 8);
        if (errno != 0) {
            perror("Error strtol()");
            fclose(tarfile);
            return -1;
        }
        // Calculate additional padding to skip over in addition to the size of the file (data is stored in 512 byte blocks)
        int padding = (BLOCK_SIZE - (file_size % BLOCK_SIZE)) % BLOCK_SIZE;
        if (fseek(tarfile, file_size + padding, SEEK_CUR) != 0) {
            perror("Error fseek()");
            fclose(tarfile);
            return -1;
        }
    }

    if (ferror(tarfile)) {
        perror("Error reading from tarfile");
        fclose(tarfile);
        return -1;
    }

    if (fclose(tarfile) == EOF) {
        perror("fclose()");
        return -1;
    }
    return 0;
}

int extract_files_from_archive(const char *archive_name) {
    FILE *tarfile = fopen(archive_name, "rb");
    if (!tarfile) {
        perror("Error creating tar file");
        return 1;
    }

    tar_header header;

    while (fread(&header, sizeof(tar_header), 1, tarfile) == 1) {
        // End of archive
        if (header.name[0] == '\0') {
            break;
        }

        // Convert size to int
        errno = 0;
        int file_size = strtol(header.size, NULL, 8);
        if (errno != 0) {
            perror("Error strtol()");
            fclose(tarfile);
            return -1;
        }

        int remaining = file_size;
        char buffer[BLOCK_SIZE];

        // Open a new file called header.name
        FILE *new_file = fopen(header.name, "w");
        if (new_file == NULL) {
            perror("Error creating new file");
            fclose(tarfile);
            return -1;
        }

        // While there are bytes to still be read
        while (remaining > 0) {
            // sets the bytes to read to the remaining if it is less than 512 bytes, else just 512 bytes
            size_t to_read = (remaining < BLOCK_SIZE) ? remaining : BLOCK_SIZE;
            fread(buffer, 1, to_read, tarfile);
            if (ferror(tarfile)) {
                perror("Error reading from tarfile");
                fclose(tarfile);
                fclose(new_file);
                return -1;
            }
            // Write bytes read to the new file
            if (fwrite(buffer, 1, to_read, new_file) != to_read) {
                perror("Error writing to new file");
                fclose(tarfile);
                fclose(new_file);
                return -1;
            }
            // Decrement remaining by however many bytes we read
            remaining -= to_read;
        }

        // Calculate the padding used in the tarfile to skip over those bytes
        int padding = (BLOCK_SIZE - (file_size % BLOCK_SIZE)) % BLOCK_SIZE;
        if (fseek(tarfile, padding, SEEK_CUR) == -1) {
            perror("Error fseek()");
            fclose(tarfile);
            fclose(new_file);
            return -1;
        }

        if (fclose(new_file) == EOF) {
            perror("Error fclose()");
            fclose(tarfile);
            return -1;
        }
    }

    if (ferror(tarfile)) {
        perror("Error reading from tarfile");
        fclose(tarfile);
        return -1;
    }

    if (fclose(tarfile) == EOF) {
        perror("Error fclose()");
        return -1;
    }

    return 0;
}
