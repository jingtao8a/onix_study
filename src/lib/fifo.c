#include "../include/fifo.h"
#include "../include/assert.h"
#include "../include/debug.h"

static _inline uint32 fifo_next(fifo_t *fifo, uint32 pos)
{
    return (pos + 1) % fifo->length;
}

void fifo_init(fifo_t *fifo, char *buf, uint32 length)
{
    fifo->buf = buf;
    fifo->length = length;
    fifo->head = 0;
    fifo->tail = 0;
}

bool fifo_full(fifo_t *fifo)
{
    bool full = (fifo_next(fifo, fifo->tail) == fifo->head);
    return full;
}

bool fifo_empty(fifo_t *fifo)
{
    return (fifo->head == fifo->tail);
}

char fifo_get(fifo_t *fifo)
{
    assert(!fifo_empty(fifo));
    fifo->head = fifo_next(fifo, fifo->head);
    char byte = fifo->buf[fifo->head];
    return byte;
}

void fifo_put(fifo_t *fifo, char byte)
{
    while (fifo_full(fifo))
    {
        fifo_get(fifo);
    }
    fifo->tail = fifo_next(fifo, fifo->tail);
    fifo->buf[fifo->tail] = byte;
    
}