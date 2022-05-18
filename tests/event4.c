/*
 * Copyright (c) 2022, Matt Liss
 * Copyright (c) 2010, Kelvin Lawson. All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 *
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 * 3. No personal names or organizations' names associated with the
 *    Atomthreads project may be used to endorse or promote products
 *    derived from this software without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE ATOMTHREADS PROJECT AND CONTRIBUTORS
 * "AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED
 * TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR
 * PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE PROJECT OR CONTRIBUTORS BE
 * LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR
 * CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF
 * SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS
 * INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN
 * CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE)
 * ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE
 * POSSIBILITY OF SUCH DAMAGE.
 */


#include "atom.h"
#include "atomevent.h"
#include "atomtests.h"


/* Number of test threads */
#define NUM_TEST_THREADS      1


/* Test OS objects */
static ATOM_EVENT event[NUM_TEST_THREADS];
static ATOM_TCB tcb[NUM_TEST_THREADS];
static uint8_t test_thread_stack[NUM_TEST_THREADS][TEST_THREAD_STACK_SIZE];


/* Test result tracking */
static volatile uint32_t g_mask[NUM_TEST_THREADS*2];
static volatile int g_result[NUM_TEST_THREADS];


/* Forward declarations */
static void test1_thread_func (uint32_t param);


/**
 * \b test_start
 *
 * Start event test.
 *
 * This test exercises setting events not in the wait mask to ensure the wait
 * is not ended unexpectedly.
 *
 * @retval Number of failures
 */
uint32_t test_start (void)
{
    uint8_t status;
    int failures;
    uint32_t i, j, mask;

    /* Default to zero failures */
    failures = 0;

    /* Set global mask for first iteration before we create the test thread */
    g_mask[0] = 1;

    /* Test wakeup of thread waiting for event with infinite timeout */
    if (atomEventCreate (&event[0]) != ATOM_OK)
    {
        ATOMLOG (_STR("Error creating test event\n"));
        failures++;
    }
    else if (atomThreadCreate(&tcb[0], TEST_THREAD_PRIO, test1_thread_func, 0,
              &test_thread_stack[0][0],
              TEST_THREAD_STACK_SIZE, TRUE) != ATOM_OK)
    {
        /* Fail */
        ATOMLOG (_STR("Error creating test thread 1\n"));
        failures++;
    }
    else
    {
        /*
         * We have created an event and a test thread waiting on it. We
         * want to check that the thread is woken up when expected with
         * the correct event mask.
         */
        for (j = 0; j < 32; j++)
        {
            /* set mask for test thread to wait on */
            g_mask[0] = 1UL << j;

            for (i = 0; i < 32; i++)
            {
                /* set mask to test */
                mask = 1UL << i;

                /* don't want to wake the thread */
                if (mask == g_mask[0])
                    continue;

                /* set global state for iteration */
                g_result[0] = 0;

                /* Wait for the other thread to start blocking on event */
                if (atomTimerDelay(SYSTEM_TICKS_PER_SEC/10) != ATOM_OK)
                {
                    ATOMLOG (_STR("Failed timer delay\n"));
                    failures++;
                }
                else
                {
                    /* Set the event with the current mask */
                    status = atomEventSet(&event[0], mask);
                    if (status != ATOM_OK)
                    {
                        ATOMLOG (_STR("Failed event set (%d) (0x%lx)\n"), status, mask);
                        failures++;
                    }
                    else
                    {
                        /* The thread should not wake up and set g_result. */
                        atomTimerDelay (SYSTEM_TICKS_PER_SEC/20);
                        if (g_result[0] == 1)
                        {
                            ATOMLOG (_STR("Thread woke unexpectedly with event 0x%lx\n"), g_mask[1]);
                            failures++;
                        }
                        else
                        {
                            /* Success */
                        }
                    }
                }
            }

            /* Wake up thread to move on to the next mask */
            status = atomEventSet(&event[0], (1UL << j));
            if (status != ATOM_OK)
            {
                ATOMLOG (_STR("Failed event set (%d) (0x%lx)\n"), status, mask);
                failures++;
            }
            atomTimerDelay (SYSTEM_TICKS_PER_SEC/20);
        }
    }

    /* Check thread stack usage (if enabled) */
#ifdef ATOM_STACK_CHECKING
    {
        uint32_t used_bytes, free_bytes;
        int thread;

        /* Check all threads */
        for (thread = 0; thread < NUM_TEST_THREADS; thread++)
        {
            /* Check thread stack usage */
            if (atomThreadStackCheck (&tcb[thread], &used_bytes, &free_bytes) != ATOM_OK)
            {
                ATOMLOG (_STR("StackCheck\n"));
                failures++;
            }
            else
            {
                /* Check the thread did not use up to the end of stack */
                if (free_bytes == 0)
                {
                    ATOMLOG (_STR("StackOverflow %d\n"), thread);
                    failures++;
                }

                /* Log the stack usage */
#ifdef TESTS_LOG_STACK_USAGE
                ATOMLOG (_STR("StackUse:%d\n"), (int)used_bytes);
#endif
            }
        }
    }
#endif

    /* Quit */
    return failures;

}


/**
 * \b test1_thread_func
 *
 * Entry point for test thread 1.
 *
 * @param[in] param Unused (optional thread entry parameter)
 *
 * @return None
 */
static void test1_thread_func (uint32_t param)
{
    uint8_t status;
    uint32_t value;

    /* Compiler warnings */
    param = param;

    while (1)
    {
        /*
         * Wait on event with no timeout. We are expecting to be woken up
         * by the main thread while blocking.
         */
        status = atomEventWait(&event[0], g_mask[0], &value, 0);
        if (status != ATOM_OK)
        {
            ATOMLOG (_STR("Test1 thread woke with error (%d)\n"), status);
        }
        else
        {
            status = atomEventClear(&event[0], 0xffffffff);
            if (status != ATOM_OK)
            {
                ATOMLOG (_STR("Test1 event clear error (%d)\n"), status);
            }
            else
            {
                /* We were woken as expected, set g_result and g_mask to notify success */
                g_mask[1] = value;
                g_result[0] = 1;
            }
        }

        atomTimerDelay(SYSTEM_TICKS_PER_SEC/10);
    }
}
