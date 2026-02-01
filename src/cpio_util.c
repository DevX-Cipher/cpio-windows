#include "cpio.h"

void* CpioAlloc(SIZE_T size) {
    return HeapAlloc(GetProcessHeap(), HEAP_ZERO_MEMORY, size);
}

void CpioFree(void* ptr) {
    if (ptr) {
        HeapFree(GetProcessHeap(), 0, ptr);
    }
}

void* CpioRealloc(void* ptr, SIZE_T newSize) {
    if (!ptr) {
        return CpioAlloc(newSize);
    }
    return HeapReAlloc(GetProcessHeap(), HEAP_ZERO_MEMORY, ptr, newSize);
}

void CpioZeroMemory(void* ptr, SIZE_T size) {
    char* p = (char*)ptr;
    while (size--) {
        *p++ = 0;
    }
}

void CpioCopyMemory(void* dest, const void* src, SIZE_T size) {
    char* d = (char*)dest;
    const char* s = (const char*)src;
    while (size--) {
        *d++ = *s++;
    }
}

int CpioCompareMemory(const void* ptr1, const void* ptr2, SIZE_T size) {
    const unsigned char* p1 = (const unsigned char*)ptr1;
    const unsigned char* p2 = (const unsigned char*)ptr2;
    
    while (size--) {
        if (*p1 != *p2) {
            return *p1 - *p2;
        }
        p1++;
        p2++;
    }
    return 0;
}

SIZE_T CpioStringLength(const char* str) {
    SIZE_T len = 0;
    if (!str) return 0;
    while (*str++) len++;
    return len;
}

int CpioStringCompare(const char* s1, const char* s2) {
    if (!s1 || !s2) return (s1 == s2) ? 0 : (s1 ? 1 : -1);
    
    while (*s1 && (*s1 == *s2)) {
        s1++;
        s2++;
    }
    return *(unsigned char*)s1 - *(unsigned char*)s2;
}

BOOL CpioStringStartsWith(const char* str, const char* prefix) {
    if (!str || !prefix) return FALSE;
    
    while (*prefix) {
        if (*str != *prefix) return FALSE;
        str++;
        prefix++;
    }
    return TRUE;
}

CpioString* CpioStringCreate(void) {
    CpioString* str = (CpioString*)CpioAlloc(sizeof(CpioString));
    if (!str) return NULL;
    
    str->capacity = 64;
    str->data = (char*)CpioAlloc(str->capacity);
    if (!str->data) {
        CpioFree(str);
        return NULL;
    }
    
    str->length = 0;
    str->data[0] = '\0';
    return str;
}

void CpioStringDestroy(CpioString* str) {
    if (!str) return;
    if (str->data) CpioFree(str->data);
    CpioFree(str);
}

BOOL CpioStringAppend(CpioString* str, const char* data, SIZE_T len) {
    if (!str || !data) return FALSE;
    
    SIZE_T newLen = str->length + len;
    if (newLen + 1 > str->capacity) {
        SIZE_T newCap = str->capacity;
        while (newCap <= newLen) newCap *= 2;
        
        char* newData = (char*)CpioRealloc(str->data, newCap);
        if (!newData) return FALSE;
        
        str->data = newData;
        str->capacity = newCap;
    }
    
    CpioCopyMemory(str->data + str->length, data, len);
    str->length = newLen;
    str->data[str->length] = '\0';
    return TRUE;
}

BOOL CpioStringAppendChar(CpioString* str, char c) {
    return CpioStringAppend(str, &c, 1);
}

BOOL CpioStringSet(CpioString* str, const char* data, SIZE_T len) {
    if (!str) return FALSE;
    str->length = 0;
    str->data[0] = '\0';
    return CpioStringAppend(str, data, len);
}

void CpioStringClear(CpioString* str) {
    if (!str) return;
    str->length = 0;
    if (str->data) str->data[0] = '\0';
}

CpioStringList* CpioStringListCreate(void) {
    CpioStringList* list = (CpioStringList*)CpioAlloc(sizeof(CpioStringList));
    if (!list) return NULL;
    
    list->capacity = 16;
    list->items = (char**)CpioAlloc(sizeof(char*) * list->capacity);
    if (!list->items) {
        CpioFree(list);
        return NULL;
    }
    
    list->count = 0;
    return list;
}

