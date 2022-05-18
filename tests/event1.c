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


/**
 * \b test_start
 *
 * Start event test.
 *
 * This tests the bad parameter handling of all event APIs and
 * event waits with timeout and the event already set.
 *
 * @retval Number of failures
 */
uint32_t test_start (void)
{
    int failures;
    uint8_t status;
    ATOM_EVENT event;
    uint32_t i, j, set_mask = 0, mask, value;

    /* Default to zero failures */
    failures = 0;

    /* atomEventCreate bad param */
    if (atomEventCreate(NULL) != ATOM_ERR_PARAM)
    {
        ATOMLOG(_STR("Create bad event check\n"));
        failures++;
    }

    /* atomEventDelete bad param */
    if (atomEventDelete(NULL) != ATOM_ERR_PARAM)
    {
        ATOMLOG(_STR("Delete bad event check\n"));
        failures++;
    }

    /* atomEventWait bad param */
    if (atomEventWait(NULL, 0, NULL, 0) != ATOM_ERR_PARAM)
    {
        ATOMLOG(_STR("Wait bad event check\n"));
        failures++;
    }

    /* atomEventSet bad param */
    if (atomEventSet(NULL, 0) != ATOM_ERR_PARAM)
    {
        ATOMLOG(_STR("Set bad event check\n"));
        failures++;
    }

    /* atomEventClear bad param */
    if (atomEventClear(NULL, 0) != ATOM_ERR_PARAM)
    {
        ATOMLOG(_STR("Clear bad event check\n"));
        failures++;
    }

    /* Create an event for wait testing */
    status = atomEventCreate(&event);
    if (status != ATOM_OK)
    {
        ATOMLOG(_STR("Create event failed (%d)\n"), status);
        failures++;
    }
    else
    {
        /* test waiting with a timeout expected */
        for (j = 0; j < 32; j++)
        {
            set_mask = (set_mask << 1) | 1;

            for (i = 0; i < 32-j; i++)
            {
                mask = set_mask << i;

                status = atomEventWait(&event, mask, &value, 2);
                if (status != ATOM_TIMEOUT)
                {
                    ATOMLOG(_STR("Wait ended without timeout (%d)\n"), status);
                    failures++;
                }
            }
        }

        /* test waiting on an event that has already been set */
        set_mask = 0;
        for (j = 0; j < 32; j++)
        {
            set_mask = (set_mask << 1) | 1;

            for (i = 0; i < 32; i++)
            {
                mask = set_mask << i;

                /* Set the event(s) */
                status = atomEventSet(&event, mask);
                if (status != ATOM_OK)
                {
                    ATOMLOG(_STR("Set failed (%d)\n"), status);
                    failures++;
                }
                else
                {
                    /* Wait on just the first event in set mask */
                    status = atomEventWait(&event, (1UL << i), &value, SYSTEM_TICKS_PER_SEC);
                    if (status != ATOM_OK)
                    {
                        ATOMLOG(_STR("Wait ended unsuccessfully (%d)\n"), status);
                        failures++;
                    }
                    else
                    {
                        /* Ensure the returned mask was expected */
                        if (value != (1UL << i))
                        {
                            ATOMLOG(_STR("Wait returned unexpected mask 0x%lx\n"), value);
                            failures++;
                        }
                    }

                    status = atomEventClear(&event, 0xffffffff);
                    if (status != ATOM_OK)
                    {
                        ATOMLOG(_STR("Clear failed (%d)\n"), status);
                        failures++;
                    }
                }
            }
        }
    }

    /* Quit */
    return failures;

}
