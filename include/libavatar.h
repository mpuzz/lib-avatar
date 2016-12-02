#ifndef LIB_AVATAR_H
#define LIB_AVATAR_H

#include <cstdio>
#include <cstdint>
#include <mqueue.h>

enum mqs
  {
    IOREQ,
    IORESP,
    IRQ
  };

typedef void (*irq_callback)(int irq);

#endif
