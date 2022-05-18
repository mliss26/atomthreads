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
#define NUM_TEST_THREADS      2


/* State of test threads */
enum thread_state {
    IDLE,
    THREAD_WAIT,
    THREAD_RUN,
    THREAD_STOP,
};


/* Test OS objects */
static ATOM_EVENT event[NUM_TEST_THREADS];
static ATOM_TCB tcb[NUM_TEST_THREADS];
static uint8_t test_thread_stack[NUM_TEST_THREADS][TEST_THREAD_STACK_SIZE];


/* Test result tracking */
static volatile int g_result[NUM_TEST_THREADS];
static volatile enum thread_state g_state[NUM_TEST_THREADS];
static volatile uint32_t g_mask[NUM_TEST_THREADS];


/* Forward declarations */
static void test1_thread_func (uint32_t param);
static void test2_thread_func (uint32_t param);


/**
 * \b test_start
 *
 * Start event test.
 *
 * This test exercises the basic usage of events.
 *
 * @retval Number of failures
 */
uint32_t test_start (void)
{
    uint8_t status;
    int failures;
    uint32_t i, j, mask = 0, set_mask = 0;

    /* Default to zero failures */
    failures = 0;

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
            set_mask = (set_mask << 1) | 1;

            for (i = 0; i < 32-j; i++)
            {
                mask = set_mask << i;

                /* Wait for the other thread to start blocking on event */
                if (atomTimerDelay(SYSTEM_TICKS_PER_SEC/10) != ATOM_OK)
                {
                    ATOMLOG (_STR("Failed timer delay\n"));
                    failures++;
                }
                else
                {
                    /* clear global state */
                    g_result[0] = 0;
                    g_mask[0] = 0;

                    if (g_state[0] != THREAD_WAIT)
                    {
                        ATOMLOG (_STR("Thread1 not in correct state (%d)\n"), g_state[0]);
                        failures++;
                    }

                    /* Set the event with the current mask */
                    status = atomEventSet(&event[0], mask);
                    if (status != ATOM_OK)
                    {
                        ATOMLOG (_STR("Failed event set (%d) (0x%lx)\n"), status, mask);
                        failures++;
                    }
                    else
                    {
                        /* The thread should now wake up and set g_result. */
                        atomTimerDelay (SYSTEM_TICKS_PER_SEC/20);
                        if (g_result[0] == 0 || g_state[0] != THREAD_RUN)
                        {
                            ATOMLOG (_STR("Notify fail\n"));
                            failures++;
                        }
                        /* Check that the proper mask was returned from wait */
                        else if (mask != g_mask[0])
                        {
                            ATOMLOG (_STR("Mask fail: expected 0x%lx, got 0x%lx\n"), mask, g_mask[0]);
                            failures++;
                        }
                        else
                        {
                            /* Success */
                        }
                    }
                }
            }
        }
    }

    /* Test wakeup of thread waiting for event with specified timeout */
    if (atomEventCreate (&event[1]) != ATOM_OK)
    {
        ATOMLOG (_STR("Error creating test event\n"));
        failures++;
    }
    else if (atomThreadCreate(&tcb[1], TEST_THREAD_PRIO, test2_thread_func, 0,
              &test_thread_stack[1][0],
              TEST_THREAD_STACK_SIZE, TRUE) != ATOM_OK)
    {
        /* Fail */
        ATOMLOG (_STR("Error creating test thread 2\n"));
        failures++;
    }
    else
    {
        /*
         * We have created an event and a test thread waiting on it. We
         * want to check that the thread is woken up when expected with
         * the correct event mask.
         */
        for (i = 0; i < 32; i++)
        {
            mask = 1UL << i;

            /* Wait for the other thread to start blocking on event */
            if (atomTimerDelay(SYSTEM_TICKS_PER_SEC/5) != ATOM_OK)
            {
                ATOMLOG (_STR("Failed timer delay\n"));
                failures++;
            }
            else
            {
                /* clear global state */
                g_result[1] = 0;
                g_mask[1] = 0;

                if (g_state[1] != THREAD_WAIT)
                {
                    ATOMLOG (_STR("Thread2 not in correct state (%d)\n"), g_state[1]);
                    failures++;
                }

                /* Set the event with the current mask */
                status = atomEventSet(&event[1], mask);
                if (status != ATOM_OK)
                {
                    ATOMLOG (_STR("Failed event set (%d) (0x%lx)\n"), status, mask);
                    failures++;
                }
                else
                {
                    /* The thread should now wake up and set g_result. */
                    atomTimerDelay (SYSTEM_TICKS_PER_SEC/20);
                    if (g_result[1] == 0 || g_state[1] != THREAD_RUN)
                    {
                        ATOMLOG (_STR("Notify fail\n"));
                        failures++;
                    }
                    /* Check that the proper mask was returned from wait */
                    else if (mask != g_mask[1])
                    {
                        ATOMLOG (_STR("Mask fail: expected 0x%lx, got 0x%lx\n"), mask, g_mask[1]);
                        failures++;
                    }
                    else
                    {
                        /* Success */
                    }
                }
            }
        }
        /* Inform test2 thread to stop */
        g_state[1] = THREAD_STOP;
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
        g_state[0] = THREAD_WAIT;
        status = atomEventWait(&event[0], 0xffffffff, &value, 0);
        g_state[0] = THREAD_RUN;
        if (status != ATOM_OK)
        {
            ATOMLOG (_STR("Test1 thread woke with error (%d)\n"), status);
        }
        else
        {
            status = atomEventClear(&event[0], value);
            if (status != ATOM_OK)
            {
                ATOMLOG (_STR("Test1 event clear error (%d)\n"), status);
            }
            else
            {
                /* We were woken as expected, set g_result and g_mask to notify success */
                g_mask[0] = value;
                g_result[0] = 1;
            }
        }

        atomTimerDelay(SYSTEM_TICKS_PER_SEC/10);
    }
}


/**
 * \b test2_thread_func
 *
 * Entry point for test thread 2.
 *
 * @param[in] param Unused (optional thread entry parameter)
 *
 * @return None
 */
static void test2_thread_func (uint32_t param)
{
    uint8_t status;
    uint32_t value;

    /* Compiler warnings */
    param = param;

    while (1)
    {
        if (g_state[1] != THREAD_STOP)
        {
            /*
             * Wait on event with timeout. We are expecting to be woken up
             * by the main thread while blocking.
             */
            g_state[1] = THREAD_WAIT;
            status = atomEventWait(&event[1], 0xffffffff, &value, SYSTEM_TICKS_PER_SEC);
            g_state[1] = THREAD_RUN;
            if (status != ATOM_OK)
            {
                ATOMLOG (_STR("Test2 thread woke with error (%d)\n"), status);
            }
            else
            {
                status = atomEventClear(&event[1], value);
                if (status != ATOM_OK)
                {
                    ATOMLOG (_STR("Test1 event clear error (%d)\n"), status);
                }
                else
                {
                    /* We were woken as expected, set g_result and g_mask to notify success */
                    g_mask[1] = value;
                    g_result[1] = 1;
                }
            }
        }

        atomTimerDelay(SYSTEM_TICKS_PER_SEC/10);
    }
}
