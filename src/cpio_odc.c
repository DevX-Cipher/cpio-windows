#include "cpio.h"

static UINT32 OctalCharToValue(char c) {
    if (c >= '0' && c <= '7') return c - '0';
    return 0;
}

static UINT32 ParseOctalU32(const char* data, SIZE_T length) {
    UINT32 result = 0;
    for (SIZE_T i = 0; i < length; i++) {
        result = (result << 3) | OctalCharToValue(data[i]);
    }
    return result;
}

static UINT64 ParseOctalU64(const char* data, SIZE_T length) {
    UINT64 result = 0;
    for (SIZE_T i = 0; i < length; i++) {
        result = (result << 3) | OctalCharToValue(data[i]);
    }
    return result;
}

static UINT32 ReadOctalU32(HANDLE hFile, SIZE_T count, CpioError* error) {
    char buffer[32];
    DWORD bytesRead;
    
    if (!ReadFile(hFile, buffer, (DWORD)count, &bytesRead, NULL) || bytesRead != count) {
        CpioErrorSet(error, CPIO_ERROR_IO, "Failed to read octal value");
        return 0;
    }
    
    return ParseOctalU32(buffer, count);
}

static UINT64 ReadOctalU64(HANDLE hFile, SIZE_T count, CpioError* error) {
    char buffer[32];
    DWORD bytesRead;
    
    if (!ReadFile(hFile, buffer, (DWORD)count, &bytesRead, NULL) || bytesRead != count) {
        CpioErrorSet(error, CPIO_ERROR_IO, "Failed to read octal value");
        return 0;
    }
    
    return ParseOctalU64(buffer, count);
}

static BOOL WriteOctal(HANDLE hFile, UINT64 value, SIZE_T size, CpioError* error) {
    char buffer[32];
    
    for (int i = (int)size - 1; i >= 0; i--) {
        buffer[i] = '0' + (char)(value & 7);
        value >>= 3;
    }
    
    DWORD bytesWritten;
    if (!WriteFile(hFile, buffer, (DWORD)size, &bytesWritten, NULL) || bytesWritten != size) {
        CpioErrorSet(error, CPIO_ERROR_IO, "Failed to write octal value");
        return FALSE;
    }
    
    return TRUE;
}

BOOL CpioOdcHeaderRead(HANDLE hFile, CpioOdcHeader* header, CpioError* error) {
    if (!header) {
        CpioErrorSet(error, CPIO_ERROR_INVALID_PARAMETER, "NULL header");
        return FALSE;
    }
    
    CpioZeroMemory(header, sizeof(CpioOdcHeader));
    
    header->dev = ReadOctalU32(hFile, 6, error);
    header->inode = ReadOctalU32(hFile, 6, error);
    header->mode = ReadOctalU32(hFile, 6, error);
    header->uid = ReadOctalU32(hFile, 6, error);
    header->gid = ReadOctalU32(hFile, 6, error);
    header->nlink = ReadOctalU32(hFile, 6, error);
    header->rdev = ReadOctalU32(hFile, 6, error);
    header->mtime = ReadOctalU32(hFile, 11, error);
    
    UINT32 nameLength = ReadOctalU32(hFile, 6, error);
    header->fileSize = ReadOctalU64(hFile, 11, error);
    
    if (nameLength == 0 || nameLength > CPIO_MAX_NAME_LENGTH) {
        CpioErrorSet(error, CPIO_ERROR_BAD_HEADER, "Invalid name length");
        return FALSE;
    }
    
    DWORD bytesRead;
    if (!ReadFile(hFile, header->name, nameLength, &bytesRead, NULL) || bytesRead != nameLength) {
        CpioErrorSet(error, CPIO_ERROR_IO, "Failed to read filename");
        return FALSE;
    }
    
    header->name[nameLength - 1] = '\0';
    
    return TRUE;
}

