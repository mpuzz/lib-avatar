#ifndef AVATAR_MSG
#define AVATAR_MSG

enum AvatarIOOperation {
  AVATAR_READ,
  AVATAR_WRITE,
  AVATAR_FORK,
  AVATAR_CLOSE
};

struct _AvatarIORequestMessage {
  uint64_t id;
  uint64_t hwaddr;
  uint64_t value;
  uint32_t state;
  enum AvatarIOOperation operation;
  char new_mq[8];
};

struct _AvatarIOResponseMessage {
  uint64_t id;
  uint64_t value;
  uint32_t state;
  bool success;
};

struct _IRQ_MSG {
  uint32_t irq_num;
  uint32_t state;
  uint32_t level;
};

typedef struct _AvatarIORequestMessage  AvatarIORequestMessage;
typedef struct _AvatarIOResponseMessage AvatarIOResponseMessage;
typedef struct _IRQ_MSG IRQ_MSG;

#endif
