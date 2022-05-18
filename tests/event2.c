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


/* Test OS objects */
static ATOM_EVENT event;
static ATOM_TCB tcb[NUM_TEST_THREADS];
static uint8_t test_thread_stack[NUM_TEST_THREADS][TEST_THREAD_STACK_SIZE];


/* Test result tracking */
static volatile int g_result;


/* Forward declarations */
static void test1_thread_func (uint32_t param);
static void test2_thread_func (uint32_t param);


/**
 * \b test_start
 *
 * Start event test.
 *
 * This test exercises the event creation and deletion APIs, including
 * waking threads waiting on an event if the event is deleted.
 * Deletion wakeups are tested twice: once for a thread which is blocking
 * with a timeout and once for a thread which is blocking with no timeout.
 *
 * @retval Number of failures
 */
uint32_t test_start (void)
{
    int failures;
    uint32_t i;

    /* Default to zero failures */
    failures = 0;

    /* Test creation and deletion of events: good values */
    for (i = 0; i < 1000; i++)
    {
        if (atomEventCreate (&event) == ATOM_OK)
        {
            if (atomEventDelete (&event) == ATOM_OK)
            {
                /* Success */
            }
            else
            {
                /* Fail */
                ATOMLOG (_STR("Error deleting event\n"));
                failures++;
                break;
            }
        }
        else
        {
            /* Fail */
            ATOMLOG (_STR("Error creating event\n"));
            failures++;
            break;
        }
    }

    /* Test wakeup of threads on event deletion (thread blocking with no timeout) */
    g_result = 0;
    if (atomEventCreate (&event) != ATOM_OK)
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
         * want to see that the other thread is woken up if its event
         * is deleted. This is indicated through g_result being set.
         */

        /* Wait for the other thread to start blocking on event */
        if (atomTimerDelay(SYSTEM_TICKS_PER_SEC) != ATOM_OK)
        {
            ATOMLOG (_STR("Failed timer delay\n"));
            failures++;
        }
        else
        {
            /* The other thread will be blocking on event now, delete it */
            if (atomEventDelete(&event) != ATOM_OK)
            {
                ATOMLOG (_STR("Failed event delete\n"));
                failures++;
            }
            else
            {
                /* event deleted. The thread should now wake up and set g_result. */
                atomTimerDelay (SYSTEM_TICKS_PER_SEC);
                if (g_result == 0)
                {
                    ATOMLOG (_STR("Notify fail\n"));
                    failures++;
                }
                else
                {
                    /* Success */
                }
            }
        }
    }

    /* Test wakeup of threads on event deletion (thread blocking with timeout) */
    g_result = 0;
    if (atomEventCreate (&event) != ATOM_OK)
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
         * want to see that the other thread is woken up if its event
         * is deleted. This is indicated through g_result being set.
         */

        /* Wait for the other thread to start blocking on event */
        if (atomTimerDelay(SYSTEM_TICKS_PER_SEC) != ATOM_OK)
        {
            ATOMLOG (_STR("Failed timer delay\n"));
            failures++;
        }
        else
        {
            /* The other thread will be blocking on event now, delete it */
            if (atomEventDelete(&event) != ATOM_OK)
            {
                ATOMLOG (_STR("Failed event delete\n"));
                failures++;
            }
            else
            {
                /* event deleted. The thread should now wake up and set g_result. */
                atomTimerDelay (SYSTEM_TICKS_PER_SEC);
                if (g_result == 0)
                {
                    ATOMLOG (_STR("Notify fail\n"));
                    failures++;
                }
                else
                {
                    /* Success */
                }
            }
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

    /*
     * Wait on event with no timeout. We are expecting to be woken up
     * by the main thread while blocking.
     */
    status = atomEventWait(&event, 0xffffffff, &value, 0);
    if (status != ATOM_ERR_DELETED)
    {
        ATOMLOG (_STR("Test1 thread woke without deletion (%d)\n"), status);
    }
    else
    {
        /* We were woken due to deletion as expected, set g_result to notify success */
        g_result = 1;
    }

    /* Wait forever */
    while (1)
    {
        atomTimerDelay (SYSTEM_TICKS_PER_SEC);
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

    /*
     * Wait on event with timeout. We are expecting to be woken up
     * by the main thread while blocking.
     */
    status = atomEventWait(&event, 0xffffffff, &value, (5 * SYSTEM_TICKS_PER_SEC));
    if (status != ATOM_ERR_DELETED)
    {
        ATOMLOG (_STR("Test2 thread woke without deletion (%d)\n"), status);
    }
    else
    {
        /* We were woken due to deletion as expected, set g_result to notify success */
        g_result = 1;
    }

    /* Wait forever */
    while (1)
    {
        atomTimerDelay (SYSTEM_TICKS_PER_SEC);
    }
}
