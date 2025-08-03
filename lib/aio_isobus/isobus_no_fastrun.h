// isobus_no_fastrun.h
// Prevent ISOBUS code from being placed in ITCM

#ifndef ISOBUS_NO_FASTRUN_H
#define ISOBUS_NO_FASTRUN_H

// Save original FASTRUN definition
#ifdef FASTRUN
#define FASTRUN_ORIGINAL FASTRUN
#undef FASTRUN
#endif

// Replace with empty definition
#define FASTRUN

// Save PROGMEM definition
#ifdef PROGMEM
#define PROGMEM_ORIGINAL PROGMEM
#endif

#endif // ISOBUS_NO_FASTRUN_H