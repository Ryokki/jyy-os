#include <stdlib.h>
#include <stdio.h>
#include <assert.h>
#include <pthread.h>

struct thread {
  int id;   // 从1开始的线程号
  pthread_t thread; // pthreads 线程号
  void (*entry)(int);   // 线程的入口函数
  struct thread *next; // 链表
};

struct thread *threads; // 当前最新的线程

void (*join_fn)();  // join 后执行的函数

// ========== Basics ==========

__attribute__((destructor)) static void join_all() {
  // 遍历所有的thread,去执行pthread_join,并释放各个threads结构体
  for (struct thread *next; threads; threads = next) {  
    pthread_join(threads->thread, NULL);
    next = threads->next;
    free(threads);
  }
  // 执行join中定义的函数
  join_fn ? join_fn() : (void)0;
}

static inline void *entry_all(void *arg) {
  struct thread *thread = (struct thread *)arg;
  thread->entry(thread->id);
  return NULL;
}

/* 创建线程并立即执行,入口代码为  void fn(int tid) */
static inline void create(void *fn) {
  struct thread *cur = (struct thread *)malloc(sizeof(struct thread));  
  assert(cur);
  cur->id    = threads ? threads->id + 1 : 1; 
  cur->next  = threads;
  cur->entry = (void (*)(int))fn;
  threads    = cur;
  pthread_create(&cur->thread, NULL, entry_all, cur);
  /**
   * pthread_create
   * @arg[1] pthread_t *thread     // 返回线程号
   * @arg[2] pthread_attr_t *attr   // 这个NULL即可
   * @arg[3] void *(*start_routine) (void *)    // 调用的函数
   * @arg[4] void *arg      // 参数
   */
}

/* 等待所有线程执行结束后，调用 fn */
static inline void join(void (*fn)()) {
  join_fn = fn;
}

// ========== Synchronization ==========

#include <stdint.h>

intptr_t atomic_xchg(volatile intptr_t *addr,
                               intptr_t newval) {
  // swap(*addr, newval);
  intptr_t result;
  asm volatile ("lock xchg %0, %1":
    "+m"(*addr), "=a"(result) : "1"(newval) : "cc");
  return result;
}

intptr_t locked = 0;

static inline void lock() {
  while (1) {
    intptr_t value = atomic_xchg(&locked, 1);
    if (value == 0) {
      break;
    }
  }
}

static inline void unlock() {
  atomic_xchg(&locked, 0);
}

#include <semaphore.h>

#define P sem_wait
#define V sem_post
#define SEM_INIT(sem, val) sem_init(&(sem), 0, val)