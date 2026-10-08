#ifndef COROUTINE_H_
#define COROUTINE_H_

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>
#include <stdlib.h>
#include <sys/epoll.h>
#include <sys/socket.h>
#include <sys/types.h>
#include <unistd.h>

#define COROUTINE_DEFAULT_STACK_SIZE 16384

typedef struct Coroutine Coroutine;
typedef void (*CoroutineFunc)(Coroutine *);

typedef enum {
  CR_DEAD = 0,
  CR_READY,
  CR_RUNNING,
  CR_SUSPENDED,
  CR_SLEEPING,
} CoroutineState;

struct Coroutine {
  char *stack;
  size_t stack_size;

  void *routine_sp;
  void *caller_sp;
  void *userdata;
  size_t userdata_size;

  void *yield_dest;
  size_t yield_size;

  CoroutineFunc entry_point;
  CoroutineState state;

  struct Coroutine *next; // Ready Queue Pointer

  // Time to wakeup after doing a sleep yield, called 0 if not currently sleeping.
  uint64_t wakeup_time;
};

Coroutine *CoroutineNew(CoroutineFunc entry_point, void *userdata, size_t userdata_size);
Coroutine *CoroutineNewStackSize(CoroutineFunc entry_point, void *userdata, size_t userdata_size, size_t stack_size);
void *CoroutineCall(Coroutine *routine);
void CoroutineYield(Coroutine *routine);
void CoroutineYieldValue(Coroutine *routine, void *value, size_t size);
void CoroutineTerminate(Coroutine *cr);
void CoroutineDestroy(Coroutine *routine);
bool CoroutineIsAlive(Coroutine *routine);

