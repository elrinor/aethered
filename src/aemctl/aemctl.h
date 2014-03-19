/* This file is part of Aethered, a collection of virtual device drivers for
 * Windows.
 *
 * Copyright (C) 2010-2011 Alexander Fokin <apfokin@gmail.com>
 *
 * Aethered is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License 
 * as published by the Free Software Foundation; either
 * version 3 of the License, or (at your option) any later version.
 *
 * Aethered is distributed in the hope that it will be useful, but WITHOUT ANY
 * WARRANTY; without even the implied warranty of MERCHANTABILITY or FITNESS
 * FOR A PARTICULAR PURPOSE. See the GNU General Public License 
 * for more details.
 *
 * You should have received a copy of the GNU General Public License 
 * along with Aethered. If not, see <http://www.gnu.org/licenses/>. */
#ifndef __AEMCTL_H__
#define __AEMCTL_H__

#ifdef AEMCTLDLL
#  define AEMCTLAPI __declspec(dllexport)
#  define AEMCTLAPIENTRY  __cdecl
#else
#  define AEMCTLAPI __declspec(dllimport)
#  define AEMCTLAPIENTRY  __cdecl
#endif

#ifdef __cplusplus
extern "C" {
#endif

typedef enum AEMCTLRESULT_ {
  AEMCTL_OK = 0,
  AEMCTL_INIT_FAILED = 1,
  AEMCTL_INVALID_PARAMETER = 2,
  AEMCTL_QUEUE_FULL = 3,
  AEMCTL_COMMUNICATION_FAILED = 4
} AEMCTLRESULT;

/** This function sends a move mouse message to the arx ethereal mouse device.
 * Note that arx ethereal mouse device maintains a queue of incoming messages 
 * and processes only one message per tick. 
 * 
 * X and y parameters do not correspond to the actual screen size. 
 *
 * If arx ethereal mouse device driver was compiled in relative motion mode,
 * then x and y must be in range [-127, 127].
 *
 * If it was compiled in an absolute motion mode, then a coordinate system with 
 * (1, 1) and (32767, 32767) being the coordinates of upper-left and lower-right 
 * corners of the screen is used instead.
 *
 * @param x                            x coordinate.
 * @param y                            y coordinate.
 * @param buttons                      button flags.
 * @returns                            AEMCTL_OK if everything went fine, non-zero error code otherwise. */
AEMCTLAPI AEMCTLRESULT AEMCTLAPIENTRY AemSendMessage(int x, int y, char buttons);

/** Clears the message queue of arx ethereal mouse device.
 *
 * @returns                            AEMCTL_OK if everything went fine, non-zero error code otherwise. */
AEMCTLAPI AEMCTLRESULT AEMCTLAPIENTRY AemClearMessageQueue();

/** Gets current size of the message queue of arx ethereal mouse device.
 * 
 * @param size                         (out) size of the message queue.
 * @returns                            AEMCTL_OK if everything went fine, non-zero error code otherwise. */
AEMCTLAPI AEMCTLRESULT AEMCTLAPIENTRY AemGetMessageQueueSize(int* size);

/** Gets current message check interval of arx ethereal mouse device.
 *
 * @param interval                     (out) current message check interval of arx ethereal mouse device, int 1/1000000th of a second.
 * @returns                            AEMCTL_OK if everything went fine, non-zero error code otherwise. */
AEMCTLAPI AEMCTLRESULT AEMCTLAPIENTRY AemGetMessageCheckInterval(int* interval);

/** Sets message check interval of arx ethereal mouse device.
 *
 * @param interval                     new message check interval for arx ethereal mouse device, int 1/1000000th of a second.
 * @returns                            AEMCTL_OK if everything went fine, non-zero error code otherwise. */
AEMCTLAPI AEMCTLRESULT AEMCTLAPIENTRY AemSetMessageCheckInterval(int interval);

/** This function can be used to obtain information on arx ethereal mouse device.
 * 
 * @param isRelative                   (out) non-zero if arx ethereal mouse was compiled in relative motion mode, zero otherwise.
 * @param maxQueueSize                 (out) message queue capacity.
 * @returns                            AEMCTL_OK if everything went fine, non-zero error code otherwise. */
AEMCTLAPI AEMCTLRESULT AEMCTLAPIENTRY AemGetDeviceInfo(int* isRelative, int* queueCapacity);

/** @returns                           textual representation of the last error occurred. */
AEMCTLAPI const char* AEMCTLAPIENTRY AemGetLastErrorString(void);


#ifdef __cplusplus
}; // extern "C"
#endif

#endif // __AEMCTL_H__
