
#include "cpio.h"

static UINT32 HexCharToValue(char c) {
    if (c >= '0' && c <= '9') return c - '0';
    if (c >= 'a' && c <= 'f') return c - 'a' + 10;
    if (c >= 'A' && c <= 'F') return c - 'A' + 10;
    return 0;
}

static UINT32 ParseHexU32(const char* data, SIZE_T length) {
    UINT32 result = 0;
    for (SIZE_T i = 0; i < length; i++) {
        result = (result << 4) | HexCharToValue(data[i]);
    }
    return result;
}

static UINT64 ParseHexU64(const char* data, SIZE_T length) {
    UINT64 result = 0;
    for (SIZE_T i = 0; i < length; i++) {
        result = (result << 4) | HexCharToValue(data[i]);
    }
    return result;
}

static UINT32 ReadHexU32(HANDLE hFile, SIZE_T count, CpioError* error) {
    char buffer[32];
    DWORD bytesRead;
    
    if (!ReadFile(hFile, buffer, (DWORD)count, &bytesRead, NULL) || bytesRead != count) {
        CpioErrorSet(error, CPIO_ERROR_IO, "Failed to read hex value");
        return 0;
    }
    
    return ParseHexU32(buffer, count);
}

static UINT64 ReadHexU64(HANDLE hFile, SIZE_T count, CpioError* error) {
    char buffer[32];
    DWORD bytesRead;
    
    if (!ReadFile(hFile, buffer, (DWORD)count, &bytesRead, NULL) || bytesRead != count) {
        CpioErrorSet(error, CPIO_ERROR_IO, "Failed to read hex value");
        return 0;
    }
    
    return ParseHexU64(buffer, count);
}

static BOOL WriteHex(HANDLE hFile, UINT64 value, SIZE_T width, CpioError* error) {
    char buffer[32];
    char hexChars[] = "0123456789abcdef";
    
    for (int i = (int)width - 1; i >= 0; i--) {
        buffer[i] = hexChars[value & 0xF];
        value >>= 4;
    }
    
    DWORD bytesWritten;
    if (!WriteFile(hFile, buffer, (DWORD)width, &bytesWritten, NULL) || bytesWritten != width) {
        CpioErrorSet(error, CPIO_ERROR_IO, "Failed to write hex value");
        return FALSE;
    }
    
    return TRUE;
}

static BOOL WritePadding(HANDLE hFile, SIZE_T count, CpioError* error) {
    if (count == 0) return TRUE;
    
    char zeros[4] = {0, 0, 0, 0};
    DWORD bytesWritten;
    
    if (!WriteFile(hFile, zeros, (DWORD)count, &bytesWritten, NULL) || bytesWritten != count) {
        CpioErrorSet(error, CPIO_ERROR_IO, "Failed to write padding");
        return FALSE;
    }
    
    return TRUE;
}

BOOL CpioNewcHeaderRead(HANDLE hFile, CpioNewcHeader* header, CpioError* error) {
    if (!header) {
        CpioErrorSet(error, CPIO_ERROR_INVALID_PARAMETER, "NULL header");
        return FALSE;
    }
    
    CpioZeroMemory(header, sizeof(CpioNewcHeader));
    
    header->inode = ReadHexU32(hFile, 8, error);
    header->mode = ReadHexU32(hFile, 8, error);
    header->uid = ReadHexU32(hFile, 8, error);
    header->gid = ReadHexU32(hFile, 8, error);
    header->nlink = ReadHexU32(hFile, 8, error);
    header->mtime = ReadHexU32(hFile, 8, error);
    header->fileSize = ReadHexU64(hFile, 8, error);
    header->devMajor = ReadHexU32(hFile, 8, error);
    header->devMinor = ReadHexU32(hFile, 8, error);
    header->rdevMajor = ReadHexU32(hFile, 8, error);
    header->rdevMinor = ReadHexU32(hFile, 8, error);
    
    UINT32 nameLength = ReadHexU32(hFile, 8, error);
    header->checksum = ReadHexU32(hFile, 8, error);
    
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
    
    SIZE_T totalBeforePad = 110 + nameLength;
    SIZE_T padLen = (4 - (totalBeforePad % 4)) % 4;
    
    if (padLen > 0) {
        char pad[4];
        if (!ReadFile(hFile, pad, (DWORD)padLen, &bytesRead, NULL) || bytesRead != padLen) {
            CpioErrorSet(error, CPIO_ERROR_IO, "Failed to read padding");
            return FALSE;
        }
    }
    
    return TRUE;
}

