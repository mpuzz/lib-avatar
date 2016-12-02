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

extern mqd_t io_request;
extern mqd_t io_response;
extern mqd_t irq;

typedef void (*irq_callback)(int irq);

mqd_t open_mq(char *mq_name, mqs mq_type);
int  dispatch_io(std::uint32_t address, std::size_t size, void *buffer);

void register_IRQ_callback(irq_callback cb);
void wait_for_IRQs();
void stop_IRQ_handling();

#endif
