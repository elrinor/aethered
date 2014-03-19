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
#ifndef __AEM_H__
#define __AEM_H__

#include <wdm.h>
#include <hidport.h>
#include "common.h"   

/** Compile in relative motion mode? */
#define AEM_RELATIVE_MOTION

/** Message check interval, in 1/1000000 sec. */
#define AEM_DEFAULT_MESSAGE_CHECK_INTERVAL 8000
#define AEM_MINIMAL_MESSAGE_CHECK_INTERVAL 5000

/** Size of move report queue. */
#define AEM_MESSAGE_QUEUE_SIZE 1024

#if DBG
#  define DebugPrint(ARGS) { DbgPrint("ETHER: "); DbgPrint ARGS; }
#else 
#  define DebugPrint(ARGS)
#endif

#define AEM_POOL_TAG            ((ULONG) 'diHV')

/** AEM_HARDWARE_IDS can be changed directly in the binary, without the need to recompile. */
#define AEM_HARDWARE_IDS        L"HID\\Vid_037e&Pid_00a7\0\0PADDINGPADDINGPADDINGPADDINGPADDINGPADDINGPADDINGPADDINGPADDINGPADDINGPADDINGPADDINGPADDINGPADDINGPADDINGPADDINGPADDING"
#define AEM_HARDWARE_IDS_LENGTH sizeof(AEM_HARDWARE_IDS)

#ifdef AEM_RELATIVE_MOTION
#  define AEM_INPUT_REPORT_SIZE 0x3 
#else
#  define AEM_INPUT_REPORT_SIZE 0x5 
#endif

typedef UCHAR HID_REPORT_DESCRIPTOR, *PHID_REPORT_DESCRIPTOR;

/** This is the report descriptor for the Arx Ethereal Mouse device returned
 * by the minidriver in response to IOCTL_HID_GET_REPORT_DESCRIPTOR. */ 
HID_REPORT_DESCRIPTOR AemReportDescriptor[] = {
  0x05, 0x01,                      // USAGE_PAGE (Generic Desktop)
  0x09, 0x02,                      // USAGE (Mouse) 
  0xa1, 0x01,                      // COLLECTION (Application)
  0x85, AEM_POINTER_REPORT_ID,     //   REPORT_ID (AEM_POINTER_REPORT_ID)
  0x09, 0x01,                      //   USAGE (Pointer),
  0xA1, 0x00,                      //   COLLECTION (Physical),
  0x05, 0x09,                      //     USAGE_PAGE (Button)
  0x19, 0x01,                      //     USAGE_MINIMUM (Button 1)
  0x29, 0x03,                      //     USAGE_MAXIMUM (Button 3)
  0x15, 0x00,                      //     LOGICAL_MINIMUM (0)
  0x25, 0x01,                      //     LOGICAL_MAXIMUM (1)
  0x95, 0x03,                      //     REPORT_COUNT (3)
  0x75, 0x01,                      //     REPORT_SIZE (1)
  0x81, 0x02,                      //     INPUT (Data,Var,Abs)
  0x95, 0x01,                      //     REPORT_COUNT (1)
  0x75, 0x05,                      //     REPORT_SIZE (5)
  0x81, 0x03,                      //     INPUT (Cnst,Var,Abs)
  0x05, 0x01,                      //     USAGE_PAGE (Generic Desktop)
  0x09, 0x30,                      //     USAGE (X)
  0x09, 0x31,                      //     USAGE (Y)
#ifdef AEM_RELATIVE_MOTION
  0x15, 0x81,                      //     LOGICAL_MINIMUM (-127),
  0x25, 0x7F,                      //     LOGICAL_MAXIMUM (127),
  0x75, 0x08,                      //     REPORT_SIZE (8),
  0x95, 0x02,                      //     REPORT_COUNT (2),
  0x81, 0x06,                      //     INPUT (Data,Var,Rel)
#else
  0x16, 0x00, 0x80,                //     LOGICAL_MINIMUM (-32768)
  0x26, 0xff, 0x7f,                //     LOGICAL_MAXIMUM (32767)
  0x75, 0x10,                      //     REPORT_SIZE (16)
  0x95, 0x02,                      //     REPORT_COUNT (2)
  0x81, 0x02,                      //     INPUT (Data,Var,Abs)
#endif
  0xc0,                            //   END_COLLECTION
  0xc0,                            // END_COLLECTION

  0x06, AEM_USAGE_PAGE_BYTES,      // USAGE_PAGE (Vendor Defined Usage Page)
  0x09, AEM_CONTROL_USAGE,         // USAGE (Vendor Usage AEM_CONTROL_USAGE)
  0xA1, 0x01,                      // COLLECTION (Application)
  0x85, AEM_CONTROL_REPORT_ID,     //   REPORT_ID (AEM_CONTROL_REPORT_ID)
  0x09, AEM_CONTROL_USAGE,         //   USAGE (Vendor Usage AEM_CONTROL_USAGE)
  0x15, 0x00,                      //   LOGICAL_MINIMUM(0)
  0x26, 0xff, 0x00,                //   LOGICAL_MAXIMUM(255)
  0x75, 0x08,                      //   REPORT_SIZE (0x08)
  0x95, 0x01,                      //   REPORT_COUNT (0x01)
  0xB1, 0x00,                      //   FEATURE (Data,Ary,Abs)
                                   // DUMMY INPUT
  0x09, AEM_CONTROL_USAGE,         //   USAGE (Vendor Usage AEM_CONTROL_USAGE)
  0x75, 0x08,                      //   REPORT_SIZE (0x08)
  0x95, AEM_INPUT_REPORT_SIZE,     //   REPORT_COUNT (AEM_INPUT_REPORT_SIZE)
  0x81, 0x02,                      //   INPUT (Data,Var,Abs)
  0xC0                             // END_COLLECTION
};

