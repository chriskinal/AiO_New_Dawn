// WebSystemFlashmem.h
// Macros to move web server code out of ITCM to save memory for ISOBUS

#ifndef WEBSYSTEM_FLASHMEM_H
#define WEBSYSTEM_FLASHMEM_H

// When ISOBUS is enabled, move web handlers to flash memory
#ifdef ENABLE_ISOBUS_VT
    #define WEBSYSTEM_FLASHMEM __attribute__((section(".flashmem")))
#else
    #define WEBSYSTEM_FLASHMEM
#endif

// For large string processing functions
#ifdef ENABLE_ISOBUS_VT
    #define STRING_FLASHMEM __attribute__((section(".flashmem")))
#else
    #define STRING_FLASHMEM
#endif

#endif // WEBSYSTEM_FLASHMEM_H