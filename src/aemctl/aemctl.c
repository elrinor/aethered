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
#define AEMCTLDLL
#include "aemctl.h"
#include <Windows.h>
#include <hidsdi.h>
#include <setupapi.h>
#include "common.h"

#pragma comment(lib, "setupapi.lib")
#pragma comment(lib, "hid.lib")

CHAR LastErrorMessageBuffer[4096];
CHAR DeviceNotFound[] = "Arx Ethereal Mouse Device was not found or could not be opened.";
CHAR OutOfBoundsAbsolute[] = "Coordinates do not lie in [1, 32767] segment.";
CHAR OutOfBoundsRelative[] = "Coordinates do not lie in [-127, 127] segment.";
CHAR NullPassed[] = "NULL value passed where non-NULL value was expected.";
CHAR MessageCheckIntervalTooSmall[] = "Given message check interval is too small.";
CHAR QueueFull[] = "Message queue is full.";
LPCSTR LastErrorMessage;
HANDLE Heap;
HANDLE ArxEtherealMouse;
CHAR Flags;
DWORD QueueCapacity;

VOID WinApiCallFailed(LPCSTR functionName) {
  wsprintf(LastErrorMessageBuffer, "%s failed with error code 0x%x", functionName, GetLastError());
}


BOOL IsArxEtherealMouse(HANDLE file) {
  PHIDP_PREPARSED_DATA Ppd; /**< The opaque parser info describing this device */
  HIDP_CAPS            Caps; /**< The Capabilities of this hid device. */

  if(!HidD_GetPreparsedData(file, &Ppd))
    return FALSE;

  if(!HidP_GetCaps(Ppd, &Caps)) {
    HidD_FreePreparsedData (Ppd);
    return FALSE;
  }

  if((Caps.UsagePage == AEM_USAGE_PAGE_SHORT) && (Caps.Usage == AEM_CONTROL_USAGE))
    return TRUE;

  return FALSE;
}


VOID StartDll(void) {
  GUID                     hidGuid;
  HDEVINFO                 deviceInfoSet;
  SP_DEVICE_INTERFACE_DATA deviceInterfaceData;
  SP_DEVINFO_DATA          devInfoData;
  int                      i;

  /* Init error reporting. */
  LastErrorMessageBuffer[0] = '\0';
  LastErrorMessage = LastErrorMessageBuffer;

  /* Device handle is uninitialized. */
  ArxEtherealMouse = INVALID_HANDLE_VALUE;

  /* Create standard heap that has no growth limit. */
  Heap = HeapCreate(0, 0, 0);
  if(Heap == NULL) {
    WinApiCallFailed("HeapCreate");
    return;
  }

  /* Get device info set for HID devices. */
  HidD_GetHidGuid(&hidGuid);
  deviceInfoSet = SetupDiGetClassDevs(&hidGuid, NULL, NULL, (DIGCF_PRESENT | DIGCF_INTERFACEDEVICE)); 
  if(deviceInfoSet == INVALID_HANDLE_VALUE) {
    WinApiCallFailed("SetupDiGetClassDevs");
    return;
  }

  /* Enumerate devices of this interface class. */
  deviceInterfaceData.cbSize = sizeof(SP_DEVICE_INTERFACE_DATA);
  devInfoData.cbSize = sizeof(SP_DEVINFO_DATA);
  for(i = 0; SetupDiEnumDeviceInterfaces(deviceInfoSet, 0, &hidGuid, i, &deviceInterfaceData); i++) {
    DWORD                            requiredSize = 0;
    DWORD                            dummy;
    PSP_DEVICE_INTERFACE_DETAIL_DATA deviceInterfaceDetailData;
    HANDLE                           file;
    BOOL                             deviceFound = FALSE;
    AEM_INFO_FEATURE_REPORT         report;

    /* Probing so no output buffer yet. */
    SetupDiGetDeviceInterfaceDetail(deviceInfoSet, &deviceInterfaceData, NULL, 0, &requiredSize, NULL);

    /* Allocate buffer. */
    deviceInterfaceDetailData = (PSP_DEVICE_INTERFACE_DETAIL_DATA) HeapAlloc(Heap, 0, requiredSize);
    if(deviceInterfaceDetailData == NULL)
      continue;

    /* Get device interface data. */
    deviceInterfaceDetailData->cbSize = sizeof(SP_DEVICE_INTERFACE_DETAIL_DATA);
    if(!SetupDiGetDeviceInterfaceDetail(deviceInfoSet, &deviceInterfaceData, deviceInterfaceDetailData, requiredSize, &dummy, NULL)) {
      HeapFree(Heap, 0, deviceInterfaceDetailData);
      continue;
    }

    /* Open the device. */
    file = CreateFile(deviceInterfaceDetailData->DevicePath, FILE_SHARE_READ | FILE_SHARE_WRITE, 0, NULL, OPEN_EXISTING, 0, NULL);
    if(file == INVALID_HANDLE_VALUE) {
      HeapFree(Heap, 0, deviceInterfaceDetailData);
      continue;
    }

    /* Check if its our device. */
    if(!IsArxEtherealMouse(file)) {
      HeapFree(Heap, 0, deviceInterfaceDetailData);
      continue;
    }

    /* Get flags. */
    report.Report.ReportId = AEM_CONTROL_REPORT_ID;
    report.Report.ControlCode = AEM_CONTROL_CODE_INFO;
    if(!HidD_GetFeature(file, &report, sizeof(report)))
      continue;
    Flags = report.Flags;
    QueueCapacity = report.MessageQueueCapacity;

    /* Save device. */
    ArxEtherealMouse = file;
    HeapFree(Heap, 0, deviceInterfaceDetailData);
    break;
  }

  /* Clean up & check for errors. */
  SetupDiDestroyDeviceInfoList(deviceInfoSet);

  if(ArxEtherealMouse == INVALID_HANDLE_VALUE)
    LastErrorMessage = DeviceNotFound;
}


