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


/**
 * \file
 * Event library.
 *
 * This module implements an event signalling library with the following
 * features:
 *
 * \par Flexible blocking APIs
 * Threads which wish to wait on an event can choose whether to block,
 * block with timeout, or not block if the event is not signalled.
 *
 * \par Interrupt-safe calls
 * All APIs can be called from interrupt context. Any calls which could
 * potentially block have optional parameters to prevent blocking if you
 * wish to call them from interrupt context. Any attempt to make a call
 * which would block from interrupt context will be automatically and
 * safely prevented.
 *
 * \par Smart event deletion
 * Where an event is deleted while a thread is blocking on it, the blocking
 * thread is woken and returned a status code to indicate the reason for
 * being woken.
 *
 *
 * \n <b> Usage instructions: </b> \n
 *
 * All event objects must be initialised before use by calling
 * atomEventCreate(). Once initialised atomEventSet() and atomEventClear() are
 * used to set or clear the individual event flags, respectively.
 * One thread at a time can wait for one or more flags in an event with
 * atomEventWait(). A call to atomEventSet() with a specific flag bit set will
 * wake up a thread which is waiting on the same event if it specified a flag
 * mask containing the same bit.
 *
 * An event which is no longer required can be deleted using
 * atomEventDelete(). This function automatically wakes up any threads which are
 * waiting on the deleted event.
 *
 */


#include "atom.h"
#include "atomevent.h"
#include "atomtimer.h"


/* Local data types */

/* Forward declarations */

static void atomEventTimerCallback (POINTER cb_data);


/**
 * \b atomEventCreate
 *
 * Initialises an event object.
 *
 * Must be called before calling any other event library routines on an
 * event. Objects can be deleted later using atomEventDelete().
 *
 * Does not allocate storage, the caller provides the event object.
 *
 * This function can be called from interrupt context.
 *
 * @param[in] event Pointer to event object
 *
 * @retval ATOM_OK Success
 * @retval ATOM_ERR_PARAM Bad parameters
 */
uint8_t atomEventCreate (ATOM_EVENT *event)
{
    uint8_t status;

    /* Parameter check */
    if (event == NULL)
    {
        /* Bad event pointer */
        status = ATOM_ERR_PARAM;
    }
    else
    {
        /* Set the initial flags */
        event->flags = 0;
        event->mask = 0;

        /* Initialise the suspended thread */
        event->tcb_ptr = NULL;

        /* Successful */
        status = ATOM_OK;
    }

    return (status);
}


/**
 * \b atomEventDelete
 *
 * Deletes an event object.
 *
 * Any thread currently suspended on the event will be woken up with
 * return status ATOM_ERR_DELETED. If called at thread context then the
 * scheduler will be called during this function which may schedule in one
 * of the woken threads depending on relative priorities.
 *
 * This function can be called from interrupt context, but loops internally
 * waking up all threads blocking on the event, so the potential
 * execution cycles cannot be determined in advance.
 *
 * @param[in] event Pointer to event object
 *
 * @retval ATOM_OK Success
 * @retval ATOM_ERR_QUEUE Problem putting a woken thread on the ready queue
 * @retval ATOM_ERR_TIMER Problem cancelling a timeout on a woken thread
 */
