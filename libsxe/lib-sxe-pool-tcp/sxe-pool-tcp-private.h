/* Copyright (c) 2010 Sophos Group.
 * 
 * Permission is hereby granted, free of charge, to any person obtaining a copy
 * of this software and associated documentation files (the "Software"), to deal
 * in the Software without restriction, including without limitation the rights
 * to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
 * copies of the Software, and to permit persons to whom the Software is
 * furnished to do so, subject to the following conditions:
 * 
 * The above copyright notice and this permission notice shall be included in
 * all copies or substantial portions of the Software.
 * 
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
 * IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
 * FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
 * AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
 * LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
 * OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN
 * THE SOFTWARE.
 */

#ifndef __SXE_POOL_TCP_PRIVATE_H__
#define __SXE_POOL_TCP_PRIVATE_H__

/* Included for testing
 */
typedef enum SXE_POOL_TCP_STATE {
    SXE_POOL_TCP_STATE_UNCONNECTED = 0,
    SXE_POOL_TCP_STATE_CONNECTING,
    SXE_POOL_TCP_STATE_INITIALIZING,
    SXE_POOL_TCP_STATE_READY_TO_SEND,
    SXE_POOL_TCP_STATE_IN_USE,
    SXE_POOL_TCP_STATE_NUMBER_OF_STATES   /* This is not a state; must come last */
} SXE_POOL_TCP_STATE;

#endif
