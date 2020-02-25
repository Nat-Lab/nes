#ifndef NES_LOG_H
#define NES_LOG_H
#include <stdio.h>
#define log_debug(fmt, ...) log("DEBUG", fmt, ## __VA_ARGS__)
#define log_info(fmt, ...) log("\033[1mINFO\033[0m ", fmt, ## __VA_ARGS__)
#define log_warn(fmt, ...) log("\033[1;33mWARN\033[0m ", fmt, ## __VA_ARGS__)
#define log_error(fmt, ...) log("\033[1;31mERROR\033[0m", fmt, ## __VA_ARGS__)
#define log_fatal(fmt, ...) log("\033[1;31mFATAL\033[0m", fmt, ## __VA_ARGS__)
#define log(log_level, fmt, ...) fprintf(stderr, "[" log_level "] %s:%d %s: " fmt, __FILE__, __LINE__, __FUNCTION__, ## __VA_ARGS__)
#endif // NES_LOG_H