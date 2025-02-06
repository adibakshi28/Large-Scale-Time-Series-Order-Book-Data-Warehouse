#include "Progress.h"

std::atomic<uint64_t> g_totalBytes{0};   // Counter for the total number of bytes to be processed.
std::atomic<uint64_t> g_bytesProcessed{0};  // Counter for the number of bytes that have been processed so far.
