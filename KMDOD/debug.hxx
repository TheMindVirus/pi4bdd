#ifndef _DEBUG_HXX_
#define _DEBUG_HXX_

#ifndef DEBUG_FILENAME
#define DEBUG_FILENAME   L"KMDFDriver.log"
#endif//DEBUG_FILENAME

#ifndef DEBUG_FILEROOT
#define DEBUG_FILEROOT   L"\\SystemRoot\\"
#endif//DEBUG_FILEROOT

#ifdef NDEBUG
#define debug(...)
#else
#define debug(...)   DebugLogToFile(__VA_ARGS__);

static BOOLEAN _DebugOverwrite = TRUE;

void DebugLogToFile(LPSTR format, ...);

#endif//NDEBUG

#endif//_DEBUG_HXX_