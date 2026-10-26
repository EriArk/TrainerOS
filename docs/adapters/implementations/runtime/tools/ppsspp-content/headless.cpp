// The content probe has no GUI/log service. Decoder failures propagate through
// IsOK/read results; stdout is reserved for the bounded identity response.
#include "Common/Log.h"
namespace { bool logging=false; }
bool *g_bLogEnabledSetting=&logging;
LogChannel g_log[(size_t)Log::NUMBER_OF_LOGS];
void GenericLog(Log,LogLevel,const char*,int,const char*,...) {}