VOID StopDll(void) {
  if(ArxEtherealMouse != INVALID_HANDLE_VALUE) {
    CloseHandle(ArxEtherealMouse);
    ArxEtherealMouse = INVALID_HANDLE_VALUE;
  }
  if(Heap != NULL) {
    HeapDestroy(Heap);
    Heap = NULL;
  }
}


BOOL WINAPI DllMain(HINSTANCE hinstDLL, DWORD fdwReason, LPVOID lpvReserved) {
  switch(fdwReason) {
  case DLL_PROCESS_ATTACH:
    /* Init. */
    StartDll();
    break;

  case DLL_THREAD_ATTACH:
    /* No thread-specific init code. */
    break;

  case DLL_THREAD_DETACH:
    /* No thread-specific cleanup code. */
    break;

  case DLL_PROCESS_DETACH:
    /* Cleanup. */
    StopDll();
    break;
  }
  
  /* The return value is used for successful DLL_PROCESS_ATTACH */
  return TRUE;
}

AEMCTLAPI AEMCTLRESULT AEMCTLAPIENTRY AemSendMessage(int x, int y, char buttons) {
  AEM_MOVE_FEATURE_REPORT report;

  if(ArxEtherealMouse == INVALID_HANDLE_VALUE)
    return AEMCTL_INIT_FAILED;

  if(Flags & AEM_FLAG_RELATIVE) {
    if(x < -127 || x > 127 || y < -127 || y > 127) {
      LastErrorMessage = OutOfBoundsRelative;
      return AEMCTL_INVALID_PARAMETER;
    }
  } else if (x < 1 || x > 32767 || y < 1 || y > 32767) {
    LastErrorMessage = OutOfBoundsAbsolute;
    return AEMCTL_INVALID_PARAMETER;
  }  
  
  report.Report.ReportId = AEM_CONTROL_REPORT_ID;
  report.Report.ControlCode = AEM_CONTROL_CODE_MOVE;
  report.Point.X = (SHORT) x;
  report.Point.Y = (SHORT) y;
  report.Buttons = buttons;

  if(!HidD_GetFeature(ArxEtherealMouse, &report, sizeof(report))) {
    WinApiCallFailed("HidD_GetFeature");
    return AEMCTL_COMMUNICATION_FAILED;
  } else {
    if(report.Report.ControlCode == AEM_CONTROL_CODE_MOVE) {
      return AEMCTL_OK;
    } else {
      LastErrorMessage = QueueFull;
      return AEMCTL_QUEUE_FULL;
    }
  }
}

