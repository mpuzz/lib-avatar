#include "libavatar.h"
#include <mutex>
#include <ctime>

extern "C" {
#include "avatar-msgs.h"
}

std::mutex m;
static bool stop;

mqd_t io_request;
mqd_t io_response;
mqd_t irq;

static irq_callback callback;

int msg_count = 0;

mqd_t open_mq(char *mq_name, mqs mq_type)
{
  switch(mq_type)
    {
    case IOREQ:
      {
	io_request = mq_open(mq_name, O_WRONLY);
	return io_request;
      }
    case IORESP:
      {
	io_response= mq_open(mq_name, O_RDONLY);
	return io_response;
      }
    case IRQ:
      {
	irq = mq_open(mq_name, O_RDONLY);
	return irq;
      }
    default:
      return -1;
    }
}

int dispatch_io(std::uint32_t address, std::size_t size, bool write, void *buffer)
{
  AvatarIORequestMessage req;
  AvatarIOResponseMessage resp;
  std::size_t byte_read;

  req.id = msg_count++;
  req.hwaddr = address;
  req.write = write;

  if(write)
    {
      req.value = *(std::uint64_t *) buffer;
    }

  if(mq_send(io_request, (char *) &req, sizeof(req), 0))
    {
      fprintf(stderr, "Send failed with error: %d\n", errno);
      return -1;
    }

  do
    {
      byte_read = mq_receive(io_response, (char *) &resp, sizeof(resp), NULL);
      if(byte_read != sizeof(resp))
	{
	  fprintf(stderr, "Error in receiving message: %d\n", errno);
	  return -2;
	}
    } while(resp.id != req.id);

  if(resp.id != req.id)
    {
      fprintf(stderr, "Error. Wrong id\n");
      return -3;
    }
  if(!resp.success)
    {
      fprintf(stderr, "Error. IO did not succeeded\n");
      return -4;
    }

  if(!write)
    {
      uint64_t *cast = (uint64_t *) buffer;
      *cast = req.value;
    }

  return 0;
}

void register_IRQ_callback(irq_callback cb)
{
  callback = cb;
}

static inline void unset_stop()
{
  std::lock_guard<std::mutex> lg(m);
  stop = false;
}

static inline int mq_receive_5ms(mqd_t mq, char *buf, std::size_t size)
{
  struct timespec tm;

  clock_gettime(CLOCK_REALTIME, &tm);
  tm.tv_nsec += 5 * 1000 * 1000;
  int ret = mq_timedreceive(mq, buf, size, NULL, &tm);

  return (ret < 0 && errno == ETIMEDOUT) ? 0: ret;
}

static inline bool should_stop()
{
  std::lock_guard<std::mutex> lg(m);
  return stop;
}

void wait_for_IRQs()
{
  IRQ_MSG msg;
  unset_stop();
  while(true)
    {
      std::size_t ret = mq_receive_5ms(irq, (char *) &msg, sizeof(msg));

      if(ret <= 0)
	{
	  fprintf(stderr, "Error reading processing incoming interrupt\n");
	}
      else if(ret > 0)
	{
	  callback(msg.irq_num);
	}
      if(should_stop())
	break;
    }
}