/** This is the default HID descriptor returned by the minidriver
 * in response to IOCTL_HID_GET_DEVICE_DESCRIPTOR. */
HID_DESCRIPTOR  AemHidDescriptor = {
  sizeof(HID_DESCRIPTOR),
  HID_HID_DESCRIPTOR_TYPE,
  0x0100 , /**< Hid spec release. */
  0x00,    /**< Country code (Not Specified) */
  0x01,    /**< Number of HID class descriptors */
  { HID_REPORT_DESCRIPTOR_TYPE, sizeof(AemReportDescriptor) }
};

/* These are the device attributes returned by the minidriver in response 
 * to IOCTL_HID_GET_DEVICE_ATTRIBUTES. */
#define AEM_PRODUCT_ID 0xDEAD
#define AEM_VENDOR_ID  0xF00D
#define AEM_VERSION    0x0102

/** These are the states FDO transition to upon receiving a specific PnP Irp. 
 * Refer to the PnP Device States diagram in DDK documentation for better understanding. */
typedef enum _DEVICE_PNP_STATE {
    NotStarted = 0,         /**< Not started yet. */
    Started,                /**< Device has received the START_DEVICE IRP. */
    StopPending,            /**< Device has received the QUERY_STOP IRP. */
    Stopped,                /**< Device has received the STOP_DEVICE IRP. */
    RemovePending,          /**< Device has received the QUERY_REMOVE IRP. */
    SurpriseRemovePending,  /**< Device has received the SURPRISE_REMOVE IRP. */
    Deleted                 /**< Device has received the REMOVE_DEVICE IRP. */
} DEVICE_PNP_STATE;

#define SET_NEW_PNP_STATE(DEVICE_INFO, NEW_STATE)                               \
  (DEVICE_INFO)->PreviousPnPState =  (DEVICE_INFO)->DevicePnPState;             \
  (DEVICE_INFO)->DevicePnPState = (NEW_STATE);