uint8_t atomEventDelete (ATOM_EVENT *event)
{
    uint8_t status;
    CRITICAL_STORE;
    ATOM_TCB *tcb_ptr;
    uint8_t woken_threads = FALSE;

    /* Parameter check */
    if (event == NULL)
    {
        /* Bad event pointer */
        status = ATOM_ERR_PARAM;
    }
    else
    {
        /* Default to success status unless errors occur during wakeup */
        status = ATOM_OK;

        /* Wake up all suspended tasks */
        do {
            /* Enter critical region */
            CRITICAL_START ();

            /* Check if any threads are suspended */
            tcb_ptr = event->tcb_ptr;

            /* A thread is suspended on the event */
            if (tcb_ptr)
            {
                /* Return error status to the waiting thread */
                tcb_ptr->suspend_wake_status = ATOM_ERR_DELETED;

                /* Put the thread on the ready queue */
                if (tcbEnqueuePriority (&tcbReadyQ, tcb_ptr) != ATOM_OK)
                {
                    /* Exit critical region */
                    CRITICAL_END ();

                    /* Quit the loop, returning error */
                    status = ATOM_ERR_QUEUE;
                    break;
                }

                /* If there's a timeout on this suspension, cancel it */
                if (tcb_ptr->suspend_timo_cb)
                {
                    /* Cancel the callback */
                    if (atomTimerCancel (tcb_ptr->suspend_timo_cb) != ATOM_OK)
                    {
                        /* Exit critical region */
                        CRITICAL_END ();

                        /* Quit the loop, returning error */
                        status = ATOM_ERR_TIMER;
                        break;
                    }

                    /* Flag as no timeout registered */
                    tcb_ptr->suspend_timo_cb = NULL;
                }

                /* Exit critical region */
                CRITICAL_END ();

                /* Request a reschedule */
                woken_threads = TRUE;
            }

            /* No more suspended threads */
            else
            {
                /* Exit critical region and quit the loop */
                CRITICAL_END ();
                break;
            }
        } while (0);

        /* Call scheduler if any threads were woken up */
        if (woken_threads == TRUE)
        {
            /**
             * Only call the scheduler if we are in thread context, otherwise
             * it will be called on exiting the ISR by atomIntExit().
             */
            if (atomCurrentContext())
                atomSched (FALSE);
        }
    }

    return (status);
}


/**
 * \b atomEventWait
 *
 * Perform a wait operation on an event.
 *
 * This waits for one or more event bits as specified by mask.
 * If no bits are set then the call will block until one is set
 * by another thread, or until the specified \c timeout is reached.
 * Blocking threads will also be woken if the event is
 * deleted by another thread while blocking.
 *
 * Depending on the \c timeout value specified the call will do one of
 * the following if the event flags are zero:
 *
 * \c timeout == 0 : Call will block until the flags are non-zero \n
 * \c timeout > 0 : Call will block until non-zero flags up to the specified timeout \n
 * \c timeout == -1 : Return immediately if the flags are zero \n
 *
 * If the call needs to block and \c timeout is zero, it will block
 * indefinitely until atomEventSet() or atomEventDelete() is called on the
 * event.
 *
 * If the call needs to block and \c timeout is non-zero, the call will only
 * block for the specified number of system ticks after which time, if the
 * thread was not already woken, the call will return with \c ATOM_TIMEOUT.
 *
 * If the call would normally block and \c timeout is -1, the call will
 * return immediately with \c ATOM_WOULDBLOCK.
 *
 * This function can only be called from interrupt context if the \c timeout
 * parameter is -1 (in which case it does not block).
 *
 * @param[in]  event Pointer to event object
 * @param[in]  mask Bitmask of events to wait on
 * @param[out] value Bitmask of event(s) which satisfied the wait
 * @param[in]  timeout Max system ticks to block (0 = forever)
 *
 * @retval ATOM_OK Success
 * @retval ATOM_TIMEOUT event timed out before being woken
 * @retval ATOM_WOULDBLOCK Called with timeout == -1 but count is zero
 * @retval ATOM_ERR_DELETED event was deleted while suspended
 * @retval ATOM_ERR_CONTEXT Not called in thread context and attempted to block
 * @retval ATOM_ERR_PARAM Bad parameter
 * @retval ATOM_ERR_QUEUE Problem putting the thread on the suspend queue
 * @retval ATOM_ERR_TIMER Problem registering the timeout
 */
