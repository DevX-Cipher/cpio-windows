#pragma once

#ifndef CPIO_H
#define CPIO_H

#include <windows.h>

#ifdef __cplusplus
extern "C" {
#endif

void* CpioAlloc(SIZE_T size);
void CpioFree(void* ptr);
void* CpioRealloc(void* ptr, SIZE_T newSize);
void CpioZeroMemory(void* ptr, SIZE_T size);
void CpioCopyMemory(void* dest, const void* src, SIZE_T size);
int CpioCompareMemory(const void* ptr1, const void* ptr2, SIZE_T size);

typedef struct {
    char* data;
    SIZE_T length;
    SIZE_T capacity;
} CpioString;

CpioString* CpioStringCreate(void);
void CpioStringDestroy(CpioString* str);
BOOL CpioStringAppend(CpioString* str, const char* data, SIZE_T len);
BOOL CpioStringAppendChar(CpioString* str, char c);
BOOL CpioStringSet(CpioString* str, const char* data, SIZE_T len);
void CpioStringClear(CpioString* str);
SIZE_T CpioStringLength(const char* str);
int CpioStringCompare(const char* s1, const char* s2);
BOOL CpioStringStartsWith(const char* str, const char* prefix);

typedef struct {
    char** items;
    SIZE_T count;
    SIZE_T capacity;
} CpioStringList;

CpioStringList* CpioStringListCreate(void);
void CpioStringListDestroy(CpioStringList* list);
BOOL CpioStringListAdd(CpioStringList* list, const char* str);

#define CPIO_HASHSET_SIZE 256

typedef struct CpioHashSetNode {
    char* key;
    struct CpioHashSetNode* next;
} CpioHashSetNode;

typedef struct {
    CpioHashSetNode* buckets[CPIO_HASHSET_SIZE];
} CpioHashSet;

CpioHashSet* CpioHashSetCreate(void);
void CpioHashSetDestroy(CpioHashSet* set);
BOOL CpioHashSetContains(CpioHashSet* set, const char* key);
BOOL CpioHashSetInsert(CpioHashSet* set, const char* key);

typedef enum {
    CPIO_SUCCESS = 0,
    CPIO_ERROR_IO,
    CPIO_ERROR_BAD_MAGIC,
    CPIO_ERROR_BAD_HEADER,
    CPIO_ERROR_FILENAME_DECODE,
    CPIO_ERROR_VALUE_TOO_LARGE,
    CPIO_ERROR_SIZE_MISMATCH,
    CPIO_ERROR_NOT_A_FILE,
    CPIO_ERROR_INVALID_HANDLE,
    CPIO_ERROR_ALLOCATION_FAILED,
    CPIO_ERROR_INVALID_PARAMETER
} CpioErrorCode;

typedef struct {
    CpioErrorCode code;
    DWORD lastError;
    char message[256];
} CpioError;

void CpioErrorSet(CpioError* error, CpioErrorCode code, const char* message);
const char* CpioErrorGetMessage(const CpioError* error);

typedef enum {
    CPIO_FORMAT_UNKNOWN,
    CPIO_FORMAT_NEWC,
    CPIO_FORMAT_ODC
} CpioFormat;

CpioFormat CpioDetectFormat(HANDLE hFile, CpioError* error);

#define CPIO_MAGIC_SIZE 6
#define CPIO_MAX_NAME_LENGTH 4096

#define CPIO_S_IFDIR 0x4000
#define CPIO_S_IFREG 0x8000
#define CPIO_S_IRUSR 0x0100
#define CPIO_S_IWUSR 0x0080
#define CPIO_S_IXUSR 0x0040
#define CPIO_S_IRGRP 0x0020
#define CPIO_S_IWGRP 0x0010
#define CPIO_S_IXGRP 0x0008
#define CPIO_S_IROTH 0x0004
#define CPIO_S_IWOTH 0x0002
#define CPIO_S_IXOTH 0x0001

typedef struct {
    UINT32 inode;
    UINT32 mode;
    UINT32 uid;
    UINT32 gid;
    UINT32 nlink;
    UINT32 mtime;
    UINT64 fileSize;
    UINT32 devMajor;
    UINT32 devMinor;
    UINT32 rdevMajor;
    UINT32 rdevMinor;
    UINT32 checksum;
    char name[CPIO_MAX_NAME_LENGTH];
} CpioNewcHeader;

BOOL CpioNewcHeaderRead(HANDLE hFile, CpioNewcHeader* header, CpioError* error);
UINT64 CpioNewcHeaderWrite(HANDLE hFile, const CpioNewcHeader* header, CpioError* error);

typedef struct {
    HANDLE hFile;
    BOOL ownsHandle;
    UINT64 currentEntrySize;
    UINT64 currentEntryRead;
    SIZE_T entryDataPad;
    BOOL seenTrailer;
    BOOL firstEntry;
} CpioNewcReader;

CpioNewcReader* CpioNewcReaderCreate(HANDLE hFile, BOOL takeOwnership);
void CpioNewcReaderDestroy(CpioNewcReader* reader);
BOOL CpioNewcReaderReadNext(CpioNewcReader* reader, CpioNewcHeader* header, CpioError* error);
DWORD CpioNewcReaderRead(CpioNewcReader* reader, void* buffer, DWORD bufferSize, CpioError* error);
BOOL CpioNewcReaderFinish(CpioNewcReader* reader, CpioError* error);
BOOL CpioNewcReaderIsAtEnd(const CpioNewcReader* reader);

typedef struct {
    HANDLE hFile;
    BOOL ownsHandle;
    UINT32 defaultUid;
    UINT32 defaultGid;
    UINT32 defaultMtime;
    UINT32 defaultModeFile;
    UINT32 defaultModeDir;
    BOOL autoWriteDirs;
    CpioHashSet* seenDirs;
    UINT32 entryCount;
    BOOL finished;
} CpioNewcBuilder;

CpioNewcBuilder* CpioNewcBuilderCreate(HANDLE hFile, BOOL takeOwnership);
void CpioNewcBuilderDestroy(CpioNewcBuilder* builder);
void CpioNewcBuilderNextHeader(CpioNewcBuilder* builder, CpioNewcHeader* header);
UINT64 CpioNewcBuilderAppendFileFromPath(CpioNewcBuilder* builder, const char* archivePath, 
                                          const WCHAR* filePath, CpioError* error);
UINT64 CpioNewcBuilderEmitRootDirectory(CpioNewcBuilder* builder, CpioError* error);
UINT64 CpioNewcBuilderFinish(CpioNewcBuilder* builder, CpioError* error);

typedef struct {
    UINT32 dev;
    UINT32 inode;
    UINT32 mode;
    UINT32 uid;
    UINT32 gid;
    UINT32 nlink;
    UINT32 rdev;
    UINT32 mtime;
    UINT64 fileSize;
    char name[CPIO_MAX_NAME_LENGTH];
} CpioOdcHeader;

BOOL CpioOdcHeaderRead(HANDLE hFile, CpioOdcHeader* header, CpioError* error);
UINT64 CpioOdcHeaderWrite(HANDLE hFile, const CpioOdcHeader* header, CpioError* error);

typedef struct {
    HANDLE hFile;
    BOOL ownsHandle;
    UINT64 currentEntrySize;
    UINT64 currentEntryRead;
    BOOL seenTrailer;
    BOOL firstEntry;
} CpioOdcReader;

CpioOdcReader* CpioOdcReaderCreate(HANDLE hFile, BOOL takeOwnership);
void CpioOdcReaderDestroy(CpioOdcReader* reader);
BOOL CpioOdcReaderReadNext(CpioOdcReader* reader, CpioOdcHeader* header, CpioError* error);
DWORD CpioOdcReaderRead(CpioOdcReader* reader, void* buffer, DWORD bufferSize, CpioError* error);
BOOL CpioOdcReaderFinish(CpioOdcReader* reader, CpioError* error);
BOOL CpioOdcReaderIsAtEnd(const CpioOdcReader* reader);

typedef struct {
    HANDLE hFile;
    BOOL ownsHandle;
    UINT32 defaultUid;
    UINT32 defaultGid;
    UINT32 defaultMtime;
    UINT32 defaultModeFile;
    UINT32 defaultModeDir;
    BOOL autoWriteDirs;
    CpioHashSet* seenDirs;
    UINT32 entryCount;
    BOOL finished;
} CpioOdcBuilder;

CpioOdcBuilder* CpioOdcBuilderCreate(HANDLE hFile, BOOL takeOwnership);
void CpioOdcBuilderDestroy(CpioOdcBuilder* builder);
void CpioOdcBuilderNextHeader(CpioOdcBuilder* builder, CpioOdcHeader* header);
UINT64 CpioOdcBuilderAppendFileFromPath(CpioOdcBuilder* builder, const char* archivePath,
                                         const WCHAR* filePath, CpioError* error);
UINT64 CpioOdcBuilderEmitRootDirectory(CpioOdcBuilder* builder, CpioError* error);
UINT64 CpioOdcBuilderFinish(CpioOdcBuilder* builder, CpioError* error);

BOOL CpioNormalizeArchivePath(const char* path, char* output, SIZE_T outputSize);
WCHAR* CpioStringToWide(const char* str);
char* CpioWideToString(const WCHAR* wstr);
UINT32 CpioGetCurrentUnixTime(void);

#ifdef __cplusplus
}
#endif

#endif