UINT64 CpioOdcHeaderWrite(HANDLE hFile, const CpioOdcHeader* header, CpioError* error) {
    if (!header) {
        CpioErrorSet(error, CPIO_ERROR_INVALID_PARAMETER, "NULL header");
        return 0;
    }
    
    DWORD bytesWritten;
    
    if (!WriteFile(hFile, "070707", 6, &bytesWritten, NULL) || bytesWritten != 6) {
        CpioErrorSet(error, CPIO_ERROR_IO, "Failed to write magic");
        return 0;
    }
    
    DWORD totalWritten = 6;
    
    if (!WriteOctal(hFile, header->dev, 6, error)) return 0;
    if (!WriteOctal(hFile, header->inode, 6, error)) return 0;
    if (!WriteOctal(hFile, header->mode, 6, error)) return 0;
    if (!WriteOctal(hFile, header->uid, 6, error)) return 0;
    if (!WriteOctal(hFile, header->gid, 6, error)) return 0;
    if (!WriteOctal(hFile, header->nlink, 6, error)) return 0;
    if (!WriteOctal(hFile, header->rdev, 6, error)) return 0;
    if (!WriteOctal(hFile, header->mtime, 11, error)) return 0;
    
    UINT32 nameLen = (UINT32)CpioStringLength(header->name) + 1;
    if (!WriteOctal(hFile, nameLen, 6, error)) return 0;
    if (!WriteOctal(hFile, header->fileSize, 11, error)) return 0;
    
    totalWritten += 6 * 7 + 11 * 2;
    
    if (!WriteFile(hFile, header->name, nameLen - 1, &bytesWritten, NULL)) {
        CpioErrorSet(error, CPIO_ERROR_IO, "Failed to write filename");
        return 0;
    }
    totalWritten += bytesWritten;
    
    char nul = '\0';
    if (!WriteFile(hFile, &nul, 1, &bytesWritten, NULL)) {
        CpioErrorSet(error, CPIO_ERROR_IO, "Failed to write filename NUL");
        return 0;
    }
    totalWritten += 1;
    
    return totalWritten;
}

CpioOdcReader* CpioOdcReaderCreate(HANDLE hFile, BOOL takeOwnership) {
    if (hFile == INVALID_HANDLE_VALUE) return NULL;
    
    CpioOdcReader* reader = (CpioOdcReader*)CpioAlloc(sizeof(CpioOdcReader));
    if (!reader) return NULL;
    
    reader->hFile = hFile;
    reader->ownsHandle = takeOwnership;
    reader->currentEntrySize = 0;
    reader->currentEntryRead = 0;
    reader->seenTrailer = FALSE;
    reader->firstEntry = TRUE;
    
    return reader;
}

void CpioOdcReaderDestroy(CpioOdcReader* reader) {
    if (!reader) return;
    
    if (reader->ownsHandle && reader->hFile != INVALID_HANDLE_VALUE) {
        CloseHandle(reader->hFile);
    }
    
    CpioFree(reader);
}

BOOL CpioOdcReaderReadNext(CpioOdcReader* reader, CpioOdcHeader* header, CpioError* error) {
    if (!reader || !header) {
        CpioErrorSet(error, CPIO_ERROR_INVALID_PARAMETER, "NULL parameter");
        return FALSE;
    }
    
    CpioOdcReaderFinish(reader, error);
    
    if (reader->seenTrailer) {
        return FALSE;
    }
    
    if (!reader->firstEntry) {
        char magic[6];
        DWORD bytesRead;
        
        if (!ReadFile(reader->hFile, magic, 6, &bytesRead, NULL)) {
            if (GetLastError() == ERROR_HANDLE_EOF) {
                return FALSE;
            }
            CpioErrorSet(error, CPIO_ERROR_IO, "Failed to read magic");
            return FALSE;
        }
        
        if (bytesRead == 0) {
            return FALSE;
        }
        
        if (bytesRead != 6 || CpioCompareMemory(magic, "070707", 6) != 0) {
            CpioErrorSet(error, CPIO_ERROR_BAD_MAGIC, "Invalid magic number");
            return FALSE;
        }
    }
    reader->firstEntry = FALSE;
    
    if (!CpioOdcHeaderRead(reader->hFile, header, error)) {
        return FALSE;
    }
    
    if (CpioStringCompare(header->name, "TRAILER!!!") == 0) {
        reader->seenTrailer = TRUE;
        return FALSE;
    }
    
    reader->currentEntrySize = header->fileSize;
    reader->currentEntryRead = 0;
    
    return TRUE;
}