uint8_t atomEventWait (ATOM_EVENT *event, uint32_t mask, uint32_t *value, int32_t timeout)
{
    CRITICAL_STORE;
    uint8_t status;
    ATOM_TIMER timer_cb;
    ATOM_TCB *curr_tcb_ptr;

    /* Check parameters */
    if (event == NULL)
    {
        /* Bad event pointer */
        status = ATOM_ERR_PARAM;
    }
    else
    {
        /* Protect access to the event object and OS queues */
        CRITICAL_START ();

        if (value != NULL)
            *value = 0;

        /* If flags are zero, block the calling thread */
        if ((event->flags & mask) == 0)
        {
            /* If called with timeout >= 0, we should block */
            if (timeout >= 0)
            {
                /* flags are zero, block the calling thread */

                /* Get the current TCB */
                curr_tcb_ptr = atomCurrentContext();

                /* Check we are actually in thread context */
                if (curr_tcb_ptr)
                {
                    /* Add current thread to the suspend list on this event */
                    if (event->tcb_ptr != NULL)
                    {
                        /* Exit critical region */
                        CRITICAL_END ();

                        /* There was an error putting this thread on the suspend list */
                        status = ATOM_ERR_QUEUE;
                    }
                    else
                    {
                        /* Save event data for atomEventSet() */
                        event->tcb_ptr = curr_tcb_ptr;
                        event->mask = mask;

                        /* Set suspended status for the current thread */
                        curr_tcb_ptr->suspended = TRUE;

                        /* Track errors */
                        status = ATOM_OK;

                        /* Register a timer callback if requested */
                        if (timeout)
                        {
                            /* Fill out the timer callback request structure */
                            timer_cb.cb_func = atomEventTimerCallback;
                            timer_cb.cb_data = (POINTER)event;
                            timer_cb.cb_ticks = timeout;

                            /**
                             * Store the timer details in the TCB so that we can
                             * cancel the timer callback if the event is set
                             * before the timeout occurs.
                             */
                            curr_tcb_ptr->suspend_timo_cb = &timer_cb;

                            /* Register a callback on timeout */
                            if (atomTimerRegister (&timer_cb) != ATOM_OK)
                            {
                                /* Timer registration failed */
                                status = ATOM_ERR_TIMER;

                                /* Clean up and return to the caller */
                                event->tcb_ptr = NULL;
                                event->mask = 0;
                                curr_tcb_ptr->suspended = FALSE;
                                curr_tcb_ptr->suspend_timo_cb = NULL;
                            }
                        }

                        /* Set no timeout requested */
                        else
                        {
                            /* No need to cancel timeouts on this one */
                            curr_tcb_ptr->suspend_timo_cb = NULL;
                        }

                        /* Check no errors have occurred */
                        if (status == ATOM_OK)
                        {
                            /**
                             * Current thread now blocking, schedule in a new
                             * one. We already know we are in thread context
                             * so can call the scheduler from here.
                             */
                            atomSched (FALSE);

                            /**
                             * Normal atomEventSet() wakeups will set ATOM_OK status,
                             * while timeouts will set ATOM_TIMEOUT and event
                             * deletions will set ATOM_ERR_DELETED.
                             */
                            status = curr_tcb_ptr->suspend_wake_status;

                            /**
                             * If we have been woken up with ATOM_OK then
                             * another thread set a flag in the event and
                             * handed control to this thread. Return any
                             * set flags that were in the wait mask.
                             */
                            if (status == ATOM_OK && value != NULL)
                            {
                                *value = event->flags & event->mask;
                            }

                            /* Clean up event data */
                            event->tcb_ptr = NULL;
                            event->mask = 0;
                        }

                        /* Exit critical region */
                        CRITICAL_END ();
                    }
                }
                else
                {
                    /* Exit critical region */
                    CRITICAL_END ();

                    /* Not currently in thread context, can't suspend */
                    status = ATOM_ERR_CONTEXT;
                }
            }
            else
            {
                /* timeout == -1, requested not to block and event flags are zero */
                CRITICAL_END();
                status = ATOM_WOULDBLOCK;
            }
        }
        else
        {
            /* At least one event flag is set, just return to calling thread */
            if (value != NULL) {
                *value = event->flags & mask;
            }

            /* Exit critical region */
            CRITICAL_END ();

            /* Successful */
            status = ATOM_OK;
        }
    }

    return (status);
}


/**
 * \b atomEventSet
 *
 * Perform a set operation on event flags.
 *
 * This sets the specified event flags and returns.
 *
 * If the flags were previously zero and there is a thread blocking on the
 * event, the call will wake up the suspended thread.
 *
 * This function can be called from interrupt context.
 *
 * @param[in] event Pointer to event object
 * @param[in] mask  Event flags to set
 *
 * @retval ATOM_OK Success
 * @retval ATOM_ERR_PARAM Bad parameter
 * @retval ATOM_ERR_QUEUE Problem putting a woken thread on the ready queue
 * @retval ATOM_ERR_TIMER Problem cancelling a timeout for a woken thread
 */
