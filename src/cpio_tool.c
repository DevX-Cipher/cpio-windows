#include "cpio.h"

static void WriteStdErr(const char* message) {
  HANDLE hStdErr = GetStdHandle(STD_ERROR_HANDLE);
  if (hStdErr != INVALID_HANDLE_VALUE) {
    DWORD len = (DWORD)CpioStringLength(message);
    DWORD written;
    WriteFile(hStdErr, message, len, &written, NULL);
  }
}

static void WriteStdErrLine(const char* message) {
  WriteStdErr(message);
  WriteStdErr("\r\n");
}

static void PrintUsage(void) {
  WriteStdErrLine("Usage:");
  WriteStdErrLine("  Create archive (copy-out):");
  WriteStdErrLine("    IMPORTANT: cd into the directory you want to archive first!");
  WriteStdErrLine("    ");
  WriteStdErrLine("    Correct usage in cmd.exe:");
  WriteStdErrLine("      cd PayloadRoot");
  WriteStdErrLine("      dir /b /s | ..\\cpio -o > ..\\Payload");
  WriteStdErrLine("    ");
  WriteStdErrLine("    From PowerShell (run cmd):");
  WriteStdErrLine("      cmd /c \"cd PayloadRoot && dir /b /s | ..\\cpio -o > ..\\Payload\"");
  WriteStdErrLine("    ");
  WriteStdErrLine("    DO NOT do this (creates absolute paths):");
  WriteStdErrLine("      dir /b /s PayloadRoot | cpio -o > Payload  # WRONG!");
  WriteStdErrLine("");
  WriteStdErrLine("  Extract archive (copy-in):");
  WriteStdErrLine("    cpio -i < archive.cpio");
  WriteStdErrLine("    Get-Content archive.cpio -Raw | cpio -i");
  WriteStdErrLine("");
  WriteStdErrLine("Options:");
  WriteStdErrLine("  -o, --create          Create archive (copy-out mode)");
  WriteStdErrLine("  -i, --extract         Extract archive (copy-in mode)");
  WriteStdErrLine("  --format=newc         Use NewC format (default, required for macOS .pkg)");
  WriteStdErrLine("  --format=odc          Use ODC format");
  WriteStdErrLine("  -v, --verbose         Verbose output");
  WriteStdErrLine("");
  WriteStdErrLine("NOTES:");
  WriteStdErrLine("  - Always cd into the directory you want to archive");
  WriteStdErrLine("  - Use cmd.exe, not PowerShell, for archive creation");
  WriteStdErrLine("  - PowerShell's > uses UTF-16 which corrupts binary archives");
  WriteStdErrLine("  - macOS .pkg payloads MUST use newc format (070701)");
}

static CpioStringList* ReadFilenamesFromStdin(void) {
  CpioStringList* list = CpioStringListCreate();
  if (!list) return NULL;

  HANDLE hStdin = GetStdHandle(STD_INPUT_HANDLE);
  if (hStdin == INVALID_HANDLE_VALUE) {
    CpioStringListDestroy(list);
    return NULL;
  }

  CpioString* line = CpioStringCreate();
  if (!line) {
    CpioStringListDestroy(list);
    return NULL;
  }

  char buffer[1024];
  DWORD bytesRead;

  while (ReadFile(hStdin, buffer, sizeof(buffer), &bytesRead, NULL) && bytesRead > 0) {
    for (DWORD i = 0; i < bytesRead; i++) {
      char c = buffer[i];

      if (c == '\n' || c == '\r') {
        if (line->length > 0) {
          while (line->length > 0 &&
            (line->data[line->length - 1] == ' ' ||
              line->data[line->length - 1] == '\t' ||
              line->data[line->length - 1] == '\r' ||
              line->data[line->length - 1] == '\n')) {
            line->length--;
          }
          line->data[line->length] = '\0';

          if (line->length > 0) {
            CpioStringListAdd(list, line->data);
          }

          CpioStringClear(line);
        }
      }
      else {
        CpioStringAppendChar(line, c);
      }
    }
  }

  if (line->length > 0) {
    while (line->length > 0 &&
      (line->data[line->length - 1] == ' ' ||
        line->data[line->length - 1] == '\t' ||
        line->data[line->length - 1] == '\r' ||
        line->data[line->length - 1] == '\n')) {
      line->length--;
    }
    line->data[line->length] = '\0';

    if (line->length > 0) {
      CpioStringListAdd(list, line->data);
    }
  }

  CpioStringDestroy(line);
  return list;
}