#define GET_16TH_ARG_(_1, _2, _3, _4, _5, _6, _7, _8, _9, _10, _11, _12, _13, _14, _15, NAME, ...) NAME
#define DISPATCH_NAME_(macro, ...)                              \
  GET_16TH_ARG_(__VA_ARGS__,                                    \
                macro##_params, macro##_params, macro##_params, \
                macro##_params, macro##_params, macro##_params, \
                macro##_params, macro##_params, macro##_params, \
                macro##_params, macro##_params, macro##_params, \
                macro##_params, macro##_params, macro##_void, _)(__VA_ARGS__)

#define coroutine(...) DISPATCH_NAME_(coroutine, __VA_ARGS__)
#define subroutine(...) DISPATCH_NAME_(subroutine, __VA_ARGS__)
#define subroutine_call(...) DISPATCH_NAME_(subroutine_call, __VA_ARGS__)
#define coroutine_init(...) DISPATCH_NAME_(coroutine_init, __VA_ARGS__)
#define coroutine_init_stack(...) DISPATCH_NAME_(coroutine_init_stack, __VA_ARGS__)

#define coroutine_params(name, ...) \
  typedef struct {                  \
    __VA_ARGS__;                    \
  } name##_params_;                 \
  void name(Coroutine *routine)

#define coroutine_void(name) \
  void name(Coroutine *routine)

#define subroutine_params(name, ...) \
  name(Coroutine *routine, __VA_ARGS__)

#define subroutine_void(name) \
  name(Coroutine *routine)

#define subroutine_call_void(name) \
  name(routine)

#define subroutine_call_params(name, ...) \
  name(routine, __VA_ARGS__)

#define get_args(name) name##_params_ args = *((name##_params_ *)(routine->userdata))

#define coroutine_init_params(name, ...) \
  Coroutine *name##_routine_ = CoroutineNew(name, &(name##_params_){__VA_ARGS__}, sizeof(name##_params_))

#define coroutine_init_void(name) \
  Coroutine *name##_routine_ = CoroutineNew(name, NULL, 0)

#define coroutine_init_stack_void(name, stack_size) \
  Coroutine *name##_routine_ = CoroutineNewStackSize(name, NULL, 0, stack_size)

#define coroutine_init_stack_params(name, stack_size, ...) \
  Coroutine *name##_routine_ = CoroutineNewStackSize(name, &(name##_params_){__VA_ARGS__}, sizeof(name##_params_), stack_size)

#define coroutine_call(name) \
  CoroutineCall(name##_routine_)

#define yield_val(type, value) \
  CoroutineYieldValue(routine, &(type){value}, sizeof(type))

#define yield \
  CoroutineYield(routine)

#define coroutine_destroy(name) \
  CoroutineDestroy(name##_routine_)

#define coroutine_is_alive(name) \
  CoroutineIsAlive(name##_routine_)

void *make_context(void *stack_bottom, size_t stack_size, Coroutine *routine);

// Store the current context and put the stack address into *from.
// Will move %rsp into the address specified via 'to'.
void swap_context(void **from, void *to);

typedef struct CoroutineNode {
  Coroutine *routine;
  struct CoroutineNode *next;
} CoroutineNode;

typedef struct {
  CoroutineNode *start;
  CoroutineNode *end;
  size_t length;
} CoroutineReadyQueue;

#define EVENT_QUEUE_SIZE 16

typedef struct {
  Coroutine *scheduler_routine;

  // Ready queue - Linked List Queue.
  Coroutine *ready;
  Coroutine *ready_tail;

  // Sleeping Queue - Min-Heap Priority Queue.
  Coroutine **sleep_queue;
  size_t sleep_queue_size;
  size_t sleep_queue_cap;

  // Suspended Queue - epoll events.
  int epollFd;
  struct epoll_event event_queue[EVENT_QUEUE_SIZE];
} CoroutineScheduler;

void CrSchedulerInit(void);
void CrSchedulerAddReady(Coroutine *cr);
void CrSchedulerAddReadyToFront(Coroutine *cr);
Coroutine *CrSchedulerGetNext(void);
Coroutine *MainCoroutine(void);
// void CrSchedulerStart(void);
void CrSchedulerStart(Coroutine *main_coroutine);
void CrSchedulerEnd(void);

void CoroutineYieldSleep(Coroutine *cr, uint64_t wakeup_time);
Coroutine *CrSchedulerRemoveSleeper(void);
void CrSchedulerWakeUp(int sig);

void CoroutineYieldEvent(Coroutine *cr, int fd, uint32_t events);

ssize_t CoroutineRead(Coroutine *cr, int fd, void *buf, size_t size);
ssize_t CoroutineWrite(Coroutine *cr, int fd, const void *buf, size_t count);
int CoroutineAccept(Coroutine *cr, int sockfd, struct sockaddr *addr, socklen_t *addrlen);

#define setup_coroutines()              \
  Coroutine *routine = MainCoroutine(); \
  CrSchedulerInit();                    \
  CrSchedulerAddReady(routine);         \
  CrSchedulerStart(routine)

#define teardown_coroutines() \
  CrSchedulerEnd();           \
  free(routine)

#define coroutine_spawn(...) DISPATCH_NAME_(coroutine_spawn, __VA_ARGS__)
#define coroutine_spawn_stack(...) DISPATCH_NAME_(coroutine_spawn_stack, __VA_ARGS__)

#define coroutine_spawn_void(name)        \
  do {                                    \
    coroutine_init_void(name);            \
    CrSchedulerAddReady(name##_routine_); \
  } while (0)

#define coroutine_spawn_stack_void(name)  \
  do {                                    \
    coroutine_init_stack_void(name);      \
    CrSchedulerAddReady(name##_routine_); \
  } while (0)

#define coroutine_spawn_params(name, ...)     \
  do {                                        \
    coroutine_init_params(name, __VA_ARGS__); \
    CrSchedulerAddReady(name##_routine_);     \
  } while (0)

#define coroutine_spawn_stack_params(name, ...)     \
  do {                                              \
    coroutine_init_stack_params(name, __VA_ARGS__); \
    CrSchedulerAddReady(name##_routine_);           \
  } while (0)

#define yield_sleep(millis) \
  CoroutineYieldSleep(routine, millis)

#define cr_read(fd, buf, size) \
  CoroutineRead(routine, fd, buf, size)

#define cr_write(fd, buf, count) \
  CoroutineWrite(routine, fd, buf, count)

#define cr_accept(sockfd, addr, addrlen) \
  CoroutineAccept(routine, sockfd, addr, addrlen)

int make_nonblocking(int fd);

#endif

#ifdef COROUTINE_IMPLEMENTATION
#include <errno.h>
#include <fcntl.h>
#include <poll.h>
#include <stddef.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/epoll.h>
#include <sys/mman.h>
#include <sys/poll.h>
#include <sys/time.h>
#include <sys/types.h>
#include <time.h>
#include <unistd.h>

__attribute__((naked)) static void coroutine_entry_stub_(void) {
  __asm__ volatile(
      "popq %rdi\n\t"
      "ret\n\t");
}

static void coroutine_return_caller_(Coroutine *routine) {
  swap_context(&routine->routine_sp, routine->caller_sp);
}

static void coroutine_entry_(Coroutine *routine) {
  routine->entry_point(routine);

  // If coroutine finishes.
  routine->state = CR_DEAD;
  coroutine_return_caller_(routine);
}

void *make_context(void *stack_bottom, size_t stack_size, Coroutine *routine) {
  // Move to the top so stack can grow downwards.
  uintptr_t sp = (uintptr_t)stack_bottom + stack_size;

  // Align (downwards) to 16 to comply with C ABI.
  sp &= ~((uintptr_t)15);

  // Stack is an "Array of 64 bit ints"
  uint64_t *stack = (uint64_t *)sp;

  *(--stack) = 0; // padding to ensure allignment

  // When calling swap_context we have 6 pops followed by a return.
  // To make this context usable we allocate 6 pointers for the pops
  // and then entry point will be called.
  *(--stack) = (uintptr_t)coroutine_entry_;
  *(--stack) = (uintptr_t)routine;
  *(--stack) = (uintptr_t)coroutine_entry_stub_;
  for (int i = 0; i < 6; i++) {
    *(--stack) = 0;
  }

  return (void *)stack;
}

#if defined(__clang__)
#pragma clang diagnostic push
#pragma clang diagnostic ignored "-Wunused-parameter"
#elif defined(__GNUC__)
#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wunused-parameter"
#endif

__attribute__((naked)) void swap_context(void **from, void *to) {
  // %rdi - will have the address into which current %rsp will be put in
  // %rsi - will house the address of the new stack.

  // Stores callee saved register on the context before swapping.
  // Restores the callee saved register of the new context.
  __asm__ volatile(
      "pushq %rbp\n\t"
      "pushq %rbx\n\t"
      "pushq %r12\n\t"
      "pushq %r13\n\t"
      "pushq %r14\n\t"
      "pushq %r15\n\t"
      "movq %rsp, (%rdi)\n\t"
      "movq %rsi, %rsp\n\t"
      "popq %r15\n\t"
      "popq %r14\n\t"
      "popq %r13\n\t"
      "popq %r12\n\t"
      "popq %rbx\n\t"
      "popq %rbp\n\t"
      "ret\n\t");
}

#if defined(__clang__)
#pragma clang diagnostic pop
#elif defined(__GNUC__)
#pragma GCC diagnostic pop
#endif

Coroutine *CoroutineNewStackSize(CoroutineFunc entry_point, void *userdata, size_t userdata_size, size_t stack_size) {
  Coroutine *routine = calloc(1, sizeof(Coroutine));
  if (!routine)
    return NULL;

  size_t page_size = sysconf(_SC_PAGESIZE);
  size_t aligned_stack_size = (stack_size + page_size - 1) & ~(page_size - 1);
  routine->stack = mmap(NULL,
                        aligned_stack_size + page_size, PROT_READ | PROT_WRITE,
                        MAP_ANONYMOUS | MAP_PRIVATE, -1, 0);

  if (routine->stack == MAP_FAILED) {
    free(routine);
    return NULL;
  }
  if (mprotect(routine->stack, page_size, PROT_NONE) != 0) {
    munmap(routine->stack, aligned_stack_size + page_size);
    free(routine);
    return NULL;
  }

  routine->stack += page_size;
  routine->stack_size = aligned_stack_size;

  if (userdata_size > 0) {
    routine->userdata = malloc(userdata_size);
    if (!routine->userdata) {
      routine->state = CR_DEAD;
      return NULL;
    }
    routine->userdata_size = userdata_size;
    memcpy(routine->userdata, userdata, userdata_size);
  } else {
    routine->userdata = NULL;
    routine->userdata_size = 0;
  }

  routine->routine_sp = make_context(routine->stack, routine->stack_size, routine);
  routine->entry_point = entry_point;
  routine->state = CR_READY;
  return routine;
}

Coroutine *CoroutineNew(CoroutineFunc entry_point, void *userdata, size_t userdata_size) {
  return CoroutineNewStackSize(entry_point, userdata, userdata_size, COROUTINE_DEFAULT_STACK_SIZE);
}

void *CoroutineCall(Coroutine *routine) {
  routine->state = CR_RUNNING;
  swap_context(&routine->caller_sp, routine->routine_sp);
  return routine->yield_dest;
}

void CoroutineYield(Coroutine *routine) {
  routine->state = CR_READY;
  coroutine_return_caller_(routine);
}

void CoroutineYieldValue(Coroutine *routine, void *value, size_t size) {
  routine->state = CR_READY;
  if (routine->yield_size != size) {
    void *ptr = realloc(routine->yield_dest, size);
    if (!ptr) {
      routine->state = CR_DEAD;
      routine->yield_dest = NULL;
      routine->yield_size = 0;
      swap_context(&routine->routine_sp, routine->caller_sp);
      return;
    }
    routine->yield_dest = ptr;
    routine->yield_size = size;
  }

  memcpy(routine->yield_dest, value, size);
  coroutine_return_caller_(routine);
}

void CoroutineDestroy(Coroutine *routine) {
  if (routine->stack) {
    size_t page_size = sysconf(_SC_PAGESIZE);
    munmap(routine->stack - page_size, routine->stack_size + page_size);
  }

  if (routine->yield_dest)
    free(routine->yield_dest);

  if (routine->userdata)
    free(routine->userdata);

  free(routine);
}

bool CoroutineIsAlive(Coroutine *routine) {
  return routine->state != CR_DEAD;
}

static uint64_t get_current_time_ms_(void) {
  struct timeval tv;
  gettimeofday(&tv, NULL);

  return (uint64_t)tv.tv_sec * 1000ULL + (uint64_t)tv.tv_usec / 1000ULL;
}

static CoroutineScheduler scheduler;

static void scheduler_routine_(Coroutine *scheduler_routine) {
  (void)scheduler_routine;

  while (1) {
    uint64_t now = get_current_time_ms_();

    // Wake up any sleeping routines that had their wakeup time passed.
    while (scheduler.sleep_queue_size > 0 && scheduler.sleep_queue[0]->wakeup_time <= now) {
      Coroutine *cr = CrSchedulerRemoveSleeper();
      cr->wakeup_time = 0;
      CrSchedulerAddReadyToFront(cr);
    }

    // If no tasks are available + some are sleeping then we timeout the scheduler until a task can wakeup.
    // Using epoll here also allows us to wake up if a suspended task's event is ready.
    int timeout = -1;
    if (scheduler.ready) {
      timeout = 0;
    } else if (scheduler.sleep_queue_size > 0) {
      if (scheduler.sleep_queue[0]->wakeup_time > now) {
        timeout = (int)(scheduler.sleep_queue[0]->wakeup_time - now);
      } else {
        timeout = 0;
      }
    }

    // Remove suspention for any coroutines with ready events or wait for sleeping events.
    int events_ready = epoll_wait(scheduler.epollFd, scheduler.event_queue, EVENT_QUEUE_SIZE, timeout);
    for (int i = 0; i < events_ready; i++) {
      CrSchedulerAddReadyToFront((Coroutine *)(scheduler.event_queue[i].data.ptr));
    }

    if (scheduler.sleep_queue_size == 0 && events_ready == 0 && !scheduler.ready) {
      // All Coroutines are empty?
      // Should not happen since main would run until it ends with the program.
      // In any case it means the main coroutine has ended unexpectedly so we print an error and abort.
      fprintf(stderr, "Coroutine scheduler has run out of coroutines. Main coroutine ended unexpectedly.");
      abort();
    }

    Coroutine *upcoming = CrSchedulerGetNext();
    if (upcoming) {
      CoroutineCall(upcoming);
      if (upcoming->state == CR_READY) {
        CrSchedulerAddReady(upcoming);
      }
      if (upcoming->state == CR_DEAD) {
        CoroutineDestroy(upcoming);
      }

      continue;
    }
  }
}

void CrSchedulerInit(void) {
  memset(&scheduler, 0, sizeof(CoroutineScheduler));
  scheduler.scheduler_routine = CoroutineNew(scheduler_routine_, NULL, 0);
  scheduler.epollFd = epoll_create1(EPOLL_CLOEXEC);
}

void CrSchedulerAddReady(Coroutine *cr) {
  cr->state = CR_READY;
  if (!scheduler.ready) {
    scheduler.ready = cr;
    scheduler.ready_tail = cr;
    return;
  }

  scheduler.ready_tail->next = cr;
  scheduler.ready_tail = cr;
}

Coroutine *CrSchedulerGetNext(void) {
  if (!scheduler.ready) {
    return NULL;
  }

  Coroutine *next = scheduler.ready;
  scheduler.ready = scheduler.ready->next;
  if (!scheduler.ready)
    scheduler.ready_tail = NULL;

  next->next = NULL;
  return next;
}

void CrSchedulerStart(Coroutine *main_coroutine) {
  swap_context(&main_coroutine->routine_sp, scheduler.scheduler_routine->routine_sp);
}

void CrSchedulerEnd(void) {
  while (scheduler.ready) {
    Coroutine *next = scheduler.ready->next;
    CoroutineDestroy(scheduler.ready);
    scheduler.ready = next;
  }

  for (size_t i = 0; i < scheduler.sleep_queue_size; i++) {
    CoroutineDestroy(scheduler.sleep_queue[i]);
  }
  free(scheduler.sleep_queue);

  CoroutineDestroy(scheduler.scheduler_routine);
}

Coroutine *MainCoroutine(void) {
  Coroutine *main = calloc(1, sizeof(Coroutine));
  main->state = CR_READY;
  return main;
}

void CoroutineYieldSleep(Coroutine *cr, uint64_t delay_millis) {
  uint64_t wakeup_time = get_current_time_ms_() + delay_millis;
  cr->wakeup_time = wakeup_time;

  if (scheduler.sleep_queue_size >= scheduler.sleep_queue_cap) {
    size_t new_cap = scheduler.sleep_queue_cap ? scheduler.sleep_queue_cap * 2 : 8;
    if (new_cap < scheduler.sleep_queue_size + 1)
      new_cap = scheduler.sleep_queue_size + 1;

    scheduler.sleep_queue_cap = new_cap;
    scheduler.sleep_queue = realloc(scheduler.sleep_queue, new_cap * sizeof(Coroutine *));
  }

  scheduler.sleep_queue[scheduler.sleep_queue_size++] = cr;

  Coroutine **sleep_q = scheduler.sleep_queue;
  size_t index = scheduler.sleep_queue_size - 1;
  while (index > 0 &&
         sleep_q[(index - 1) / 2]->wakeup_time > sleep_q[index]->wakeup_time) {
    Coroutine *tmp = sleep_q[index];
    sleep_q[index] = sleep_q[(index - 1) / 2];
    sleep_q[(index - 1) / 2] = tmp;
    index = (index - 1) / 2;
  }

  cr->state = CR_SLEEPING;
  coroutine_return_caller_(cr);
}

Coroutine *CrSchedulerRemoveSleeper(void) {
  if (scheduler.sleep_queue_size == 0)
    return NULL;

  Coroutine **sleep_q = scheduler.sleep_queue;
  Coroutine *least_sleeper = sleep_q[0];

  sleep_q[0] = sleep_q[scheduler.sleep_queue_size - 1];
  scheduler.sleep_queue_size--;

  size_t size = scheduler.sleep_queue_size;
  size_t index = 0;
  while (1) {
    size_t smallest = index;
    size_t left = 2 * index + 1;
    size_t right = 2 * index + 2;
    if (left < size && sleep_q[left]->wakeup_time < sleep_q[smallest]->wakeup_time) {
      smallest = left;
    }

    if (right < size && sleep_q[right]->wakeup_time < sleep_q[smallest]->wakeup_time) {
      smallest = right;
    }
    if (smallest != index) {
      Coroutine *tmp = sleep_q[index];
      sleep_q[index] = sleep_q[smallest];
      sleep_q[smallest] = tmp;
      index = smallest;
    } else {
      break;
    }
  }

  return least_sleeper;
}

void CoroutineYieldEvent(Coroutine *cr, int fd, uint32_t events) {
  struct epoll_event ev = {
      .events = events | EPOLLONESHOT,
      .data.ptr = cr,
  };

  if (epoll_ctl(scheduler.epollFd, EPOLL_CTL_ADD, fd, &ev) != 0) {
    if (errno == EEXIST) {
      epoll_ctl(scheduler.epollFd, EPOLL_CTL_MOD, fd, &ev);
    }
  }
  cr->state = CR_SUSPENDED;
  coroutine_return_caller_(cr);
}

void CrSchedulerAddReadyToFront(Coroutine *cr) {
  cr->state = CR_READY;
  if (!scheduler.ready) {
    cr->next = NULL;
    scheduler.ready = scheduler.ready_tail = cr;
    return;
  }

  cr->next = scheduler.ready;
  scheduler.ready = cr;
}

ssize_t CoroutineRead(Coroutine *cr, int fd, void *buf, size_t size) {
  while (1) {
    ssize_t res = read(fd, buf, size);
    if (res >= 0)
      return res;

    if (errno != EWOULDBLOCK && errno != EAGAIN)
      return -1;

    CoroutineYieldEvent(cr, fd, EPOLLIN);
  }
}

ssize_t CoroutineWrite(Coroutine *cr, int fd, const void *buf, size_t count) {
  size_t written = 0;
  const char *ptr = (const char *)buf;
  while (written < count) {
    ssize_t res = write(fd, ptr + written, count - written);
    if (res > 0) {
      written += res;
      continue;
    }

    if (res < 0 && (errno == EWOULDBLOCK || errno == EAGAIN)) {
      CoroutineYieldEvent(cr, fd, EPOLLOUT);
      continue;
    }

    return -1;
  }

  return written;
}

int CoroutineAccept(Coroutine *cr, int sockfd, struct sockaddr *addr, socklen_t *addrlen) {
  while (1) {
    int sock = accept(sockfd, addr, addrlen);
    if (sock >= 0)
      return sock;

    if (sock < 0 && (errno == EWOULDBLOCK || errno == EAGAIN)) {
      CoroutineYieldEvent(cr, sockfd, EPOLLIN);
      continue;
    }

    return -1;
  }
}

int make_nonblocking(int fd) {
  int flags = fcntl(fd, F_GETFL);
  if (flags == -1)
    return -1;

  return fcntl(fd, F_SETFL, flags | O_NONBLOCK);
}
#endif
