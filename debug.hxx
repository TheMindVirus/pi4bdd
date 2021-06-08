#ifndef _DEBUG_HXX_
#define _DEBUG_HXX_

#ifndef DEBUG_FILENAME
#define DEBUG_FILENAME   L"pi4bdd.log"
#endif//DEBUG_FILENAME

#ifndef DEBUG_FILEROOT
#define DEBUG_FILEROOT   L"\\SystemRoot\\"
#endif//DEBUG_FILEROOT

#ifdef DBG
#define debug(...)   KdPrintEx((DPFLTR_IHVDRIVER_ID, DPFLTR_ERROR_LEVEL, __VA_ARGS__)); KdPrintEx((DPFLTR_IHVDRIVER_ID, DPFLTR_ERROR_LEVEL, "\n")); //KeStallExecutionProcessor(10000); //DebugLogToFile(__VA_ARGS__);
#else
#define debug(...)   KeStallExecutionProcessor(10000); //Potential fix for timing issues in Release builds

static BOOLEAN _DebugOverwrite = TRUE;

void DebugLogToFile(LPSTR format, ...);

#endif//NDEBUG

#endif//_DEBUG_HXX_