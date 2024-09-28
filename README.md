# MicroTar

MicroTar is a lightweight, simplified version of the `tar` utility used for creating, updating, appending, listing, and extracting files from an archive. It is designed to be a compact, easy-to-use alternative to `tar`, suitable for educational purposes or lightweight archiving tasks.

## Features

- **Create** a new archive from a list of files
- **Append** files to an existing archive
- **Update** files in an archive with newer versions
- **List** all files contained in an archive
- **Extract** files from an archive

MicroTar is fully compliant with the POSIX tar standard, allowing interoperability with other tar utilities, meaning you can extract archives created by MicroTar using standard tar utilities and vice versa.

## Getting Started

### Prerequisites

- A C compiler (e.g., `gcc`)
- `make` utility for building the project

### Building MicroTar

1. Clone the repository:
   ```bash
   git clone https://github.com/YourUsername/microtar.git
   cd microtar
2. Build the project using make
    ```
    make
    ```

### Usage

MicroTar provides a command-line interface similar to the standard tar utility. The basic syntax is:

./microtar <operation> -f <archive_name> [file_name1 file_name2 ... file_nameN]

-c : Create a new archive from the specified files.
-a : Append files to an existing archive.
-u : Update existing files in the archive.
-t : List all files contained in the archive.
-x : Extract all files from the archive.


### Examples
```
./microtar -c -f archive.tar file1.txt file2.txt

```

### Error Handling

MicroTar provides informative error messages in case of issues such as:

Attempting to add a non-existent file
Trying to update a file that is not present in the archive
Issues with reading/writing files

### Compatibility

MicroTar is POSIX-compliant and should work on any Unix-like system. It has been tested on Linux but should work on macOS and other environments with minimal changes.

