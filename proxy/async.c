#include <stddef.h> // NULL, size_t
#include <stdbool.h> // bool, true, false
#include <assert.h> // assert
#include <errno.h> // errno
#include <stdio.h> // perror, fprintf, stderr
#include <stdlib.h> // abort, exit
#include <string.h> // strerror

#include <pthread.h> // pthread_mutex_t, pthread_t, PTHREAD_MUTEX_INITIALIZER, pthread_create, pthread_mutex_lock, pthread_mutex_unlock, pthread_join
#include <unistd.h> // pipe, close, write, read, ssize_t
#include <poll.h> // poll, struct pollfd, POLLIN

#include "./atem_server.h" // atem_server_recv
#include "./timeout.h" // timeout_next, timeout_dispatch
#include "./async.h" // struct async_task, enum async_loop_index

// Indexes used when registering file descriptors in the background processes main loop
enum async_loop_index {
	ASYNC_LOOP_INDEX_DISPATCH,
	ASYNC_LOOP_INDEX_SERVER,
	ASYNC_LOOP_INDEX_MAX
};

// Global dispatch queue for sending functions with optional arguments to run in the background thread
static struct {
	struct async_task* head;
	struct async_task* tail;
	pthread_mutex_t mutex;
	pthread_t thread;
	int pipe_write_fd;
} async_ctx = {
	.mutex = PTHREAD_MUTEX_INITIALIZER,
};

// All file descriptors to poll in background threads main loop
static struct pollfd async_loop_fds[ASYNC_LOOP_INDEX_MAX];

// Processes all tasks in dispatch queue
static void async_process(void) {
	// Empties pipe that signaled the dispatch queue has data
	char buf[1024];
	ssize_t read_len = read(async_loop_fds[ASYNC_LOOP_INDEX_DISPATCH].fd, buf, sizeof(buf));
	if (read_len == -1) {
		perror("Failed to read from pipe");
		return;
	}
	assert(read_len == 1);

	// Acquires the mutex protecting the dispatch queue before processing it
	int err = pthread_mutex_lock(&async_ctx.mutex);
	if (err != 0) {
		fprintf(stderr, "Failed to lock async mutex: %s\n", strerror(err));
		return;
	}

	// Processes all tasks in dispatch queue
	while (async_ctx.head) {
		struct async_task* task = async_ctx.head;
		async_ctx.head = task->next;
		task->fn(task);
	}

	// Releases the mutex protecting the dispatch queue after processing is done
	err = pthread_mutex_unlock(&async_ctx.mutex);
	assert(err == 0);
}

// @todo
void async_loop_next(void) {
	// Waits for file descriptors to have data available for read or timeout
	int timeout = timeout_next();
	assert(timeout != 0);
	assert(timeout >= -1);
	int poll_len = poll(async_loop_fds, ASYNC_LOOP_INDEX_MAX, timeout);
	if (poll_len == -1) {
		perror("Failed to poll");
		abort();
	}
	assert(poll_len >= 0);
	assert(poll_len <= ASYNC_LOOP_INDEX_MAX);

	// Processes tasks in dispatch queue if signaled from main thread there are tasks available
	if (async_loop_fds[ASYNC_LOOP_INDEX_DISPATCH].revents) {
		async_process();
	}

	// Processes ATEM packet from a client to the proxy server
	if (async_loop_fds[ASYNC_LOOP_INDEX_SERVER].revents) {
		atem_server_recv();
	}
}

// Background threads main loop function
static void* async_loop(void* arg) {
	(void)arg;
	while (true) {
		async_loop_next();
	}
}

// Enables the ATEM proxy server to be polled for the async background run loop
void async_loop_server_enable(void) {
	assert(atem_server.sock > 0);
	assert(async_loop_fds[ASYNC_LOOP_INDEX_SERVER].fd == -1);
	async_loop_fds[ASYNC_LOOP_INDEX_SERVER].fd = atem_server.sock;
}

// Removes the ATEM proxy server from being polled in the async background run loop
void async_loop_server_disable(void) {
	assert(async_loop_fds[ASYNC_LOOP_INDEX_SERVER].fd == atem_server.sock);
	async_loop_fds[ASYNC_LOOP_INDEX_SERVER].fd = -1;
}



// Starts main loop in background process
bool async_init(void) {
	int err;

	// Creates unidirectional pipe to signal background process when tasks are available in the dispatch queue
	int pipe_fds[2];
	err = pipe(pipe_fds);
	if (err == -1) {
		return false;
	}
	assert(err == 0);
	async_ctx.pipe_write_fd = pipe_fds[1];
	async_loop_fds[ASYNC_LOOP_INDEX_DISPATCH].fd = pipe_fds[0];

	// Sets initial value of file descriptors to poll in background threads main loop to be deactivated
	async_loop_fds[ASYNC_LOOP_INDEX_SERVER].fd = -1;

	// Enables all file descriptors to poll in background threads main loop for reading
	for (size_t i = 0; i < (sizeof(async_loop_fds) / sizeof(*async_loop_fds)); i++) {
		async_loop_fds[i].events = POLLIN;
		if (i == ASYNC_LOOP_INDEX_DISPATCH) {
			assert(async_loop_fds[i].fd != -1);
		}
		else {
			assert(async_loop_fds[i].fd == -1);
		}
	}

	// Launches the background process
	err = pthread_create(&async_ctx.thread, NULL, &async_loop, NULL);
	if (err != 0) {
		close(pipe_fds[0]);
		close(pipe_fds[1]);
		errno = err;
		return false;
	}
	assert(err == 0);

	return true;
}

/** Releases background thread and the communication resources
 * @attention Does not release any resources used in the thread
 */
void async_release(void) {
	int err = close(async_ctx.pipe_write_fd);
	if (err == 0) {
		perror("Failed to close dispatch queue write pipe");
	}
	err = close(async_loop_fds[ASYNC_LOOP_INDEX_DISPATCH].fd);
	if (err == 0) {
		perror("Failed to close dispatch queue read pipe");
	}
	err = pthread_cancel(async_ctx.thread);
	if (err == 0) {
		perror("Failed to close background thread");
	}
}

// Releases the background thread, blocking until thread is completely torn down
bool async_release_sync(void) {
	async_release();
	int err = pthread_join(async_ctx.thread, NULL);
	if (err != 0) {
		errno = err;
		return false;
	}
	return true;
}

/** Dispatches a task to the background thread
 * @attention This function should never be called in a task function. Doing so will lead to deadlocks
 * @attention The task has to be heap allocated and can not be used in the main thread after being dispatched
 * @returns Indicates success or failure, on failure errno is set
 */
bool async_dispatch(struct async_task* task, void (*fn)(struct async_task* task)) {
	assert(task != NULL);
	assert(fn != NULL);

	// Sets up the task before appending
	task->fn = fn;
	task->next = NULL;

	// Acquires the mutex protecting the dispatch queue before appending the task
	int err = pthread_mutex_lock(&async_ctx.mutex);
	if (err != 0) {
		errno = err;
		return false;
	}

	// Appends task to the dispatch queue and signals background thread there are tasks in the queue if not done already
	if (async_ctx.head == NULL) {
		async_ctx.head = task;
		ssize_t written = write(async_ctx.pipe_write_fd, "x", 1);
		if (written == -1) {
			return false;
		}
		assert(written == 1);
	}
	else {
		async_ctx.tail->next = task;
	}
	async_ctx.tail = task;

	// Releases the mutex protecting the dispatch queue after appending to it
	err = pthread_mutex_unlock(&async_ctx.mutex);
	assert(err == 0);

	return true;
}
