#include "storage_mgr.h"
#include "dberror.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <fcntl.h>
#include <sys/stat.h>

// Initialize storage manager (no-op for this implementation)
void initStorageManager(void) {
    // Optional: Initialize any global variables here
}

// Create a new page file filled with zero bytes
RC createPageFile(char *fileName) {
    // Open file with read/write, create if not exists, truncate to size 0
    int fd = open(fileName, O_RDWR | O_CREAT | O_TRUNC, S_IRUSR | S_IWUSR);
    if (fd == -1) THROW(RC_FILE_NOT_FOUND, "Failed to create file");

    // Allocate and zero-fill a page
    SM_PageHandle page = (SM_PageHandle)calloc(PAGE_SIZE, sizeof(char));
    if (!page) {
        close(fd);
        THROW(RC_WRITE_FAILED, "Memory allocation failed");
    }

    // Write the initial page to disk
    ssize_t bytesWritten = write(fd, page, PAGE_SIZE);
    free(page);
    close(fd);

    if (bytesWritten != PAGE_SIZE)
        THROW(RC_WRITE_FAILED, "Failed to write initial page");
    
    return RC_OK;
}

// Open a page file and initialize the file handle
RC openPageFile(char *fileName, SM_FileHandle *fHandle) {
    // Open existing file
    int fd = open(fileName, O_RDWR);
    if (fd == -1) THROW(RC_FILE_NOT_FOUND, "File not found");

    // Get file size to calculate total pages
    struct stat st;
    fstat(fd, &st);
    fHandle->totalNumPages = st.st_size / PAGE_SIZE;
    fHandle->curPagePos = 0;
    fHandle->fileName = strdup(fileName);
    fHandle->mgmtInfo = (void *)(intptr_t)fd; // Store file descriptor

    return RC_OK;
}

// Close the page file and cleanup
RC closePageFile(SM_FileHandle *fHandle) {
    if (!fHandle->mgmtInfo)
        THROW(RC_FILE_HANDLE_NOT_INIT, "File not open");

    close((int)(intptr_t)fHandle->mgmtInfo);
    free(fHandle->fileName);
    fHandle->mgmtInfo = NULL;
    return RC_OK;
}

// Delete a page file
RC destroyPageFile(char *fileName) {
    if (unlink(fileName) != 0)
        THROW(RC_FILE_NOT_FOUND, "File deletion failed");
    return RC_OK;
}

// Read a specific page into memory
RC readBlock(int pageNum, SM_FileHandle *fHandle, SM_PageHandle memPage) {
    if (pageNum < 0 || pageNum >= fHandle->totalNumPages)
        THROW(RC_READ_NON_EXISTING_PAGE, "Invalid page number");

    int fd = (int)(intptr_t)fHandle->mgmtInfo;
    off_t offset = lseek(fd, pageNum * PAGE_SIZE, SEEK_SET);
    if (offset == -1) THROW(RC_READ_NON_EXISTING_PAGE, "Seek failed");

    if (read(fd, memPage, PAGE_SIZE) != PAGE_SIZE)
        THROW(RC_READ_NON_EXISTING_PAGE, "Read failed");

    fHandle->curPagePos = pageNum;
    return RC_OK;
}

// Get current page position
int getBlockPos(SM_FileHandle *fHandle) {
    return fHandle->curPagePos;
}

// Read first page
RC readFirstBlock(SM_FileHandle *fHandle, SM_PageHandle memPage) {
    return readBlock(0, fHandle, memPage);
}

// Read previous page
RC readPreviousBlock(SM_FileHandle *fHandle, SM_PageHandle memPage) {
    return readBlock(fHandle->curPagePos - 1, fHandle, memPage);
}

// Read current page
RC readCurrentBlock(SM_FileHandle *fHandle, SM_PageHandle memPage) {
    return readBlock(fHandle->curPagePos, fHandle, memPage);
}

// Read next page
RC readNextBlock(SM_FileHandle *fHandle, SM_PageHandle memPage) {
    return readBlock(fHandle->curPagePos + 1, fHandle, memPage);
}

// Read last page
RC readLastBlock(SM_FileHandle *fHandle, SM_PageHandle memPage) {
    return readBlock(fHandle->totalNumPages - 1, fHandle, memPage);
}

// Write data to a specific page
RC writeBlock(int pageNum, SM_FileHandle *fHandle, SM_PageHandle memPage) {
    if (pageNum < 0 || pageNum >= fHandle->totalNumPages)
        THROW(RC_READ_NON_EXISTING_PAGE, "Invalid page number");

    int fd = (int)(intptr_t)fHandle->mgmtInfo;
    off_t offset = lseek(fd, pageNum * PAGE_SIZE, SEEK_SET);
    if (offset == -1) THROW(RC_WRITE_FAILED, "Seek failed");

    if (write(fd, memPage, PAGE_SIZE) != PAGE_SIZE)
        THROW(RC_WRITE_FAILED, "Write failed");

    return RC_OK;
}

// Write to current page
RC writeCurrentBlock(SM_FileHandle *fHandle, SM_PageHandle memPage) {
    return writeBlock(fHandle->curPagePos, fHandle, memPage);
}

// Append a new empty page
RC appendEmptyBlock(SM_FileHandle *fHandle) {
    SM_PageHandle page = (SM_PageHandle)calloc(PAGE_SIZE, sizeof(char));
    if (!page) THROW(RC_WRITE_FAILED, "Memory allocation failed");

    int fd = (int)(intptr_t)fHandle->mgmtInfo;
    lseek(fd, 0, SEEK_END);

    if (write(fd, page, PAGE_SIZE) != PAGE_SIZE) {
        free(page);
        THROW(RC_WRITE_FAILED, "Append failed");
    }

    free(page);
    fHandle->totalNumPages++;
    return RC_OK;
}

// Ensure the file has at least 'numberOfPages' pages
RC ensureCapacity(int numberOfPages, SM_FileHandle *fHandle) {
    if (numberOfPages <= fHandle->totalNumPages)
        return RC_OK;

    for (int i = fHandle->totalNumPages; i < numberOfPages; i++) {
        RC rc = appendEmptyBlock(fHandle);
        if (rc != RC_OK) return rc;
    }

    return RC_OK;
}