static int CreateArchive(BOOL verbose, BOOL useOdc) {
  HANDLE hStdout = GetStdHandle(STD_OUTPUT_HANDLE);
  if (hStdout == INVALID_HANDLE_VALUE) {
    WriteStdErrLine("Error: Cannot get stdout handle");
    return 1;
  }

  SetFilePointer(hStdout, 0, NULL, FILE_BEGIN);

  if (useOdc) {
    WriteStdErrLine("Warning: ODC format selected. macOS .pkg payloads require newc (070701).");
  }

  CpioStringList* filenames = ReadFilenamesFromStdin();
  if (!filenames) {
    WriteStdErrLine("Error: Failed to read filenames from stdin");
    return 1;
  }

  CpioError error = { 0 };
  int result = 0;

  if (useOdc) {
    CpioOdcBuilder* builder = CpioOdcBuilderCreate(hStdout, FALSE);
    if (!builder) {
      WriteStdErrLine("Error: Failed to create ODC builder");
      CpioStringListDestroy(filenames);
      return 1;
    }

    CpioOdcBuilderEmitRootDirectory(builder, &error);
    if (verbose) WriteStdErrLine("  dir  .");

    for (SIZE_T i = 0; i < filenames->count; i++) {
      const char* filename = filenames->items[i];

      if (CpioStringCompare(filename, "Payload") == 0 ||
        CpioStringCompare(filename, "./Payload") == 0 ||
        CpioStringCompare(filename, ".\\Payload") == 0) {
        continue;
      }

      WCHAR* widePath = CpioStringToWide(filename);
      if (!widePath) continue;

      DWORD attrs = GetFileAttributesW(widePath);
      if (attrs == INVALID_FILE_ATTRIBUTES) {
        WriteStdErr("Warning: Cannot access ");
        WriteStdErrLine(filename);
        CpioFree(widePath);
        continue;
      }

      if (attrs & FILE_ATTRIBUTE_DIRECTORY) {
        if (verbose) {
          WriteStdErr("  skip dir  ");
          WriteStdErrLine(filename);
        }
        CpioFree(widePath);
        continue;
      }

      UINT64 written = CpioOdcBuilderAppendFileFromPath(builder, filename, widePath, &error);
      if (written == 0) {
        WriteStdErr("Warning: Cannot add ");
        WriteStdErr(filename);
        WriteStdErr(": ");
        WriteStdErrLine(error.message);
      }
      else if (verbose) {
        WriteStdErr("  file ");
        WriteStdErrLine(filename);
      }

      CpioFree(widePath);
    }

    CpioOdcBuilderFinish(builder, &error);
    CpioOdcBuilderDestroy(builder);

  }
  else {
    CpioNewcBuilder* builder = CpioNewcBuilderCreate(hStdout, FALSE);
    if (!builder) {
      WriteStdErrLine("Error: Failed to create NewC builder");
      CpioStringListDestroy(filenames);
      return 1;
    }

    CpioNewcBuilderEmitRootDirectory(builder, &error);
    if (verbose) WriteStdErrLine("  dir  .");

    for (SIZE_T i = 0; i < filenames->count; i++) {
      const char* filename = filenames->items[i];

      if (CpioStringCompare(filename, "Payload") == 0 ||
        CpioStringCompare(filename, "./Payload") == 0 ||
        CpioStringCompare(filename, ".\\Payload") == 0) {
        continue;
      }

      WCHAR* widePath = CpioStringToWide(filename);
      if (!widePath) continue;

      DWORD attrs = GetFileAttributesW(widePath);
      if (attrs == INVALID_FILE_ATTRIBUTES) {
        WriteStdErr("Warning: Cannot access ");
        WriteStdErrLine(filename);
        CpioFree(widePath);
        continue;
      }

      if (attrs & FILE_ATTRIBUTE_DIRECTORY) {
        if (verbose) {
          WriteStdErr("  skip dir  ");
          WriteStdErrLine(filename);
        }
        CpioFree(widePath);
        continue;
      }

      UINT64 written = CpioNewcBuilderAppendFileFromPath(builder, filename, widePath, &error);
      if (written == 0) {
        WriteStdErr("Warning: Cannot add ");
        WriteStdErr(filename);
        WriteStdErr(": ");
        WriteStdErrLine(error.message);
      }
      else if (verbose) {
        WriteStdErr("  file ");
        WriteStdErrLine(filename);
      }

      CpioFree(widePath);
    }

    CpioNewcBuilderFinish(builder, &error);
    CpioNewcBuilderDestroy(builder);
  }

  CpioStringListDestroy(filenames);
  return result;
}