uint8_t atomEventSet (ATOM_EVENT *event, uint32_t mask)
{
    uint8_t status;
    CRITICAL_STORE;
    ATOM_TCB *tcb_ptr;

    /* Check parameters */
    if (event == NULL || mask == 0)
    {
        /* Bad event pointer or no flags to set */
        status = ATOM_ERR_PARAM;
    }
    else
    {
        /* Protect access to the event object and OS queues */
        CRITICAL_START ();

        /* Set the event flags regardless of wait status */
        event->flags |= mask;

        /* If a thread is blocking on the event just set, wake it up */
        if (event->tcb_ptr != NULL && (event->flags & event->mask) != 0)
        {
            tcb_ptr = event->tcb_ptr;
            if (tcbEnqueuePriority (&tcbReadyQ, tcb_ptr) != ATOM_OK)
            {
                /* Exit critical region */
                CRITICAL_END ();

                /* There was a problem putting the thread on the ready queue */
                status = ATOM_ERR_QUEUE;
            }
            else
            {
                /* Set OK status to be returned to the waiting thread */
                tcb_ptr->suspend_wake_status = ATOM_OK;

                /* If there's a timeout on this suspension, cancel it */
                if ((tcb_ptr->suspend_timo_cb != NULL)
                    && (atomTimerCancel (tcb_ptr->suspend_timo_cb) != ATOM_OK))
                {
                    /* There was a problem cancelling a timeout on this event */
                    status = ATOM_ERR_TIMER;
                }
                else
                {
                    /* Flag as no timeout registered */
                    tcb_ptr->suspend_timo_cb = NULL;

                    /* Successful */
                    status = ATOM_OK;
                }

                /* Exit critical region */
                CRITICAL_END ();

                /**
                 * The scheduler may now make a policy decision to thread
                 * switch if we are currently in thread context. If we are
                 * in interrupt context it will be handled by atomIntExit().
                 */
                if (atomCurrentContext())
                    atomSched (FALSE);
            }
        }

        /* If no threads waiting, just return */
        else
        {
            status = ATOM_OK;

            /* Exit critical region */
            CRITICAL_END ();
        }
    }

    return (status);
}


/**
 * \b atomEventClear
 *
 * Perform a clear operation on event flags.
 *
 * Care must be taken when using this function, as there may be a thread
 * suspended on the event. In general it should only be used by a thread
 * that was waiting on the event after being woken up.
 *
 * This function can be called from interrupt context.
 *
 * @param[in] event Pointer to event object
 * @param[in] mask  Event flags to clear
 *
 * @retval ATOM_OK Success
 * @retval ATOM_ERR_PARAM Bad parameter
 */
uint8_t atomEventClear (ATOM_EVENT *event, uint32_t mask)
{
    uint8_t status;
    CRITICAL_STORE;

    /* Parameter check */
    if (event == NULL)
    {
        /* Bad event pointer */
        status = ATOM_ERR_PARAM;
    }
    else
    {
        /* Protect access to the event object */
        CRITICAL_START ();

        /* Clear the event flags */
        event->flags &= ~mask;

        /* Successful */
        status = ATOM_OK;

        /* Exit critical region */
        CRITICAL_END ();
    }

    return (status);
}


/**
 * \b atomEventTimerCallback
 *
 * This is an internal function not for use by application code.
 *
 * Timeouts on suspended threads are notified by the timer system through
 * this generic callback. The timer system calls us back with a pointer to
 * the relevant \c ATOM_EVENT object.
 *
 * @param[in] cb_data Pointer to an ATOM_EVENT object
 */
static void atomEventTimerCallback (POINTER cb_data)
{
    ATOM_EVENT *timer_data_ptr;
    CRITICAL_STORE;

    /* Get the ATOM_EVENT structure pointer */
    timer_data_ptr = (ATOM_EVENT *)cb_data;

    /* Check parameter is valid */
    if (timer_data_ptr)
    {
        /* Enter critical region */
        CRITICAL_START ();

        /* Set status to indicate to the waiting thread that it timed out */
        timer_data_ptr->tcb_ptr->suspend_wake_status = ATOM_TIMEOUT;

        /* Flag as no timeout registered */
        timer_data_ptr->tcb_ptr->suspend_timo_cb = NULL;

        /* Put the thread on the ready queue */
        (void)tcbEnqueuePriority (&tcbReadyQ, timer_data_ptr->tcb_ptr);

        /* Exit critical region */
        CRITICAL_END ();

        /**
         * Note that we don't call the scheduler now as it will be called
         * when we exit the ISR by atomIntExit().
         */
    }
}