DWORD CpioOdcReaderRead(CpioOdcReader* reader, void* buffer, DWORD bufferSize, CpioError* error) {
    if (!reader || !buffer) {
        CpioErrorSet(error, CPIO_ERROR_INVALID_PARAMETER, "NULL parameter");
        return 0;
    }
    
    if (reader->currentEntryRead >= reader->currentEntrySize) {
        return 0;
    }
    
    DWORD toRead = bufferSize;
    UINT64 remaining = reader->currentEntrySize - reader->currentEntryRead;
    
    if (toRead > remaining) {
        toRead = (DWORD)remaining;
    }
    
    DWORD bytesRead;
    if (!ReadFile(reader->hFile, buffer, toRead, &bytesRead, NULL)) {
        CpioErrorSet(error, CPIO_ERROR_IO, "Failed to read entry data");
        return 0;
    }
    
    reader->currentEntryRead += bytesRead;
    return bytesRead;
}

BOOL CpioOdcReaderFinish(CpioOdcReader* reader, CpioError* error) {
    if (!reader) return FALSE;
    
    if (reader->currentEntrySize == 0 || reader->currentEntryRead >= reader->currentEntrySize) {
        reader->currentEntrySize = 0;
        reader->currentEntryRead = 0;
        return TRUE;
    }
    
    UINT64 remaining = reader->currentEntrySize - reader->currentEntryRead;
    
    if (remaining > 0) {
        LARGE_INTEGER dist;
        dist.QuadPart = remaining;
        
        if (!SetFilePointerEx(reader->hFile, dist, NULL, FILE_CURRENT)) {
            char buffer[8192];
            while (remaining > 0) {
                DWORD toRead = remaining > sizeof(buffer) ? sizeof(buffer) : (DWORD)remaining;
                DWORD bytesRead;
                
                if (!ReadFile(reader->hFile, buffer, toRead, &bytesRead, NULL)) {
                    CpioErrorSet(error, CPIO_ERROR_IO, "Failed to skip entry data");
                    return FALSE;
                }
                
                remaining -= bytesRead;
                if (bytesRead == 0) break;
            }
        }
    }
    
    reader->currentEntrySize = 0;
    reader->currentEntryRead = 0;
    return TRUE;
}

BOOL CpioOdcReaderIsAtEnd(const CpioOdcReader* reader) {
    return reader ? reader->seenTrailer : TRUE;
}

CpioOdcBuilder* CpioOdcBuilderCreate(HANDLE hFile, BOOL takeOwnership) {
    if (hFile == INVALID_HANDLE_VALUE) return NULL;
    
    CpioOdcBuilder* builder = (CpioOdcBuilder*)CpioAlloc(sizeof(CpioOdcBuilder));
    if (!builder) return NULL;
    
    builder->hFile = hFile;
    builder->ownsHandle = takeOwnership;
    builder->defaultUid = 0;
    builder->defaultGid = 0;
    builder->defaultMtime = CpioGetCurrentUnixTime();
    builder->defaultModeFile = CPIO_S_IFREG | CPIO_S_IRUSR | CPIO_S_IWUSR | CPIO_S_IRGRP | CPIO_S_IROTH;
    builder->defaultModeDir = CPIO_S_IFDIR | CPIO_S_IRUSR | CPIO_S_IWUSR | CPIO_S_IXUSR | 
                              CPIO_S_IRGRP | CPIO_S_IXGRP | CPIO_S_IROTH | CPIO_S_IXOTH;
    builder->autoWriteDirs = TRUE;
    builder->seenDirs = CpioHashSetCreate();
    builder->entryCount = 0;
    builder->finished = FALSE;
    
    if (!builder->seenDirs) {
        CpioFree(builder);
        return NULL;
    }
    
    return builder;
}