static int ExtractArchive(BOOL verbose) {
  HANDLE hStdin = GetStdHandle(STD_INPUT_HANDLE);
  if (hStdin == INVALID_HANDLE_VALUE) {
    WriteStdErrLine("Error: Cannot get stdin handle");
    return 1;
  }

  CpioError error = { 0 };
  CpioFormat format = CpioDetectFormat(hStdin, &error);

  if (format == CPIO_FORMAT_UNKNOWN) {
    WriteStdErrLine("Error: Unknown or invalid CPIO format");
    return 1;
  }

  if (verbose) {
    if (format == CPIO_FORMAT_ODC) {
      WriteStdErrLine("Format: ODC");
    }
    else {
      WriteStdErrLine("Format: NewC");
    }
  }

  if (format == CPIO_FORMAT_ODC) {
    CpioOdcReader* reader = CpioOdcReaderCreate(hStdin, FALSE);
    if (!reader) {
      WriteStdErrLine("Error: Failed to create ODC reader");
      return 1;
    }

    CpioOdcHeader header;
    while (CpioOdcReaderReadNext(reader, &header, &error)) {
      if (CpioStringCompare(header.name, ".") == 0) {
        CpioOdcReaderFinish(reader, &error);
        continue;
      }

      const char* name = header.name;
      if (name[0] == '.' && name[1] == '/') {
        name += 2;
      }

      if (name[0] == '\0') {
        CpioOdcReaderFinish(reader, &error);
        continue;
      }

      char winPath[CPIO_MAX_NAME_LENGTH];
      SIZE_T j = 0;
      for (SIZE_T i = 0; name[i] && j < CPIO_MAX_NAME_LENGTH - 1; i++) {
        winPath[j++] = (name[i] == '/') ? '\\' : name[i];
      }
      winPath[j] = '\0';

      if (verbose) WriteStdErrLine(winPath);

      WCHAR* wideName = CpioStringToWide(winPath);
      if (!wideName) {
        CpioOdcReaderFinish(reader, &error);
        continue;
      }

      if (header.mode & CPIO_S_IFDIR) {
        CreateDirectoryW(wideName, NULL);
      }
      else {
        WCHAR* lastSlash = wideName;
        for (WCHAR* p = wideName; *p; p++) {
          if (*p == L'\\') lastSlash = p;
        }

        if (lastSlash != wideName) {
          *lastSlash = L'\0';

          for (WCHAR* p = wideName; *p; p++) {
            if (*p == L'\\') {
              *p = L'\0';
              CreateDirectoryW(wideName, NULL);
              *p = L'\\';
            }
          }
          CreateDirectoryW(wideName, NULL);

          *lastSlash = L'\\';
        }

        HANDLE hOutFile = CreateFileW(wideName, GENERIC_WRITE, 0, NULL,
          CREATE_ALWAYS, FILE_ATTRIBUTE_NORMAL, NULL);

        if (hOutFile == INVALID_HANDLE_VALUE) {
          WriteStdErr("Warning: Cannot create ");
          WriteStdErrLine(winPath);
          CpioFree(wideName);
          CpioOdcReaderFinish(reader, &error);
          continue;
        }

        char buffer[8192];
        DWORD bytesRead;
        while ((bytesRead = CpioOdcReaderRead(reader, buffer, sizeof(buffer), &error)) > 0) {
          DWORD bytesWritten;
          WriteFile(hOutFile, buffer, bytesRead, &bytesWritten, NULL);
        }

        CloseHandle(hOutFile);
      }

      CpioFree(wideName);
    }

    CpioOdcReaderDestroy(reader);

  }
  else {
    CpioNewcReader* reader = CpioNewcReaderCreate(hStdin, FALSE);
    if (!reader) {
      WriteStdErrLine("Error: Failed to create NewC reader");
      return 1;
    }

    CpioNewcHeader header;
    while (CpioNewcReaderReadNext(reader, &header, &error)) {
      if (CpioStringCompare(header.name, ".") == 0) {
        CpioNewcReaderFinish(reader, &error);
        continue;
      }

      const char* name = header.name;
      if (name[0] == '.' && name[1] == '/') {
        name += 2;
      }

      if (name[0] == '\0') {
        CpioNewcReaderFinish(reader, &error);
        continue;
      }

      char winPath[CPIO_MAX_NAME_LENGTH];
      SIZE_T j = 0;
      for (SIZE_T i = 0; name[i] && j < CPIO_MAX_NAME_LENGTH - 1; i++) {
        winPath[j++] = (name[i] == '/') ? '\\' : name[i];
      }
      winPath[j] = '\0';

      if (verbose) WriteStdErrLine(winPath);

      WCHAR* wideName = CpioStringToWide(winPath);
      if (!wideName) {
        CpioNewcReaderFinish(reader, &error);
        continue;
      }

      if (header.mode & CPIO_S_IFDIR) {
        CreateDirectoryW(wideName, NULL);
      }
      else {
        WCHAR* lastSlash = wideName;
        for (WCHAR* p = wideName; *p; p++) {
          if (*p == L'\\') lastSlash = p;
        }

        if (lastSlash != wideName) {
          *lastSlash = L'\0';

          for (WCHAR* p = wideName; *p; p++) {
            if (*p == L'\\') {
              *p = L'\0';
              CreateDirectoryW(wideName, NULL);
              *p = L'\\';
            }
          }
          CreateDirectoryW(wideName, NULL);

          *lastSlash = L'\\';
        }

        HANDLE hOutFile = CreateFileW(wideName, GENERIC_WRITE, 0, NULL,
          CREATE_ALWAYS, FILE_ATTRIBUTE_NORMAL, NULL);

        if (hOutFile == INVALID_HANDLE_VALUE) {
          WriteStdErr("Warning: Cannot create ");
          WriteStdErrLine(winPath);
          CpioFree(wideName);
          CpioNewcReaderFinish(reader, &error);
          continue;
        }

        char buffer[8192];
        DWORD bytesRead;
        while ((bytesRead = CpioNewcReaderRead(reader, buffer, sizeof(buffer), &error)) > 0) {
          DWORD bytesWritten;
          WriteFile(hOutFile, buffer, bytesRead, &bytesWritten, NULL);
        }

        CloseHandle(hOutFile);
      }

      CpioFree(wideName);
    }

    CpioNewcReaderDestroy(reader);
  }

  if (verbose) {
    WriteStdErrLine("Extraction complete");
  }

  return 0;
}

