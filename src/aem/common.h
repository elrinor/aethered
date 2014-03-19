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
#ifndef __AEM_COMMON_H__
#define __AEM_COMMON_H__

#define AEM_POINTER_REPORT_ID 0x01
#define AEM_CONTROL_REPORT_ID 0x02

#define AEM_CONTROL_USAGE 0x07

#define AEM_USAGE_PAGE_SHORT 0xFF00
#define AEM_USAGE_PAGE_BYTES 0x00, 0xFF

#define AEM_CONTROL_CODE_MOVE        0x00
#define AEM_CONTROL_CODE_INFO        0x01
#define AEM_CONTROL_CODE_CLEAR_QUEUE 0x02
#define AEM_CONTROL_CODE_INTERVAL    0x03
#define AEM_CONTROL_CODE_QUEUE_SIZE  0x04
#define AEM_CONTROL_CODE_ERROR       0xFF

#define AEM_FLAG_RELATIVE 0x01

#include <pshpack1.h>

typedef struct _SHORT_POINT {
  SHORT X;
  SHORT Y;
} SHORT_POINT, *PSHORT_POINT;

typedef struct _AEM_FEATURE_REPORT {
  UCHAR ReportId; /**< Report ID of the collection to which the control request is sent. */
  UCHAR ControlCode; /**< Control Code / Success Flag. */
} AEM_FEATURE_REPORT, *PAEM_FEATURE_REPORT;

typedef struct _AEM_MOVE_FEATURE_REPORT {
  AEM_FEATURE_REPORT Report; /**< Base report. */
  UCHAR Buttons; /**< Button flags. */
  SHORT_POINT Point; /**< New coord. */
} AEM_MOVE_FEATURE_REPORT, *PAEM_MOVE_FEATURE_REPORT;

typedef struct _AEM_INFO_FEATURE_REPORT {
  AEM_FEATURE_REPORT Report; /**< Base report. */
  UCHAR Flags; /**< Flags. */
  DWORD32 MessageQueueCapacity; /**< Size of message queue. */
} AEM_INFO_FEATURE_REPORT, *PAEM_INFO_FEATURE_REPORT;

typedef struct _AEM_DWORD_FEATURE_REPORT {
  AEM_FEATURE_REPORT Report; /**< Base report. */
  DWORD32 Value;
} AEM_DWORD_FEATURE_REPORT, *PAEM_DWORD_FEATURE_REPORT;

#include <poppack.h>

#endif // __AEM_COMMON_H__
