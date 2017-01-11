#ifndef LIB_AVATAR_H
#define LIB_AVATAR_H

#include <cstdio>
#include <cstdint>
#include <mqueue.h>

#define DEBUG_PRINT(...) if(debug) fprintf(stderr, __VA_ARGS__);

enum mqs
  {
    IOREQ,
    IORESP,
    IRQ
  };

typedef void (*irq_callback)(int irq, unsigned int state);

#endif
