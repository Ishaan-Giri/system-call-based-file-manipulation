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

    printf("\rVerifying...: [");
    for (int i = 0; i < barWidth; ++i) {
        if (i < filledBars) {
            printf("=");
        } else {
            printf(" ");
        }
    }
    printf("] %.1f%%", percent);
    // printf("\n");
    fflush(stdout);
    // printf("\n");
}



// Verifies content for Flag 0 (Block-wise Reversal)
bool verifyBlockwise(int oldFd, int newFd, long blockSize, off_t fileSize) {
    char oldBuf[blockSize];
    char newBuf[blockSize];
    ssize_t oldBytesRead, newBytesRead;
    off_t totalVerified = 0;

    while (true) {
        oldBytesRead = read(oldFd, oldBuf, blockSize);
        newBytesRead = read(newFd, newBuf, blockSize);

        if (oldBytesRead == -1 || newBytesRead == -1) {
            printError("Error reading files for block-wise verification");
        }
        if (oldBytesRead == 0 && newBytesRead == 0) {
            break; // End of both files
        }
        if (oldBytesRead != newBytesRead) {
            return false; // Mismatched read sizes
        }

        reverseData(oldBuf, oldBytesRead);
        if (memcmp(oldBuf, newBuf, oldBytesRead) != 0) {
            return false; // Blocks do not match
        }
        totalVerified += oldBytesRead;
        displayProgress(totalVerified, fileSize);
    }
    return true;
}

// Verifies content for Flag 1 (Full File Reversal)
bool verifyFullReversal(int oldFd, int newFd, off_t fileSize) {
    char oldBuf[BUFFER_SIZE];
    char newBuf[BUFFER_SIZE];
    off_t pos = 0;

    while (pos < fileSize) {
        ssize_t currentChunkSize = min((off_t)BUFFER_SIZE, fileSize - pos);
        
        lseek(oldFd, pos, SEEK_SET);
        if (read(oldFd, oldBuf, currentChunkSize) != currentChunkSize) {
            printError("Read error in old file during full reversal check");
        }

        lseek(newFd, fileSize - pos - currentChunkSize, SEEK_SET);
        if (read(newFd, newBuf, currentChunkSize) != currentChunkSize) {
            printError("Read error in new file during full reversal check");
        }

        reverseData(newBuf, currentChunkSize);

        if (memcmp(oldBuf, newBuf, currentChunkSize) != 0) {
            return false;
        }
        pos += currentChunkSize;
        displayProgress(pos, fileSize);
    }
    return true;
}

// Verifies content for Flag 2 (Partial Range Reversal)
bool verifyPartialReversal(int oldFd, int newFd, off_t fileSize, off_t startIdx, off_t endIdx) {
    char oldBuf[BUFFER_SIZE];
    char newBuf[BUFFER_SIZE];
    off_t totalVerified = 0;

    // Part 1: Verify reversed section [0, startIdx - 1]
    off_t part1Size = startIdx;
    off_t pos = 0;
    while (pos < part1Size) {
        ssize_t currentChunkSize = min((off_t)BUFFER_SIZE, part1Size - pos);
        lseek(oldFd, pos, SEEK_SET);
        read(oldFd, oldBuf, currentChunkSize);
        lseek(newFd, part1Size - pos - currentChunkSize, SEEK_SET);
        read(newFd, newBuf, currentChunkSize);
        reverseData(newBuf, currentChunkSize);
        if (memcmp(oldBuf, newBuf, currentChunkSize) != 0) return false;
        pos += currentChunkSize;
        totalVerified += currentChunkSize;
        displayProgress(totalVerified, fileSize);
    }

    // Part 2: Verify identical section [startIdx, endIdx]
    off_t part2Size = endIdx - startIdx + 1;
    pos = 0;
    while (pos < part2Size) {
        ssize_t currentChunkSize = min((off_t)BUFFER_SIZE, part2Size - pos);
        lseek(oldFd, startIdx + pos, SEEK_SET);
        read(oldFd, oldBuf, currentChunkSize);
        lseek(newFd, startIdx + pos, SEEK_SET);
        read(newFd, newBuf, currentChunkSize);
        if (memcmp(oldBuf, newBuf, currentChunkSize) != 0) return false;
        pos += currentChunkSize;
        totalVerified += currentChunkSize;
        displayProgress(totalVerified, fileSize);
    }

    // Part 3: Verify reversed section [endIdx + 1, EOF]
    off_t part3Start = endIdx + 1;
    off_t part3Size = fileSize - part3Start;
    pos = 0;
    while (pos < part3Size) {
        ssize_t currentChunkSize = min((off_t)BUFFER_SIZE, part3Size - pos);
        lseek(oldFd, part3Start + pos, SEEK_SET);
        read(oldFd, oldBuf, currentChunkSize);
        lseek(newFd, fileSize - pos - currentChunkSize, SEEK_SET);
        read(newFd, newBuf, currentChunkSize);
        reverseData(newBuf, currentChunkSize);
        if (memcmp(oldBuf, newBuf, currentChunkSize) != 0) return false;
        pos += currentChunkSize;
        totalVerified += currentChunkSize;
        displayProgress(totalVerified, fileSize);
    }

    return true;
}