UINT64 CpioNewcHeaderWrite(HANDLE hFile, const CpioNewcHeader* header, CpioError* error) {
    if (!header) {
        CpioErrorSet(error, CPIO_ERROR_INVALID_PARAMETER, "NULL header");
        return 0;
    }
    
    DWORD bytesWritten;
    
    if (!WriteFile(hFile, "070701", 6, &bytesWritten, NULL) || bytesWritten != 6) {
        CpioErrorSet(error, CPIO_ERROR_IO, "Failed to write magic");
        return 0;
    }
    
    UINT32 nameSize = (UINT32)CpioStringLength(header->name) + 1;
    
    if (!WriteHex(hFile, header->inode, 8, error)) return 0;
    if (!WriteHex(hFile, header->mode, 8, error)) return 0;
    if (!WriteHex(hFile, header->uid, 8, error)) return 0;
    if (!WriteHex(hFile, header->gid, 8, error)) return 0;
    if (!WriteHex(hFile, header->nlink, 8, error)) return 0;
    if (!WriteHex(hFile, header->mtime, 8, error)) return 0;
    if (!WriteHex(hFile, header->fileSize, 8, error)) return 0;
    if (!WriteHex(hFile, header->devMajor, 8, error)) return 0;
    if (!WriteHex(hFile, header->devMinor, 8, error)) return 0;
    if (!WriteHex(hFile, header->rdevMajor, 8, error)) return 0;
    if (!WriteHex(hFile, header->rdevMinor, 8, error)) return 0;
    if (!WriteHex(hFile, nameSize, 8, error)) return 0;
    if (!WriteHex(hFile, header->checksum, 8, error)) return 0;
    
    if (!WriteFile(hFile, header->name, nameSize - 1, &bytesWritten, NULL)) {
        CpioErrorSet(error, CPIO_ERROR_IO, "Failed to write filename");
        return 0;
    }
    
    char nul = '\0';
    if (!WriteFile(hFile, &nul, 1, &bytesWritten, NULL)) {
        CpioErrorSet(error, CPIO_ERROR_IO, "Failed to write filename NUL");
        return 0;
    }
    
    SIZE_T totalBeforePad = 110 + nameSize;
    SIZE_T padLen = (4 - (totalBeforePad % 4)) % 4;
    if (!WritePadding(hFile, padLen, error)) return 0;
    
    return totalBeforePad + padLen;
}

CpioNewcReader* CpioNewcReaderCreate(HANDLE hFile, BOOL takeOwnership) {
    if (hFile == INVALID_HANDLE_VALUE) return NULL;
    
    CpioNewcReader* reader = (CpioNewcReader*)CpioAlloc(sizeof(CpioNewcReader));
    if (!reader) return NULL;
    
    reader->hFile = hFile;
    reader->ownsHandle = takeOwnership;
    reader->currentEntrySize = 0;
    reader->currentEntryRead = 0;
    reader->entryDataPad = 0;
    reader->seenTrailer = FALSE;
    reader->firstEntry = TRUE;
    
    return reader;
}

void CpioNewcReaderDestroy(CpioNewcReader* reader) {
    if (!reader) return;
    
    if (reader->ownsHandle && reader->hFile != INVALID_HANDLE_VALUE) {
        CloseHandle(reader->hFile);
    }
    
    CpioFree(reader);
}

