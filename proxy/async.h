// Include guard
#ifndef ASYNC_H
#define ASYNC_H

#include <stdbool.h> // bool

// Used when dispatching tasks to the background process
struct async_task {
	struct async_task* next;
	void (*fn)(struct async_task* task);
};

void async_loop_next(void);
void async_loop_server_enable(void);
void async_loop_server_disable(void);

bool async_init(void);
void async_release(void);
bool async_release_sync(void);

bool async_dispatch(struct async_task* task, void (*fn)(struct async_task* task));

#endif // ASYNC_H