#define RESTORE_PREVIOUS_PNP_STATE(DEVICE_INFO)                                 \
  (DEVICE_INFO)->DevicePnPState = (DEVICE_INFO)->PreviousPnPState;

/** Device extension structure for Arx Ethereal Mouse device. */
typedef struct _AEM_DEVICE_EXTENSION {
  HID_DESCRIPTOR           HidDescriptor;
  PHID_REPORT_DESCRIPTOR   ReportDescriptor;
  BOOLEAN                  ReadReportDescFromRegistry;
  DEVICE_PNP_STATE         DevicePnPState;   /**< Tracks the state of the device. */
  DEVICE_PNP_STATE         PreviousPnPState; /**< Remembers the previous pnp state. */

  AEM_INFO_FEATURE_REPORT  InfoReport;
  AEM_MOVE_FEATURE_REPORT  MessageQueue[AEM_MESSAGE_QUEUE_SIZE];
  DWORD32                  MessageQueueStart;
  DWORD32                  MessageQueueEnd;
  KSPIN_LOCK               MessageQueueLock;
  DWORD32                  MessageCheckInterval;
} AEM_DEVICE_EXTENSION, *PAEM_DEVICE_EXTENSION;

typedef struct _READ_TIMER {
  KDPC           ReadTimerDpc;
  KTIMER         ReadTimer;
  PIRP           Irp;
  PDEVICE_OBJECT DeviceObject;
} READ_TIMER, *PREAD_TIMER;


/* Some accessors for device object, to avoid crazy pointer dance. */

#define GET_MINIDRIVER_DEVICE_EXTENSION(DO) \
    ((PAEM_DEVICE_EXTENSION) \
    (((PHID_DEVICE_EXTENSION)(DO)->DeviceExtension)->MiniDeviceExtension))

#define GET_NEXT_DEVICE_OBJECT(DO) \
    (((PHID_DEVICE_EXTENSION)(DO)->DeviceExtension)->NextDeviceObject)

#define GET_PHYSICAL_DEVICE_OBJECT(DO) \
    (((PHID_DEVICE_EXTENSION)(DO)->DeviceExtension)->PhysicalDeviceObject)

/* Declaration of all functions follows. */

NTSTATUS DriverEntry(PDRIVER_OBJECT DriverObject, PUNICODE_STRING registryPath);
NTSTATUS PnP(PDEVICE_OBJECT DeviceObject, PIRP Irp);
NTSTATUS PnPComplete(PDEVICE_OBJECT DeviceObject, PIRP Irp, PVOID Context);
NTSTATUS Power(PDEVICE_OBJECT DeviceObject, PIRP Irp);
NTSTATUS SystemControl(PDEVICE_OBJECT DeviceObject, PIRP Irp);
NTSTATUS AddDevice(PDRIVER_OBJECT DriverObject, PDEVICE_OBJECT FunctionalDeviceObject);
VOID Unload(PDRIVER_OBJECT DriverObject);
NTSTATUS InternalIoctl(PDEVICE_OBJECT DeviceObject, PIRP Irp);
NTSTATUS GetHidDescriptor(PDEVICE_OBJECT DeviceObject, PIRP Irp);
NTSTATUS GetReportDescriptor(PDEVICE_OBJECT DeviceObject, PIRP Irp);
NTSTATUS GetAttributes(PDEVICE_OBJECT DeviceObject, PIRP Irp);
NTSTATUS GetDeviceAttributes(PDEVICE_OBJECT DeviceObject, PIRP Irp);
NTSTATUS GetFeature(PDEVICE_OBJECT DeviceObject, PIRP Irp);
PCHAR PnPMinorFunctionString(UCHAR MinorFunction);
NTSTATUS ReadReport(PDEVICE_OBJECT DeviceObject, PIRP Irp);
VOID ReadTimerDpcRoutine(PKDPC Dpc, PVOID DeferredContext, PVOID SystemArgument1, PVOID SystemArgument2);

#endif // __AEM_H__