BOOL CpioNewcReaderReadNext(CpioNewcReader* reader, CpioNewcHeader* header, CpioError* error) {
    if (!reader || !header) {
        CpioErrorSet(error, CPIO_ERROR_INVALID_PARAMETER, "NULL parameter");
        return FALSE;
    }
    
    CpioNewcReaderFinish(reader, error);
    
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
        
        if (bytesRead != 6 || CpioCompareMemory(magic, "070701", 6) != 0) {
            CpioErrorSet(error, CPIO_ERROR_BAD_MAGIC, "Invalid magic number");
            return FALSE;
        }
    }
    reader->firstEntry = FALSE;
    
    if (!CpioNewcHeaderRead(reader->hFile, header, error)) {
        return FALSE;
    }
    
    if (CpioStringCompare(header->name, "TRAILER!!!") == 0) {
        reader->seenTrailer = TRUE;
        return FALSE;
    }
    
    reader->currentEntrySize = header->fileSize;
    reader->currentEntryRead = 0;
    reader->entryDataPad = (SIZE_T)((4 - (reader->currentEntrySize % 4)) % 4);
    
    return TRUE;
}

DWORD CpioNewcReaderRead(CpioNewcReader* reader, void* buffer, DWORD bufferSize, CpioError* error) {
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

BOOL CpioNewcReaderFinish(CpioNewcReader* reader, CpioError* error) {
    if (!reader) return FALSE;
    
    if (reader->currentEntrySize == 0 || reader->currentEntryRead >= reader->currentEntrySize) {
        if (reader->entryDataPad > 0) {
            char pad[4];
            DWORD bytesRead;
            ReadFile(reader->hFile, pad, (DWORD)reader->entryDataPad, &bytesRead, NULL);
            reader->entryDataPad = 0;
        }
        
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
    
    if (reader->entryDataPad > 0) {
        char pad[4];
        DWORD bytesRead;
        ReadFile(reader->hFile, pad, (DWORD)reader->entryDataPad, &bytesRead, NULL);
        reader->entryDataPad = 0;
    }
    
    reader->currentEntrySize = 0;
    reader->currentEntryRead = 0;
    return TRUE;
}

BOOL CpioNewcReaderIsAtEnd(const CpioNewcReader* reader) {
    return reader ? reader->seenTrailer : TRUE;
}

CpioNewcBuilder* CpioNewcBuilderCreate(HANDLE hFile, BOOL takeOwnership) {
    if (hFile == INVALID_HANDLE_VALUE) return NULL;
    
    CpioNewcBuilder* builder = (CpioNewcBuilder*)CpioAlloc(sizeof(CpioNewcBuilder));
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

void CpioNewcBuilderDestroy(CpioNewcBuilder* builder) {
    if (!builder) return;
    
    if (builder->seenDirs) {
        CpioHashSetDestroy(builder->seenDirs);
    }
    
    if (builder->ownsHandle && builder->hFile != INVALID_HANDLE_VALUE) {
        CloseHandle(builder->hFile);
    }
    
    CpioFree(builder);
}

void CpioNewcBuilderNextHeader(CpioNewcBuilder* builder, CpioNewcHeader* header) {
    if (!builder || !header) return;
    
    CpioZeroMemory(header, sizeof(CpioNewcHeader));
    
    header->devMajor = 0;
    header->devMinor = 0;
    header->inode = builder->entryCount++;
    header->mode = builder->defaultModeFile;
    header->uid = builder->defaultUid;
    header->gid = builder->defaultGid;
    header->nlink = 1;
    header->mtime = builder->defaultMtime;
    header->rdevMajor = 0;
    header->rdevMinor = 0;
    header->checksum = 0;
    header->fileSize = 0;
    header->name[0] = '\0';
}

static UINT64 EmitParentDirectories(CpioNewcBuilder* builder, const char* filePath, CpioError* error) {
    if (!builder->autoWriteDirs) return 0;
    
    char normalized[CPIO_MAX_NAME_LENGTH];
    if (!CpioNormalizeArchivePath(filePath, normalized, sizeof(normalized))) {
        return 0;
    }
    
    UINT64 totalWritten = 0;
    
    char dirPath[CPIO_MAX_NAME_LENGTH] = {0};
    SIZE_T dirIdx = 0;
    
    SIZE_T i = 0;
    BOOL inPart = FALSE;
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
                    CpioNewcHeader header;
                    CpioNewcBuilderNextHeader(builder, &header);
                    header.mode = builder->defaultModeDir;
                    header.fileSize = 0;
                    
                    SIZE_T len = CpioStringLength(dirPath);
                    for (SIZE_T j = 0; j < len && j < CPIO_MAX_NAME_LENGTH - 1; j++) {
                        header.name[j] = dirPath[j];
                    }
                    header.name[len] = '\0';
                    
                    UINT64 w = CpioNewcHeaderWrite(builder->hFile, &header, error);
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

UINT64 CpioNewcBuilderAppendFileFromPath(CpioNewcBuilder* builder, const char* archivePath,
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
    
    UINT64 totalWritten = EmitParentDirectories(builder, normalized, error);
    
    CpioNewcHeader header;
    CpioNewcBuilderNextHeader(builder, &header);
    
    SIZE_T len = CpioStringLength(normalized);
    for (SIZE_T i = 0; i < len && i < CPIO_MAX_NAME_LENGTH - 1; i++) {
        header.name[i] = normalized[i];
    }
    header.name[len] = '\0';
    header.fileSize = fileSize.QuadPart;
    
    UINT64 headerWritten = CpioNewcHeaderWrite(builder->hFile, &header, error);
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
    
    SIZE_T dataPad = (SIZE_T)((4 - (totalCopied % 4)) % 4);
    if (!WritePadding(builder->hFile, dataPad, error)) {
        return 0;
    }
    totalWritten += dataPad;
    
    return totalWritten;
}

UINT64 CpioNewcBuilderEmitRootDirectory(CpioNewcBuilder* builder, CpioError* error) {
    if (!builder) {
        CpioErrorSet(error, CPIO_ERROR_INVALID_PARAMETER, "NULL builder");
        return 0;
    }
    
    if (CpioHashSetContains(builder->seenDirs, ".")) {
        return 0;
    }
    
    CpioNewcHeader header;
    CpioNewcBuilderNextHeader(builder, &header);
    header.mode = builder->defaultModeDir;
    header.fileSize = 0;
    header.name[0] = '.';
    header.name[1] = '\0';
    
    UINT64 written = CpioNewcHeaderWrite(builder->hFile, &header, error);
    if (written > 0) {
        CpioHashSetInsert(builder->seenDirs, ".");
    }
    
    return written;
}

UINT64 CpioNewcBuilderFinish(CpioNewcBuilder* builder, CpioError* error) {
    if (!builder) {
        CpioErrorSet(error, CPIO_ERROR_INVALID_PARAMETER, "NULL builder");
        return 0;
    }
    
    if (builder->finished) {
        return 0;
    }
    
    CpioNewcHeader trailer;
    CpioNewcBuilderNextHeader(builder, &trailer);
    
    const char* trailerName = "TRAILER!!!";
    SIZE_T i = 0;
    while (trailerName[i]) {
        trailer.name[i] = trailerName[i];
        i++;
    }
    trailer.name[i] = '\0';
    
    trailer.fileSize = 0;
    trailer.mode = 0;
    trailer.nlink = 1;
    
    UINT64 written = CpioNewcHeaderWrite(builder->hFile, &trailer, error);
    builder->finished = TRUE;
    
    return written;
}