void CpioStringListDestroy(CpioStringList* list) {
    if (!list) return;
    
    if (list->items) {
        for (SIZE_T i = 0; i < list->count; i++) {
            if (list->items[i]) CpioFree(list->items[i]);
        }
        CpioFree(list->items);
    }
    
    CpioFree(list);
}

BOOL CpioStringListAdd(CpioStringList* list, const char* str) {
    if (!list || !str) return FALSE;
    
    if (list->count >= list->capacity) {
        SIZE_T newCap = list->capacity * 2;
        char** newItems = (char**)CpioRealloc(list->items, sizeof(char*) * newCap);
        if (!newItems) return FALSE;
        
        list->items = newItems;
        list->capacity = newCap;
    }
    
    SIZE_T len = CpioStringLength(str);
    char* copy = (char*)CpioAlloc(len + 1);
    if (!copy) return FALSE;
    
    CpioCopyMemory(copy, str, len);
    copy[len] = '\0';
    
    list->items[list->count++] = copy;
    return TRUE;
}

static UINT32 CpioHashString(const char* str) {
    UINT32 hash = 5381;
    int c;
    
    while ((c = *str++)) {
        hash = ((hash << 5) + hash) + c;
    }
    
    return hash % CPIO_HASHSET_SIZE;
}

CpioHashSet* CpioHashSetCreate(void) {
    CpioHashSet* set = (CpioHashSet*)CpioAlloc(sizeof(CpioHashSet));
    if (!set) return NULL;
    
    for (int i = 0; i < CPIO_HASHSET_SIZE; i++) {
        set->buckets[i] = NULL;
    }
    
    return set;
}

void CpioHashSetDestroy(CpioHashSet* set) {
    if (!set) return;
    
    for (int i = 0; i < CPIO_HASHSET_SIZE; i++) {
        CpioHashSetNode* node = set->buckets[i];
        while (node) {
            CpioHashSetNode* next = node->next;
            if (node->key) CpioFree(node->key);
            CpioFree(node);
            node = next;
        }
    }
    
    CpioFree(set);
}

BOOL CpioHashSetContains(CpioHashSet* set, const char* key) {
    if (!set || !key) return FALSE;
    
    UINT32 hash = CpioHashString(key);
    CpioHashSetNode* node = set->buckets[hash];
    
    while (node) {
        if (CpioStringCompare(node->key, key) == 0) {
            return TRUE;
        }
        node = node->next;
    }
    
    return FALSE;
}

BOOL CpioHashSetInsert(CpioHashSet* set, const char* key) {
    if (!set || !key) return FALSE;
    
    if (CpioHashSetContains(set, key)) {
        return TRUE;
    }
    
    UINT32 hash = CpioHashString(key);
    
    CpioHashSetNode* node = (CpioHashSetNode*)CpioAlloc(sizeof(CpioHashSetNode));
    if (!node) return FALSE;
    
    SIZE_T len = CpioStringLength(key);
    node->key = (char*)CpioAlloc(len + 1);
    if (!node->key) {
        CpioFree(node);
        return FALSE;
    }
    
    CpioCopyMemory(node->key, key, len);
    node->key[len] = '\0';
    
    node->next = set->buckets[hash];
    set->buckets[hash] = node;
    
    return TRUE;
}

void CpioErrorSet(CpioError* error, CpioErrorCode code, const char* message) {
    if (!error) return;
    
    error->code = code;
    error->lastError = GetLastError();
    
    if (message) {
        SIZE_T i = 0;
        while (message[i] && i < 255) {
            error->message[i] = message[i];
            i++;
        }
        error->message[i] = '\0';
    } else {
        error->message[0] = '\0';
    }
}

const char* CpioErrorGetMessage(const CpioError* error) {
    return error ? error->message : "Unknown error";
}

