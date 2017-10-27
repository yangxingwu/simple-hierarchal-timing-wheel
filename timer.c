#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>
#include <stdbool.h>

#include "list.h"

typedef void (*timer_cb)(void *arg);

struct timer {
    struct list_head list;
    uint32_t delay;
    timer_cb cb;
    void *arg;
};

#define LEVEL_SIZE      4
#define LEVEL_DEPTH     3
#define TIMER_MAX_DELAY 64 /* 4 ^3 */

struct wheel {
    uint32_t cursors[LEVEL_DEPTH];
    struct list_head *hashs[LEVEL_DEPTH];
};

static uint32_t get_level_delays(int level)
{
    uint32_t delays = 1;
    while (--level >= 0)
        delays *= LEVEL_SIZE;
    return delays;
}

static int init_wheel(struct wheel *w)
{
    int i;
    int j;
    for (i = 0; i < LEVEL_DEPTH; i++) {
        w->cursors[i] = 0;
        w->hashs[i] =
            (struct list_head *)malloc(sizeof(struct list_head) * LEVEL_SIZE);
        if (w->hashs[i] == NULL)
            return -1;
        for (j = 0; j < LEVEL_SIZE; j++)
            INIT_LIST_HEAD(&(w->hashs[i][j]));
    }
    return 0;
}

static void init_timer(struct timer *t, uint32_t delay, timer_cb cb, void *arg)
{
    INIT_LIST_HEAD(&(t->list));
    t->delay = delay;
    t->cb = cb;
    t->arg = arg;
}

static int start_timer(struct timer *t, struct wheel *w)
{
    int level;
    uint32_t off;
    uint32_t hash;

    if (t->delay >= TIMER_MAX_DELAY) {
        printf("exceed timer range\n");
        return -1;
    }

    if (t->delay == 0) {
        printf("schedule 0 timeout timer.\n");
        return -1;
    }

    for (level = LEVEL_DEPTH - 1; level >= 0; level--) {
        off = t->delay / get_level_delays(level);
        if (off > 0) {
            hash = (w->cursors[level] + off) % LEVEL_SIZE;
            list_add_tail(&(t->list), &(w->hashs[level][hash]));
            /* store the remainder */
            t->delay = t->delay % get_level_delays(level);
            for (level = level - 1; level >=0; level--)
                t->delay += w->cursors[level] * get_level_delays(level);
            return 0;
        }
    }

    printf("timer not adopted\n");
    return -1;
}

static void run_wheel(struct wheel *w)
{
    int level;
    int lower;
    uint32_t *cursor;
    uint32_t off;
    uint32_t hash;
    bool carry;
    struct timer *t;
    struct timer *n;

    for (level = 0; level < LEVEL_DEPTH; level ++) {
        cursor = &(w->cursors[level]);
        (*cursor)++;

        if ((*cursor) < LEVEL_SIZE) {
            carry = false;
        } else {
            (*cursor) = 0;
            carry = true;
        }

        list_for_each_entry_safe(t, n, &(w->hashs[level][*cursor]), list) {
            if (t->delay == 0) {
                t->cb(t->arg);
                list_del(&(t->list));
            } else {
                /* drop to lower level */
                list_del(&(t->list));

                for (lower = level; lower >= 0; lower--) {
                    off = t->delay / get_level_delays(lower);
                    if (off > 0) {
                        hash = (w->cursors[lower] + off) % LEVEL_SIZE;
                        list_add_tail(&(t->list), &(w->hashs[lower][hash]));
                        /* store the remainder */
                        t->delay = t->delay % get_level_delays(lower);
                        /*
                         * all lower cursor must be 0
                         * so it's not necessary to calculate the offset
                         * see start_timer
                         */
                        break;
                    }
                }
            }
        }

        if (!carry)
            break;
    }
}

static void print_delay(void *arg)
{
    printf("\tdelay %d\n", *((int *)arg));
}

int main()
{
    int *arg;
    struct wheel w;

    init_wheel(&w);

    uint32_t i;
    for (i = 1; i < TIMER_MAX_DELAY; i++) {
        struct timer *t = malloc(sizeof(struct timer));
        arg = malloc(sizeof(int));
        *arg = (int)i;
        init_timer(t, i, print_delay, arg);
        start_timer(t, &w);
    }

    int cnt = 1;
    static int add = 0;
    while (1) {
        printf("time %d\n", cnt);
        run_wheel(&w);
        {
            if (add < 10) {
                struct timer *t = malloc(sizeof(struct timer));
                i = random() % TIMER_MAX_DELAY;
                arg = malloc(sizeof(int));
                *arg = (int)i;
                init_timer(t, i, print_delay, arg);
                printf("\t\tadd timer ii %d\n", i);
                start_timer(t, &w);
                add++;
            }
        }
        cnt++;
        if (cnt == 128)
            break;
    }

    return 0;
}