// Prints permission details
void printPermissions(const char* path, const string& name) {
    struct stat fileInfo;
    if (stat(path, &fileInfo) == -1) {
        cout << "Could not stat " << name << endl;
        return;
    }

    int perms = fileInfo.st_mode;
    cout << "User has read permissions on " << name << ": " << ((perms & S_IRUSR) ? "Yes" : "No") << endl;
    cout << "User has write permission on " << name << ": " << ((perms & S_IWUSR) ? "Yes" : "No") << endl;
    cout << "User has execute permission on " << name << ": " << ((perms & S_IXUSR) ? "Yes" : "No") << endl;
    cout << "Group has read permissions on " << name << ": " << ((perms & S_IRGRP) ? "Yes" : "No") << endl;
    cout << "Group has write permission on " << name << ": " << ((perms & S_IWGRP) ? "Yes" : "No") << endl;
    cout << "Group has execute permission on " << name << ": " << ((perms & S_IXGRP) ? "Yes" : "No") << endl;
    cout << "Others has read permissions on " << name << ": " << ((perms & S_IROTH) ? "Yes" : "No") << endl;
    cout << "Others has write permission on " << name << ": " << ((perms & S_IWOTH) ? "Yes" : "No") << endl;
    cout << "Others has execute permission on " << name << ": " << ((perms & S_IXOTH) ? "Yes" : "No") << endl;
}


int main(int argc, char* argv[]) {
    // --- Argument Validation ---
    if (argc < 5) {
        cerr << "Usage: " << argv[0] << " <newfilepath> <oldfilepath> <directory> <flag> [args]" << endl;
        return EXIT_FAILURE;
    }

    const char* newFilePath = argv[1];
    const char* oldFilePath = argv[2];
    const char* dirPath = argv[3];
    int flag = stoi(argv[4]);

    if ((flag == 0 && argc != 6) || (flag == 1 && argc != 5) || (flag == 2 && argc != 7)) {
        cerr << "Incorrect number of arguments for flag " << flag << endl;
        return EXIT_FAILURE;
    }

    // --- File and Directory Stat ---
    struct stat oldStat, newStat, dirStat;
    bool contentCorrect = false;

    if (stat(oldFilePath, &oldStat) == -1) printError("Failed to stat old file");
    if (stat(newFilePath, &newStat) == -1) printError("Failed to stat new file");
    if (stat(dirPath, &dirStat) == -1) printError("Failed to stat directory");

    int oldFd = open(oldFilePath, O_RDONLY);
    if (oldFd == -1) printError("Failed to open old file");
    int newFd = open(newFilePath, O_RDONLY);
    if (newFd == -1) printError("Failed to open new file");

    // --- Content Verification ---
    if (oldStat.st_size == newStat.st_size) {
        switch (flag) {
            case 0: {
                long blockSize = stol(argv[5]);
                contentCorrect = verifyBlockwise(oldFd, newFd, blockSize, oldStat.st_size);
                break;
            }
            case 1: {
                contentCorrect = verifyFullReversal(oldFd, newFd, oldStat.st_size);
                break;
            }
            case 2: {
                long long startIdx = stoll(argv[5]);
                long long endIdx = stoll(argv[6]);
                contentCorrect = verifyPartialReversal(oldFd, newFd, oldStat.st_size, startIdx, endIdx);
                break;
            }
            default:
                cerr << "Invalid flag provided." << endl;
                contentCorrect = false;
                break;
        }
    }
    
    // if(contentCorrect) {
    //     displayProgress(oldStat.st_size, oldStat.st_size);
    // } else {
    //     cout << endl;
    // }


    close(oldFd);
    close(newFd);

    // --- Final Output Generation ---
    cout<<endl;
    if (S_ISDIR(dirStat.st_mode)) {
        cout << "Directory is created: Yes" << endl;
    } else {
        cout << "Directory is created: No" << endl;
    }

    if (contentCorrect) {
        cout << "Whether file contents are correctly processed: Yes" << endl;
    } else {
        cout << "Whether file contents are correctly processed: No" << endl;
    }

    if (oldStat.st_size == newStat.st_size) {
        cout << "Both Files Sizes are Same: Yes" << endl;
    } else {
        cout << "Both Files Sizes are Same: No" << endl;
    }

    
    printPermissions(newFilePath, "newfile");
    printPermissions(oldFilePath, "oldfile");
    printPermissions(dirPath, "directory");

    return EXIT_SUCCESS;
}
