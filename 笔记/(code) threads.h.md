# 迷你线程库 threads.h

我们对 POSIX 线程做了一定的封装，这样可以更方便地写一些 (单文件的) 小程序，实际体验一下多线程编程。代码参考 [`threads.h`](http://jyywiki.cn/pages/OS/2021/demos/threads.h)，它只有两个 API：
```c
void create(void *fn);   // 创建一个线程立即执行，入口代码为 void fn(int tid);
void join(void (*fn)()); // 等待所有线程执行结束后，调用 fn
```

## thread结构体
我们把thread包装成了一个struct
```c
struct thread {
  int id;   // 从1开始的线程号
  pthread_t thread; // pthreads 线程号
  void (*entry)(int);   // 线程的入口函数
  struct thread *next; // 链表 (之所以需要这个,是因为join的时候要遍历这个链表去join所有)
};

struct thread *threads; // 当前最新的线程
```

## 创建线程 create
线程创建非常简单，分配一个 struct thread 对象，并插入链表头部，然后使用 pthread_create 创建一个 POSIX 线程
```c
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
```

可以看到,pthread_create中执行的函数为`entry_all `, 参数为`struct thread cur`，看看entry_all是怎么实现的

`entry_all`：
这里所做的就是从thread结构体中拿出entry和id，调用entry(id)来真正执行。由于前面用fn来赋值给entry，所以其实是去执行fn(id).
```c
static inline void *entry_all(void *arg) {
  struct thread *thread = (struct thread *)arg;
  thread->entry(thread->id);
  return NULL;
}
```

## 等待线程 join
在 `main` 函数返回后，调用 `join_all`，其中对所有线程调用 `pthread_join` 等待完成，最后调用注册的回调函数 (callback)
```c
void (*join_fn)();  // join 后执行的函数

/* 等待所有线程执行结束后，调用 fn */
void join(void (*fn)()) {
  join_fn = fn;
}

/* 等所有线程join到后，执行用户定义的函数join_fn() */
__attribute__((destructor)) void join_all() {
  // 遍历所有的thread,去执行pthread_join,并释放各个threads结构体
  for (struct thread *next; threads; threads = next) {  
    pthread_join(threads->thread, NULL);
    next = threads->next;
    free(threads);
  }
  // 执行join中定义的函数
  join_fn ? join_fn() : (void)0;
}
```