WCHAR* CpioStringToWide(const char* str) {
    if (!str) return NULL;
    
    int size = MultiByteToWideChar(CP_UTF8, 0, str, -1, NULL, 0);
    if (size == 0) return NULL;
    
    WCHAR* result = (WCHAR*)CpioAlloc(size * sizeof(WCHAR));
    if (!result) return NULL;
    
    MultiByteToWideChar(CP_UTF8, 0, str, -1, result, size);
    return result;
}

char* CpioWideToString(const WCHAR* wstr) {
    if (!wstr) return NULL;
    
    int size = WideCharToMultiByte(CP_UTF8, 0, wstr, -1, NULL, 0, NULL, NULL);
    if (size == 0) return NULL;
    
    char* result = (char*)CpioAlloc(size);
    if (!result) return NULL;
    
    WideCharToMultiByte(CP_UTF8, 0, wstr, -1, result, size, NULL, NULL);
    return result;
}

UINT32 CpioGetCurrentUnixTime(void) {
    FILETIME ft;
    GetSystemTimeAsFileTime(&ft);
    
    ULARGE_INTEGER uli;
    uli.LowPart = ft.dwLowDateTime;
    uli.HighPart = ft.dwHighDateTime;
    
    return (UINT32)((uli.QuadPart / 10000000ULL) - 11644473600ULL);
}

BOOL CpioNormalizeArchivePath(const char* path, char* output, SIZE_T outputSize) {
    if (!path || !output || outputSize < 3) return FALSE;
    
    SIZE_T outIdx = 0;
    SIZE_T inIdx = 0;
    
    char temp[CPIO_MAX_NAME_LENGTH];
    SIZE_T tempIdx = 0;
    
    while (path[inIdx] && tempIdx < CPIO_MAX_NAME_LENGTH - 1) {
        if (path[inIdx] == '\\') {
            temp[tempIdx++] = '/';
        } else {
            temp[tempIdx++] = path[inIdx];
        }
        inIdx++;
    }
    temp[tempIdx] = '\0';
    
    SIZE_T start = 0;
    while (temp[start] == '/') start++;
    
    BOOL hasDotPrefix = FALSE;
    if (temp[start] == '.' && temp[start + 1] == '/') {
        hasDotPrefix = TRUE;
    }
    
    if (temp[start] == '.' && temp[start + 1] == '\0') {
        if (outputSize < 2) return FALSE;
        output[0] = '.';
        output[1] = '\0';
        return TRUE;
    }
    
    if (!hasDotPrefix) {
        if (outputSize < tempIdx - start + 3) return FALSE;
        output[outIdx++] = '.';
        output[outIdx++] = '/';
    } else {
        if (outputSize < tempIdx - start + 1) return FALSE;
    }
    
    while (temp[start] && outIdx < outputSize - 1) {
        output[outIdx++] = temp[start++];
    }
    output[outIdx] = '\0';
    
    return TRUE;
}

CpioFormat CpioDetectFormat(HANDLE hFile, CpioError* error) {
    if (hFile == INVALID_HANDLE_VALUE) {
        CpioErrorSet(error, CPIO_ERROR_INVALID_HANDLE, "Invalid file handle");
        return CPIO_FORMAT_UNKNOWN;
    }
    
    char magic[CPIO_MAGIC_SIZE];
    DWORD bytesRead;
    
    if (!ReadFile(hFile, magic, CPIO_MAGIC_SIZE, &bytesRead, NULL)) {
        CpioErrorSet(error, CPIO_ERROR_IO, "Failed to read magic");
        return CPIO_FORMAT_UNKNOWN;
    }
    
    if (bytesRead < CPIO_MAGIC_SIZE) {
        return CPIO_FORMAT_UNKNOWN;
    }
    
    if (CpioCompareMemory(magic, "070701", 6) == 0) {
        return CPIO_FORMAT_NEWC;
    } else if (CpioCompareMemory(magic, "070707", 6) == 0) {
        return CPIO_FORMAT_ODC;
    }
    
    return CPIO_FORMAT_UNKNOWN;
}

void* memcpy(void* dest, const void* src, SIZE_T size) {
  return CpioCopyMemory(dest, src, size), dest;
}

void* memset(void* ptr, int value, SIZE_T size) {
  char* p = (char*)ptr;
  while (size--) {
    *p++ = (char)value;
  }
  return ptr;
}