AEMCTLAPI const char* AEMCTLAPIENTRY AemGetLastErrorString(void) {
  return LastErrorMessage;
}

AEMCTLAPI AEMCTLRESULT AEMCTLAPIENTRY AemGetDeviceInfo(int* isRelative, int* queueCapacity) {
  if(ArxEtherealMouse == INVALID_HANDLE_VALUE)
    return AEMCTL_INIT_FAILED;

  if(isRelative == NULL || queueCapacity == NULL) {
    LastErrorMessage = NullPassed;
    return AEMCTL_INVALID_PARAMETER;
  }

  *isRelative = Flags & AEM_FLAG_RELATIVE;
  *queueCapacity = QueueCapacity;
  return AEMCTL_OK;
}

AEMCTLAPI AEMCTLRESULT AEMCTLAPIENTRY AemClearMessageQueue() {
  AEM_FEATURE_REPORT report;

  if(ArxEtherealMouse == INVALID_HANDLE_VALUE)
    return AEMCTL_INIT_FAILED;

  report.ReportId = AEM_CONTROL_REPORT_ID;
  report.ControlCode = AEM_CONTROL_CODE_CLEAR_QUEUE;

  if(!HidD_GetFeature(ArxEtherealMouse, &report, sizeof(report))) {
    WinApiCallFailed("HidD_GetFeature");
    return AEMCTL_COMMUNICATION_FAILED;
  } else
    return AEMCTL_OK;
}

AEMCTLAPI AEMCTLRESULT AEMCTLAPIENTRY AemGetMessageQueueSize(int* size) {
  AEM_DWORD_FEATURE_REPORT report;

  if(ArxEtherealMouse == INVALID_HANDLE_VALUE)
    return AEMCTL_INIT_FAILED;

  if(size == NULL) {
    LastErrorMessage = NullPassed;
    return AEMCTL_INVALID_PARAMETER;
  }

  report.Report.ReportId = AEM_CONTROL_REPORT_ID;
  report.Report.ControlCode = AEM_CONTROL_CODE_QUEUE_SIZE;

  if(!HidD_GetFeature(ArxEtherealMouse, &report, sizeof(report))) {
    WinApiCallFailed("HidD_GetFeature");
    return AEMCTL_COMMUNICATION_FAILED;
  } else {
    *size = report.Value;
    return AEMCTL_OK;
  }
}


AEMCTLAPI AEMCTLRESULT AEMCTLAPIENTRY AemGetMessageCheckInterval(int* interval) {
  AEM_DWORD_FEATURE_REPORT report;

  if(ArxEtherealMouse == INVALID_HANDLE_VALUE)
    return AEMCTL_INIT_FAILED;

  if(interval == NULL) {
    LastErrorMessage = NullPassed;
    return AEMCTL_INVALID_PARAMETER;
  }

  report.Report.ReportId = AEM_CONTROL_REPORT_ID;
  report.Report.ControlCode = AEM_CONTROL_CODE_INTERVAL;
  report.Value = 0;

  if(!HidD_GetFeature(ArxEtherealMouse, &report, sizeof(report))) {
    WinApiCallFailed("HidD_GetFeature");
    return AEMCTL_COMMUNICATION_FAILED;
  } else {
    *interval = report.Value;
    return AEMCTL_OK;
  }
}

AEMCTLAPI AEMCTLRESULT AEMCTLAPIENTRY AemSetMessageCheckInterval(int interval) {
  AEM_DWORD_FEATURE_REPORT report;

  if(ArxEtherealMouse == INVALID_HANDLE_VALUE)
    return AEMCTL_INIT_FAILED;

  report.Report.ReportId = AEM_CONTROL_REPORT_ID;
  report.Report.ControlCode = AEM_CONTROL_CODE_INTERVAL;
  report.Value = interval;

  if(!HidD_GetFeature(ArxEtherealMouse, &report, sizeof(report))) {
    WinApiCallFailed("HidD_GetFeature");
    return AEMCTL_COMMUNICATION_FAILED;
  } else {
    if(report.Report.ControlCode == AEM_CONTROL_CODE_INTERVAL) {
      return AEMCTL_OK;
    } else {
      LastErrorMessage = MessageCheckIntervalTooSmall;
      return AEMCTL_INVALID_PARAMETER;
    }
  }
}
