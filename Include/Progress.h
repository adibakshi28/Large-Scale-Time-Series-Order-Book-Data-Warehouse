#ifndef PROGRESS_H
#define PROGRESS_H

#include <atomic>
#include <cstdint>

/**
 * @brief Global atomic counter for the total number of bytes to be processed.
 */
extern std::atomic<uint64_t> g_totalBytes;

/**
 * @brief Global atomic counter for the number of bytes that have been processed so far.
 */
extern std::atomic<uint64_t> g_bytesProcessed;

#endif