void CpioOdcBuilderDestroy(CpioOdcBuilder* builder) {
    if (!builder) return;
    
    if (builder->seenDirs) {
        CpioHashSetDestroy(builder->seenDirs);
    }
    
    if (builder->ownsHandle && builder->hFile != INVALID_HANDLE_VALUE) {
        CloseHandle(builder->hFile);
    }
    
    CpioFree(builder);
}

void CpioOdcBuilderNextHeader(CpioOdcBuilder* builder, CpioOdcHeader* header) {
    if (!builder || !header) return;
    
    CpioZeroMemory(header, sizeof(CpioOdcHeader));
    
    header->dev = 0;
    header->inode = builder->entryCount++;
    header->mode = builder->defaultModeFile;
    header->uid = builder->defaultUid;
    header->gid = builder->defaultGid;
    header->nlink = 1;
    header->rdev = 0;
    header->mtime = builder->defaultMtime;
    header->fileSize = 0;
    header->name[0] = '\0';
}

static UINT64 EmitParentDirectoriesOdc(CpioOdcBuilder* builder, const char* filePath, CpioError* error) {
    if (!builder->autoWriteDirs) return 0;
    
    char normalized[CPIO_MAX_NAME_LENGTH];
    if (!CpioNormalizeArchivePath(filePath, normalized, sizeof(normalized))) {
        return 0;
    }
    
    UINT64 totalWritten = 0;
    
    char dirPath[CPIO_MAX_NAME_LENGTH] = {0};
    SIZE_T dirIdx = 0;
    
    SIZE_T i = 0;
    char currentPart[256];
    SIZE_T partIdx = 0;
    
    while (normalized[i]) {
        if (normalized[i] == '/') {
            if (partIdx > 0) {
                currentPart[partIdx] = '\0';
                
                if (dirIdx > 0) {
                    dirPath[dirIdx++] = '/';
                }
                
                for (SIZE_T j = 0; j < partIdx; j++) {
                    dirPath[dirIdx++] = currentPart[j];
                }
                dirPath[dirIdx] = '\0';
                
                if (!CpioHashSetContains(builder->seenDirs, dirPath)) {
                    CpioOdcHeader header;
                    CpioOdcBuilderNextHeader(builder, &header);
                    header.mode = builder->defaultModeDir;
                    header.fileSize = 0;
                    
                    SIZE_T len = CpioStringLength(dirPath);
                    for (SIZE_T j = 0; j < len && j < CPIO_MAX_NAME_LENGTH - 1; j++) {
                        header.name[j] = dirPath[j];
                    }
                    header.name[len] = '\0';
                    
                    UINT64 w = CpioOdcHeaderWrite(builder->hFile, &header, error);
                    if (w == 0) return 0;
                    totalWritten += w;
                    
                    CpioHashSetInsert(builder->seenDirs, dirPath);
                }
                
                partIdx = 0;
            }
        } else {
            if (partIdx < 255) {
                currentPart[partIdx++] = normalized[i];
            }
        }
        i++;
    }
    
    return totalWritten;
}

