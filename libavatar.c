#include "Python.h"
#include "libavatar.h"
#include <mutex>
#include <ctime>
#include <thread>
#include <sys/select.h>
extern "C" {
#include "avatar-msgs.h"
}

static std::mutex m;
static bool stop;

static mqd_t io_request;
static mqd_t io_response;
static mqd_t irq;

static irq_callback callback;

static int msg_count = 0;

static mqd_t open_mq(char *mq_name, mqs mq_type)
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
	io_response = mq_open(mq_name, O_RDONLY);
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

static int dispatch_io(std::uint32_t address, std::size_t size, bool write, void *buffer)
{
  Py_BEGIN_ALLOW_THREADS
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
      *cast = resp.value;
    }
  Py_END_ALLOW_THREADS
  return 0;
}

static void register_IRQ_callback(irq_callback cb)
{
  callback = cb;
}

static inline void unset_stop()
{
  std::lock_guard<std::mutex> lg(m);
  stop = false;
}

static inline int mq_receive_timeout(mqd_t mq, char *buf, std::size_t size)
{
  fd_set set;
  struct timeval timeout;
  int res;

  FD_ZERO(&set);
  FD_SET(mq, &set);

  timeout.tv_sec = 0;
  timeout.tv_usec = 10;
  res = select(mq+1, &set, NULL, NULL, &timeout);
  if(res == -1)
    {
      fprintf(stderr, "Error reading from IRQs queue: %d\n", errno);
      return -1;
    }
  if(res == 0)
    {
      return 0;
    }
  int ret = mq_receive(mq, buf, size, NULL);

  return ret;
}

static inline bool should_stop()
{
  std::lock_guard<std::mutex> lg(m);
  return stop;
}

static void wait_for_IRQs()
{
  IRQ_MSG msg;

  if(irq == 0)
    return;

  unset_stop();

  while(true)
    {
      std::size_t ret = mq_receive_timeout(irq, (char *) &msg, sizeof(msg));
      if(ret > 0)
	{
	  callback(msg.irq_num);
	}
      if(should_stop())
	break;
    }
}

void stop_IRQ_handling()
{
  std::lock_guard<std::mutex> lg(m);
  stop = true;
}

/*************************************************************
                    Python wrappers
*************************************************************/
static PyObject *avatar_qemu_open_mq(PyObject *self, PyObject *args)
{
  char *path;
  mqs type;
  mqd_t ret;

  if(!PyArg_ParseTuple(args, "si", &path, &type))
    {
      return NULL;
    }
  ret = open_mq(path, type);
  if(ret <= 0)
    return NULL;
  return PyLong_FromLong(ret);
}

static PyObject *avatar_qemu_write(PyObject *self, PyObject *args)
{
  std::uint32_t address;
  std::size_t size;
  std::uint64_t value;
  int ret;

  if(!PyArg_ParseTuple(args, "lil", &address, &size, &value))
    {
      return NULL;
    }

  if(size <= 0 || size > 4)
    {
      fprintf(stderr, "asdasdasd %d, %d\n", size, 4 > 4);
      return NULL;
    }

  ret = dispatch_io(address, size, true, &value);

  if(ret)
    {
      return NULL;
    }
  return PyLong_FromLong(0);
}

static PyObject *avatar_qemu_read(PyObject *self, PyObject *args)
{
  std::uint32_t address;
  std::size_t size;
  std::uint64_t value;
  int ret;

  if(!PyArg_ParseTuple(args, "li", &address, &size))
    {
      return NULL;
    }

  if(size <= 0 || size > 4)
    {
      return NULL;
    }

  ret = dispatch_io(address, size, false, &value);

  if(ret)
    {
      return NULL;
    }
  return PyLong_FromUnsignedLong(value);
}

static PyObject *python_callback = NULL;
static std::thread *irq_thread;

static PyObject *avatar_qemu_register_IRQ_callback(PyObject *self, PyObject *args)
{
  PyObject *result = NULL;
  PyObject *temp;

  if (PyArg_ParseTuple(args, "O", &temp))
    {
      if (!PyCallable_Check(temp))
	{
	  PyErr_SetString(PyExc_TypeError, "parameter must be callable");
	  return NULL;
	}

      Py_XINCREF(temp);         /* Add a reference to new callback */
      Py_XDECREF(python_callback);  /* Dispose of previous callback */
      python_callback = temp;       /* Remember new callback */
      /* Boilerplate to return "None" */
      Py_INCREF(Py_None);
      result = Py_None;
    }
  return result;
}

static void execute_callback(int irq)
{
  PyGILState_STATE gstate;
  gstate = PyGILState_Ensure();

  PyObject *arg = Py_BuildValue("(i)", irq);
  Py_INCREF(arg);
  PyObject_Call(python_callback, arg, NULL);
  Py_DECREF(arg);
  PyGILState_Release(gstate);
}

static PyObject *avatar_qemu_irq_start(PyObject *self, PyObject *args)
{

  if(python_callback == NULL)
    {
      return NULL;
    }
  register_IRQ_callback(execute_callback);
  irq_thread = new std::thread(wait_for_IRQs);
  Py_INCREF(Py_None);
  return Py_None;
}

static PyObject *avatar_qemu_irq_stop(PyObject *self, PyObject *args)
{
  Py_BEGIN_ALLOW_THREADS
  stop_IRQ_handling();

  irq_thread->join();
  Py_END_ALLOW_THREADS
  Py_INCREF(Py_None);
  return Py_None;
}

static PyMethodDef AvatarMethods[] = {
  {"open_mq", avatar_qemu_open_mq, METH_VARARGS, "Initialize specific IPC channel to the emulator"},
  {"write", avatar_qemu_write, METH_VARARGS, "Request a write operation"},
  {"read", avatar_qemu_read, METH_VARARGS, "Request a read operation"},
  {"register_IRQ_callback", avatar_qemu_register_IRQ_callback, METH_VARARGS, "Register a callback to manage IRQs"},
  {"irq_start", avatar_qemu_irq_start, METH_VARARGS, "Start a thread that manages IRQs"},
  {"irq_stop", avatar_qemu_irq_stop, METH_VARARGS, "Stop the thread that manages the IRQs"},
  {NULL, NULL, NULL, NULL}
};

PyMODINIT_FUNC initavatar_qemu(void)
{
  PyObject *module = Py_InitModule("avatar_qemu", AvatarMethods);
  PyModule_AddIntConstant(module, "IOREQ", IOREQ);
  PyModule_AddIntConstant(module, "IORESP", IORESP);
  PyModule_AddIntConstant(module, "IRQ", IRQ);
  PyEval_InitThreads();
}
