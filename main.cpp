#include <iostream>
#include <string>
#include <fcntl.h>
#include <unistd.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <cerrno>
#include <cstring>

using namespace std;

#define BUFFER_SIZE 8192 // 8KB processing size

// Print error and exit
void printError(const char* message) {
    perror(message);
    exit(EXIT_FAILURE);
}

// Reverse a buffer in place
void reverseData(char *data, ssize_t size) {
    for (ssize_t i = 0; i < size / 2; ++i) {
        char temp = data[i];
        data[i] = data[size - 1 - i];
        data[size - 1 - i] = temp;
    }
}

// Show progress percentage
void displayProgress(off_t completed, off_t total) {
    if (total == 0) return;

    double percent = ((double)completed / total) * 100.0;
    int barWidth = 50; // Width of the progress bar
    int filledBars = (int)(percent / (100.0 / barWidth));

    printf("\rProgress: [");
    for (int i = 0; i < barWidth; ++i) {
        if (i < filledBars) {
            printf("=");
        } else {
            printf(" ");
        }
    }
    printf("] %.1f%%", percent);
    if(percent == 100) printf("\nOperation complete!!");

    fflush(stdout);
}

int main(int argc, char* argv[]) {
    // Validate arguments
    // Used cerr instead of cout as cerr doesn't buffer before printing the data!!
    if (argc < 3) {
        cerr << "Usage: " << argv[0] << " <input_file> <flag> [extra args]" << endl;
        return EXIT_FAILURE;
    }

    const char* inputFile = argv[1];
    int flag = stoi(argv[2]);

    // Create output directory
    const char* folderName = "Assignment1";
    if (mkdir(folderName, 0700) == -1 && errno != EEXIST) {
        printError("Error creating directory");
    }

    // Open input file
    int inFile = open(inputFile, O_RDONLY);
    if (inFile == -1) {
        printError("Error opening input file");
    }

    // Get file size
    struct stat fileInfo;
    if (fstat(inFile, &fileInfo) == -1) {
        printError("Error getting file stats");
    }
    long long fileSize = fileInfo.st_size;

    // Build output path
    string outFilePath = string(folderName) + "/" + argv[2] + "_" + string(inputFile);
    int outFile = open(outFilePath.c_str(), O_WRONLY | O_CREAT | O_TRUNC, 0600);
    if (outFile == -1) {
        printError("Error creating output file");
    }

    char buffer[BUFFER_SIZE];
    long long totalWritten = 0;

    // Process according to flag
    switch (flag) {
        case 0: { // Block-wise reversal
            if (argc != 4) {
                cerr << "Usage: " << argv[0] << " <input_file> 0 <block_size>" << endl;
                close(inFile);
                close(outFile);
                return EXIT_FAILURE;
            }
            long blockSize = stol(argv[3]);
            if (blockSize <= 0) {
                cerr << "Block size must be positive." << endl;
                return EXIT_FAILURE;
            }

            char* blockBuffer = new char[blockSize];
            ssize_t bytesRead;
            while ((bytesRead = read(inFile, blockBuffer, blockSize)) > 0) {
                reverseData(blockBuffer, bytesRead);
                if (write(outFile, blockBuffer, bytesRead) != bytesRead) {
                    delete[] blockBuffer;
                    printError("Write error in block-wise reversal");
                }
                totalWritten += bytesRead;
                displayProgress(totalWritten, fileSize);
            }
            delete[] blockBuffer;
            break;
        }
        case 1: { // Full file reversal
            if (argc != 3) {
                cerr << "Usage: " << argv[0] << " <input_file> 1" << endl;
                close(inFile);
                close(outFile);
                return EXIT_FAILURE;
            }
            long long pos = fileSize;
            while (pos > 0) {
                long long start = max(0LL, pos - BUFFER_SIZE);
                long long size = pos - start;
                lseek(inFile, start, SEEK_SET);
                if (read(inFile, buffer, size) != size) {
                    printError("Read error in full reversal");
                }
                reverseData(buffer, size);
                if (write(outFile, buffer, size) != size) {
                    printError("Write error in full reversal");
                }
                pos = start;
                totalWritten += size;
                displayProgress(totalWritten, fileSize);
            }
            break;
        }
        case 2: { // Partial range reversal
            if (argc != 5) {
                cerr << "Usage: " << argv[0] << " <input_file> 2 <start_index> <end_index>" << endl;
                close(inFile);
                close(outFile);
                return EXIT_FAILURE;
            }
            long long startIndex = stoll(argv[3]);
            long long endIndex = stoll(argv[4]);

            if (startIndex < 0 || endIndex < startIndex || endIndex >= fileSize) {
                cerr << "Invalid start/end indices." << endl;
                return EXIT_FAILURE;
            }

            // Part 1: Reverse from beginning to startIndex - 1
            long long firstPartSize = startIndex;
            long long pos = firstPartSize;
            while (pos > 0) {
                long long start = max(0LL, pos - BUFFER_SIZE);
                long long size = pos - start;
                lseek(inFile, start, SEEK_SET);
                read(inFile, buffer, size);
                reverseData(buffer, size);
                write(outFile, buffer, size);
                pos = start;
                totalWritten += size;
                displayProgress(totalWritten, fileSize);
            }
            

            // Part 2: Copy from startIndex to endIndex
            long long middleSize = endIndex - startIndex + 1;
            lseek(inFile, startIndex, SEEK_SET);
            lseek(outFile, startIndex, SEEK_SET);
            long long writtenMiddle = 0;
            while (writtenMiddle < middleSize) {
                long long size = min((long long)BUFFER_SIZE, middleSize - writtenMiddle);
                long long readSize = read(inFile, buffer, size);
                if (readSize <= 0) break;
                write(outFile, buffer, readSize);
                writtenMiddle += readSize;
                totalWritten += readSize;
                displayProgress(totalWritten, fileSize);
            }
            

            // Part 3: Reverse from endIndex + 1 to EOF
            long long thirdPartStart = endIndex + 1;
            long long thirdPartSize = fileSize - thirdPartStart;
            pos = fileSize;
            lseek(outFile, thirdPartStart, SEEK_SET);
            while (pos > thirdPartStart) {
                long long start = max(thirdPartStart, pos - BUFFER_SIZE);
                long long size = pos - start;
                lseek(inFile, start, SEEK_SET);
                read(inFile, buffer, size);
                reverseData(buffer, size);
                write(outFile, buffer, size);
                pos = start;

                totalWritten += size;
                displayProgress(totalWritten, fileSize);
            }
            
            break;
        }
        default:
            cerr << "Invalid flag. Use 0, 1, or 2." << endl;
            close(inFile);
            close(outFile);
            return EXIT_FAILURE;
    }

    cout << endl;
    close(inFile);
    close(outFile);

    return EXIT_SUCCESS;
}
