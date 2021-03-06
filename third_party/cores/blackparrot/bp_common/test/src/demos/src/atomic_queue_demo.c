#include <stdint.h>
#include "bp_utils.h"

#define QUEUE_LENGTH 32

#ifndef NUM_CORES
#define NUM_CORES 8
#endif

#define NUM_READERS (NUM_CORES/2)
#define NUM_WRITERS (NUM_CORES/2)
#define NUM_OPS 10

// this is just a circular queue
// we dequeue at the head index and enqueue from the tail index
typedef struct {
    uint64_t head_index;
    uint64_t tail_index;
    uint64_t lock_address;
    uint64_t queue_buffer[QUEUE_LENGTH];
} shared_queue;



// FIXME: we can change this into a struct if we
// want a more complex queue
typedef uint64_t queue_item;

// declare everything volatile, so loops don't optimize themselves away
volatile shared_queue queue;

volatile uint64_t start_barrier_mem = 0;
volatile uint64_t end_barrier_mem = 0;

void lock_queue(uint64_t core_id) {
    uint64_t swap_value = 1;

    do {
        __asm__ volatile("amoswap.d %0, %2, (%1)": "=r"(swap_value) 
                                                 : "r"(&(queue.lock_address)), "r"(swap_value)
                                                 :);
    } while (swap_value != 0);
}

void unlock_queue(uint64_t core_id) {
    uint64_t swap_value = 0;
    __asm__ volatile("amoswap.d %0, %2, (%1)": "=r"(swap_value) 
                                                 : "r"(&(queue.lock_address)), "r"(swap_value)
                                                 :);
}

// attempts to enqueue an item.
// returns 0 if successful
// returns 1 if unsuccessful (the queue was full)
uint64_t enqueue(uint64_t core_id, queue_item enqueue_item) {
    uint64_t status = 0;
    // we actually don't care if the head index changes between
    // us checking and us enqueuing because we don't need to
    // change things based on head index
    // We might miss a consumer dequeue, so we don't enqueue
    // immediately to fill an empty slot, but so what
    lock_queue(core_id);

    // if the queue is full, return an error
    // the queue is full
    if ((queue.tail_index + 1) % QUEUE_LENGTH == queue.head_index) {
        status = 1;
        goto done_enqueue;
    }

    // FIXME: if a queue_item becomes a struct, change to copying
    // in the struct fields
    queue.queue_buffer[queue.tail_index] = enqueue_item;

    // update the tail index at the very end, so if a consumer
    // checks to dequeue, the item will be ready
    queue.tail_index = (queue.tail_index + 1) % QUEUE_LENGTH;
    //printf("Core %d enqueued %d\n", core_id, enqueue_item);

done_enqueue:
    unlock_queue(core_id);
    return status;
}

// attempts to dequeue an item.
// returns 0 if successful
// returns 1 if unsuccessful (the queue was full)
// the item is returned in dequeue_item
uint64_t dequeue(uint64_t core_id, queue_item *dequeue_item) {
    uint64_t status = 0;
    uint64_t print_item;

    // we actually don't care if the tail index changes between
    // us checking and us dequeuing because we don't need to
    // change things based on tail index
    // We might miss a producer enqueue, so we don't dequeue
    // a new item immediately, but so what
    lock_queue(core_id);

    // check if the queue is empty
    if (queue.tail_index == queue.head_index) {
        status = 1;
        goto done_dequeue;
    }

    // FIXME: if a queue_item becomes a struct, change to copying in the struct fields
    *dequeue_item = queue.queue_buffer[queue.head_index];

    // update the head index at the very end, so if a producer checks to
    // enqueue, the item will have already been copied out
    queue.head_index = (queue.head_index + 1) % QUEUE_LENGTH;
    print_item = *dequeue_item;
    bp_hprint(print_item);

done_dequeue:
    unlock_queue(core_id);
    return status;
}

// TODO: what actually happens when we enqueue and dequeue
// this will eventually become the function given to pthread_create
uint64_t thread_main() {
    // do some stuff, probably dependent on core ID (enqueue vs dequeue)
    // find some inline assembly to read the hartID csr
    uint64_t data = 0;
    uint64_t core_id;
    uint64_t status;
    uint64_t num_operations = 0;
    
    __asm__ volatile("csrr %0, mhartid": "=r"(core_id): :);
    // have the even cores enqueue and the odd ones dequeue
    if (core_id % 2 == 0){
        while(num_operations < NUM_OPS) {
            status = enqueue(core_id, data);
            if (status == 0) {
                data++;
                num_operations++;
                //TODO: do something on enqueue
            }
        }
    }
    else {
        while(num_operations < NUM_OPS) {
            status = dequeue(core_id, &data);
            if (status == 0) {
                // TODO: do something with dequeued data...maybe print?
                num_operations++;
            }
        }
    }
}

uint64_t main(uint64_t argc, char * argv[]) {
    uint64_t i;
    uint64_t core_id;
    __asm__ volatile("csrr %0, mhartid": "=r"(core_id): :);
   
    // only core 0 intializes data structures
    if (core_id == 0) {
        // initialize queue lock
        queue.lock_address = 0;
        // initialize queue structure
        queue.head_index = 0;
        queue.tail_index = 0;
        for (i = 0; i < QUEUE_LENGTH; i++) {
            queue.queue_buffer[i] = 0xdeadbeef;
        }
        start_barrier_mem = 0xdeadbeef;
    }
    else {
        while (start_barrier_mem != 0xdeadbeef) { }
    }
    thread_main();

    bp_finish(0);

    return 0;
}