void mainCRTStartup(void) {
  LPWSTR cmdLine = GetCommandLineW();

  int argc;
  LPWSTR* argv = CommandLineToArgvW(cmdLine, &argc);

  BOOL createMode = FALSE;
  BOOL extractMode = FALSE;
  BOOL verbose = FALSE;
  BOOL useOdc = FALSE;

  for (int i = 1; i < argc; i++) {
    char arg[256];
    WideCharToMultiByte(CP_UTF8, 0, argv[i], -1, arg, sizeof(arg), NULL, NULL);

    if (CpioStringCompare(arg, "-o") == 0 || CpioStringCompare(arg, "--create") == 0) {
      createMode = TRUE;
    }
    else if (CpioStringCompare(arg, "-i") == 0 || CpioStringCompare(arg, "--extract") == 0) {
      extractMode = TRUE;
    }
    else if (CpioStringCompare(arg, "-v") == 0 || CpioStringCompare(arg, "--verbose") == 0) {
      verbose = TRUE;
    }
    else if (CpioStringStartsWith(arg, "--format=")) {
      const char* format = arg + 9;
      if (CpioStringCompare(format, "odc") == 0 || CpioStringCompare(format, "ODC") == 0) {
        useOdc = TRUE;
      }
    }
    else if (CpioStringCompare(arg, "-h") == 0 || CpioStringCompare(arg, "--help") == 0) {
      PrintUsage();
      ExitProcess(0);
    }
  }

  if (!createMode && !extractMode) {
    WriteStdErrLine("Error: Must specify -o (create) or -i (extract)\n");
    PrintUsage();
    ExitProcess(1);
  }

  if (createMode && extractMode) {
    WriteStdErrLine("Error: Cannot specify both -o and -i\n");
    PrintUsage();
    ExitProcess(1);
  }

  int exitCode;
  if (createMode) {
    exitCode = CreateArchive(verbose, useOdc);
  }
  else {
    exitCode = ExtractArchive(verbose);
  }

  LocalFree(argv);
  ExitProcess(exitCode);
}