UINT64 CpioOdcBuilderAppendFileFromPath(CpioOdcBuilder* builder, const char* archivePath,
                                         const WCHAR* filePath, CpioError* error) {
    if (!builder || !archivePath || !filePath) {
        CpioErrorSet(error, CPIO_ERROR_INVALID_PARAMETER, "NULL parameter");
        return 0;
    }
    
    char normalized[CPIO_MAX_NAME_LENGTH];
    if (!CpioNormalizeArchivePath(archivePath, normalized, sizeof(normalized))) {
        CpioErrorSet(error, CPIO_ERROR_INVALID_PARAMETER, "Path normalization failed");
        return 0;
    }
    
    HANDLE hSourceFile = CreateFileW(filePath, GENERIC_READ, FILE_SHARE_READ,
                                      NULL, OPEN_EXISTING, FILE_ATTRIBUTE_NORMAL, NULL);
    
    if (hSourceFile == INVALID_HANDLE_VALUE) {
        CpioErrorSet(error, CPIO_ERROR_IO, "Failed to open source file");
        return 0;
    }
    
    LARGE_INTEGER fileSize;
    if (!GetFileSizeEx(hSourceFile, &fileSize)) {
        CloseHandle(hSourceFile);
        CpioErrorSet(error, CPIO_ERROR_IO, "Failed to get file size");
        return 0;
    }
    
    UINT64 totalWritten = EmitParentDirectoriesOdc(builder, normalized, error);
    
    CpioOdcHeader header;
    CpioOdcBuilderNextHeader(builder, &header);
    
    SIZE_T len = CpioStringLength(normalized);
    for (SIZE_T i = 0; i < len && i < CPIO_MAX_NAME_LENGTH - 1; i++) {
        header.name[i] = normalized[i];
    }
    header.name[len] = '\0';
    header.fileSize = fileSize.QuadPart;
    
    UINT64 headerWritten = CpioOdcHeaderWrite(builder->hFile, &header, error);
    if (headerWritten == 0) {
        CloseHandle(hSourceFile);
        return 0;
    }
    totalWritten += headerWritten;
    
    char buffer[8192];
    UINT64 totalCopied = 0;
    
    while (totalCopied < (UINT64)fileSize.QuadPart) {
        DWORD toRead = sizeof(buffer);
        UINT64 remaining = fileSize.QuadPart - totalCopied;
        
        if (toRead > remaining) {
            toRead = (DWORD)remaining;
        }
        
        DWORD bytesRead;
        if (!ReadFile(hSourceFile, buffer, toRead, &bytesRead, NULL)) {
            CloseHandle(hSourceFile);
            CpioErrorSet(error, CPIO_ERROR_IO, "Failed to read source file");
            return 0;
        }
        
        if (bytesRead == 0) break;
        
        DWORD bytesWritten;
        if (!WriteFile(builder->hFile, buffer, bytesRead, &bytesWritten, NULL) ||
            bytesWritten != bytesRead) {
            CloseHandle(hSourceFile);
            CpioErrorSet(error, CPIO_ERROR_IO, "Failed to write file data");
            return 0;
        }
        
        totalCopied += bytesRead;
    }
    
    CloseHandle(hSourceFile);
    totalWritten += totalCopied;
    
    return totalWritten;
}

UINT64 CpioOdcBuilderEmitRootDirectory(CpioOdcBuilder* builder, CpioError* error) {
    if (!builder) {
        CpioErrorSet(error, CPIO_ERROR_INVALID_PARAMETER, "NULL builder");
        return 0;
    }
    
    if (CpioHashSetContains(builder->seenDirs, ".")) {
        return 0;
    }
    
    CpioOdcHeader header;
    CpioOdcBuilderNextHeader(builder, &header);
    header.mode = builder->defaultModeDir;
    header.fileSize = 0;
    header.name[0] = '.';
    header.name[1] = '\0';
    
    UINT64 written = CpioOdcHeaderWrite(builder->hFile, &header, error);
    if (written > 0) {
        CpioHashSetInsert(builder->seenDirs, ".");
    }
    
    return written;
}

UINT64 CpioOdcBuilderFinish(CpioOdcBuilder* builder, CpioError* error) {
    if (!builder) {
        CpioErrorSet(error, CPIO_ERROR_INVALID_PARAMETER, "NULL builder");
        return 0;
    }
    
    if (builder->finished) {
        return 0;
    }
    
    CpioOdcHeader trailer;
    CpioOdcBuilderNextHeader(builder, &trailer);
    
    const char* trailerName = "TRAILER!!!";
    SIZE_T i = 0;
    while (trailerName[i]) {
        trailer.name[i] = trailerName[i];
        i++;
    }
    trailer.name[i] = '\0';
    
    UINT64 written = CpioOdcHeaderWrite(builder->hFile, &trailer, error);
    builder->finished = TRUE;
    
    return written;
}