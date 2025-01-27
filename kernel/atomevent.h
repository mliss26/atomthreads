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

#ifndef __ATOM_EVENT_H
#define __ATOM_EVENT_H

#ifdef __cplusplus
extern "C" {
#endif

typedef struct atom_event
{
    ATOM_TCB *tcb_ptr;  /* Thread suspended on this event */
    uint32_t flags;     /* Event flags */
    uint32_t mask;      /* Event wait mask */
} ATOM_EVENT;

extern uint8_t atomEventCreate (ATOM_EVENT *event);
extern uint8_t atomEventDelete (ATOM_EVENT *event);
extern uint8_t atomEventWait (ATOM_EVENT *event, uint32_t mask, uint32_t *value, int32_t timeout);
extern uint8_t atomEventSet (ATOM_EVENT *event, uint32_t mask);
extern uint8_t atomEventClear (ATOM_EVENT *event, uint32_t mask);

#ifdef __cplusplus
}
#endif

#endif /* __ATOM_EVENT_H */
