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
#include "aem.h"

#ifdef ALLOC_PRAGMA
  #pragma alloc_text(INIT, DriverEntry)
  #pragma alloc_text(PAGE, AddDevice)
  #pragma alloc_text(PAGE, Unload)
  #pragma alloc_text(PAGE, PnP)
#endif 

/** Installable driver initialization entry point. This entry point is called directly by the I/O system.
 * 
 * @param DriverObject                 Pointer to the driver object.
 * @param RegistryPath                 Pointer to a unicode string representing the path to driver-specific key in the registry.
 * @returns                            STATUS_SUCCESS if successful, STATUS_UNSUCCESSFUL otherwise. */
NTSTATUS DriverEntry(PDRIVER_OBJECT DriverObject, PUNICODE_STRING RegistryPath) {
  NTSTATUS                    ntStatus;
  HID_MINIDRIVER_REGISTRATION hidMinidriverRegistration;

  DebugPrint(("Enter DriverEntry()\n"));

  DriverObject->MajorFunction[IRP_MJ_INTERNAL_DEVICE_CONTROL] = InternalIoctl;
  DriverObject->MajorFunction[IRP_MJ_PNP]                     = PnP;
  DriverObject->MajorFunction[IRP_MJ_POWER]                   = Power;
  DriverObject->DriverUnload                                  = Unload;
  DriverObject->DriverExtension->AddDevice                    = AddDevice;

  RtlZeroMemory(&hidMinidriverRegistration, sizeof(hidMinidriverRegistration));

  /* Revision must be set to HID_REVISION by the minidriver. */
  hidMinidriverRegistration.Revision            = HID_REVISION;
  hidMinidriverRegistration.DriverObject        = DriverObject;
  hidMinidriverRegistration.RegistryPath        = RegistryPath;
  hidMinidriverRegistration.DeviceExtensionSize = sizeof(AEM_DEVICE_EXTENSION);

  /* If "DevicesArePolled" is FALSE then the hidclass driver does not do polling and instead reuses a 
   * few Irps (ping-pong) if the device has an Input item. Otherwise, it will do polling at regular intervals. 
   * USB HID devices do not need polling by the HID class driver. Some legacy devices may need polling. */
  hidMinidriverRegistration.DevicesArePolled = FALSE; 

  /* Register with hidclass. */
  ntStatus = HidRegisterMinidriver(&hidMinidriverRegistration);
  if(!NT_SUCCESS(ntStatus))
      DebugPrint(("HidRegisterMinidriver FAILED, returnCode=%x\n", ntStatus));
  
  DebugPrint(("Exit DriverEntry() status=0x%x\n", ntStatus));
  return ntStatus;
}

/** HidClass Driver calls our AddDevice routine after creating a FDO for us. 
 * We do not need to create a device object or attach it to the PDO. Hidclass driver will do it for us.
 * 
 * @param DriverObject                 Pointer to the driver object.
 * @param FunctionalDeviceObject       Pointer to the FDO created by the Hidclass driver for us.
 * @returns                            NT status code. */
NTSTATUS AddDevice(PDRIVER_OBJECT DriverObject, PDEVICE_OBJECT FunctionalDeviceObject) {
  NTSTATUS                  ntStatus = STATUS_SUCCESS;
  PAEM_DEVICE_EXTENSION deviceInfo;

  PAGED_CODE();
  DebugPrint(("Enter AddDevice(DriverObject=0x%x, FunctionalDeviceObject=0x%x)\n", DriverObject, FunctionalDeviceObject));
  ASSERTMSG("AddDevice:", FunctionalDeviceObject != NULL);

  deviceInfo = GET_MINIDRIVER_DEVICE_EXTENSION(FunctionalDeviceObject);

  /* Initialize all the members of device extension. */
  RtlZeroMemory(deviceInfo, sizeof(AEM_DEVICE_EXTENSION));

  /* Set the initial state of the FDO. */
  deviceInfo->DevicePnPState = NotStarted;
  deviceInfo->PreviousPnPState = NotStarted;
    
  deviceInfo->InfoReport.Report.ReportId = AEM_CONTROL_REPORT_ID;
  deviceInfo->InfoReport.Report.ControlCode = AEM_CONTROL_CODE_INFO;
  deviceInfo->InfoReport.Flags = 0;
#ifdef AEM_RELATIVE_MOTION
  deviceInfo->InfoReport.Flags |= AEM_FLAG_RELATIVE;
#endif
  deviceInfo->InfoReport.MessageQueueCapacity = AEM_MESSAGE_QUEUE_SIZE;

  KeInitializeSpinLock(&deviceInfo->MessageQueueLock);
  deviceInfo->MessageQueueEnd = 0;
  deviceInfo->MessageQueueStart = 0;

  deviceInfo->MessageCheckInterval = AEM_DEFAULT_MESSAGE_CHECK_INTERVAL;

  /* Initialization finished. */
  FunctionalDeviceObject->Flags &= ~DO_DEVICE_INITIALIZING;
  
  DebugPrint(("Exit AddDevice\n", ntStatus));
  return ntStatus;
}


/** Handles PnP IRPs sent to FDO.
 * 
 * @param DeviceObject                 Pointer to device object.
 * @param Irp                          Pointer to a PnP IRP.
 * @returns                            NT status code. */
NTSTATUS PnP(PDEVICE_OBJECT DeviceObject, PIRP Irp) {
  NTSTATUS                    ntStatus, queryStatus;
  PAEM_DEVICE_EXTENSION   deviceInfo;
  KEVENT                      startEvent;
  PIO_STACK_LOCATION          IrpStack, previousSp;
  PWCHAR                      buffer;

  PAGED_CODE();

  /* Get a pointer to the device extension. */
  deviceInfo = GET_MINIDRIVER_DEVICE_EXTENSION(DeviceObject);

  /* Get a pointer to the current location in the Irp. */
  IrpStack = IoGetCurrentIrpStackLocation(Irp);

  DebugPrint(("%s Irp=0x%x\n", PnPMinorFunctionString(IrpStack->MinorFunction), Irp));

  switch(IrpStack->MinorFunction) {
  case IRP_MN_START_DEVICE:
    KeInitializeEvent(&startEvent, NotificationEvent, FALSE);
    IoCopyCurrentIrpStackLocationToNext(Irp);
    IoSetCompletionRoutine(Irp, PnPComplete, &startEvent, TRUE, TRUE, TRUE);
    ntStatus = IoCallDriver(GET_NEXT_DEVICE_OBJECT(DeviceObject), Irp);

    if(ntStatus == STATUS_PENDING) {
        KeWaitForSingleObject(&startEvent, Executive, KernelMode, FALSE, NULL); /* No alerts & no timeout. */
        ntStatus = Irp->IoStatus.Status;
    }

    if(NT_SUCCESS(ntStatus)) {
      /* Use default "HID Descriptor" (hardcoded). 
       * Note that ReadDescriptorFromRegistry() changes wReportLength member. */
      deviceInfo->HidDescriptor = AemHidDescriptor;

      deviceInfo->ReportDescriptor = AemReportDescriptor;
      DebugPrint(("Using Hard-coded Report descriptor\n"));
      
      /* Set new PnP state. */
      if(NT_SUCCESS(ntStatus))
        SET_NEW_PNP_STATE(deviceInfo, Started);
    }

    Irp->IoStatus.Status = ntStatus;
    IoCompleteRequest(Irp, IO_NO_INCREMENT);
    return ntStatus;

  case IRP_MN_STOP_DEVICE:
    /* Mark the device as stopped. */
    SET_NEW_PNP_STATE(deviceInfo, Stopped);
    ntStatus = STATUS_SUCCESS;
    break;

  case IRP_MN_CANCEL_STOP_DEVICE:
    RESTORE_PREVIOUS_PNP_STATE(deviceInfo);
    ntStatus = STATUS_SUCCESS;         
    break;

  case IRP_MN_QUERY_STOP_DEVICE:
    SET_NEW_PNP_STATE(deviceInfo, StopPending);
    ntStatus = STATUS_SUCCESS;
    break;

  case IRP_MN_QUERY_REMOVE_DEVICE:
    SET_NEW_PNP_STATE(deviceInfo, RemovePending);
    ntStatus = STATUS_SUCCESS;
    break;

  case IRP_MN_CANCEL_REMOVE_DEVICE:
    RESTORE_PREVIOUS_PNP_STATE(deviceInfo);
    ntStatus = STATUS_SUCCESS;
    break;

  case IRP_MN_SURPRISE_REMOVAL:
    SET_NEW_PNP_STATE(deviceInfo, SurpriseRemovePending);
    ntStatus = STATUS_SUCCESS;
    break;

  case IRP_MN_REMOVE_DEVICE:
    /* Free memory if allocated for report descriptor */
    if(deviceInfo->ReadReportDescFromRegistry)
      ExFreePool(deviceInfo->ReportDescriptor);
    SET_NEW_PNP_STATE(deviceInfo, Deleted);
    ntStatus = STATUS_SUCCESS;           
    break;
      
  case IRP_MN_QUERY_ID:
    /* This check is required to filter out QUERY_IDs forwarded by the HIDCLASS for the parent FDO. 
     * These IDs are sent by PNP manager for the parent FDO if you root-enumerate this driver. */
    previousSp = ((PIO_STACK_LOCATION) ((UCHAR *) (IrpStack) + sizeof(IO_STACK_LOCATION)));
    
    if(previousSp->DeviceObject == DeviceObject) {
      /* Filtering out this basically prevents the Found New Hardware popup for the root-enumerated VHIDMINI on reboot. */
      ntStatus = Irp->IoStatus.Status;            
      break;
    }
            
    switch (IrpStack->Parameters.QueryId.IdType) {
    case BusQueryDeviceID:
    case BusQueryHardwareIDs:
      /* HIDClass is asking for child deviceid & hardwareids. Let us just make up some id for our child device. */
      buffer = (PWCHAR) ExAllocatePoolWithTag(NonPagedPool, AEM_HARDWARE_IDS_LENGTH, AEM_POOL_TAG);

      if(buffer) {
        /* Do the copy, store the buffer in the Irp. */
        RtlCopyMemory(buffer, AEM_HARDWARE_IDS, AEM_HARDWARE_IDS_LENGTH);
        Irp->IoStatus.Information = (ULONG_PTR) buffer;
        ntStatus = STATUS_SUCCESS;
      } else {
        /* No memory. */
        ntStatus = STATUS_INSUFFICIENT_RESOURCES;
      }
      Irp->IoStatus.Status = ntStatus;

      /* We don't need to forward this to our bus. 
       * This query is for our child so we should complete it right here. Fallthru. */
      IoCompleteRequest(Irp, IO_NO_INCREMENT);
      return ntStatus;           
             
    default:            
      ntStatus = Irp->IoStatus.Status;
      IoCompleteRequest(Irp, IO_NO_INCREMENT);          
      return ntStatus;
    }

  default:         
    ntStatus = Irp->IoStatus.Status;
    break;
  }
  
  Irp->IoStatus.Status = ntStatus;
  IoSkipCurrentIrpStackLocation(Irp);
  return IoCallDriver(GET_NEXT_DEVICE_OBJECT(DeviceObject), Irp);
}

NTSTATUS PnPComplete(PDEVICE_OBJECT DeviceObject, PIRP Irp,PVOID Context) {
  DebugPrint(("PnPComplete(DeviceObject=0x%x,Irp=0x%x,Context=0x%x)\n", DeviceObject, Irp, Context));
  UNREFERENCED_PARAMETER(DeviceObject);

  if(Irp->PendingReturned)
    KeSetEvent((PKEVENT) Context, 0, FALSE);
  return STATUS_MORE_PROCESSING_REQUIRED;
}

/** This routine is the dispatch routine for power irps.
 * Does nothing except forwarding the IRP to the next device in the stack.
 *
 * @param DeviceObject                 Pointer to the device object.
 * @param Irp                          Pointer to the request packet.
 * @returns                            NT Status code. */
NTSTATUS Power(PDEVICE_OBJECT DeviceObject, PIRP Irp) {
  DebugPrint(("Enter Power()\n"));
  
  /* Make sure to store and restore the device context depending on whether the system is going into 
   * lower power state or resuming. The job of converting S-IRPs to D-IRPs is done by HIDCLASS. 
   * All you need to do here is handle Set-Power request for device power state according to the 
   * guidelines given in the power management documentation of the DDK. 
   * Before powering down your device make sure to cancel any pending IRPs, if any, sent by you to the lower device stack. */

  PoStartNextPowerIrp(Irp);     
  IoSkipCurrentIrpStackLocation(Irp);
  return PoCallDriver(GET_NEXT_DEVICE_OBJECT(DeviceObject), Irp);
}


/** Free all the allocated resources, etc.
 *
 * @param DriverObject                 Pointer to a driver object. */
VOID Unload(PDRIVER_OBJECT DriverObject) {
  PAGED_CODE();
  
  /* The device object(s) should be NULL now 
   * (since we unload, all the devices objects associated with this driver must have been deleted). */
  DebugPrint(("Enter Unload()\n"));
  ASSERT(DriverObject->DeviceObject == NULL);
  return;
}


/** Handles all the internal ioctls.
 *
 * @param DeviceObject                 Pointer to the device object.
 * @param Irp                          Pointer to the request packet.
 * @returns                            NT Status code. */
NTSTATUS InternalIoctl(PDEVICE_OBJECT DeviceObject, PIRP Irp) {
  NTSTATUS            ntStatus = STATUS_SUCCESS;
  PIO_STACK_LOCATION  IrpStack;

  //DebugPrint(("Enter InternalIoctl (DO=0x%x,Irp=0x%x)\n", DeviceObject, Irp));
  IrpStack = IoGetCurrentIrpStackLocation(Irp);

  switch(IrpStack->Parameters.DeviceIoControl.IoControlCode) {
  case IOCTL_HID_GET_DEVICE_DESCRIPTOR:
    /* Retrieves the device's HID descriptor. */
    DebugPrint(("IOCTL_HID_GET_DEVICE_DESCRIPTOR\n"));
    ntStatus = GetHidDescriptor(DeviceObject, Irp);
    break;

  case IOCTL_HID_GET_REPORT_DESCRIPTOR:
    /* Obtains the report descriptor for the HID device. */
    DebugPrint(("IOCTL_HID_GET_REPORT_DESCRIPTOR\n"));
    ntStatus = GetReportDescriptor(DeviceObject, Irp);
    break;

  case IOCTL_HID_READ_REPORT:
    /* Return a report from the device into a class driver-supplied buffer. */
    //DebugPrint(("IOCTL_HID_READ_REPORT\n"));
    ntStatus = ReadReport(DeviceObject, Irp);
    return ntStatus;
      
  case IOCTL_HID_GET_DEVICE_ATTRIBUTES:
    /* Retrieves a device's attributes in a HID_DEVICE_ATTRIBUTES structure. */
    DebugPrint(("IOCTL_HID_GET_DEVICE_ATTRIBUTES\n"));
    ntStatus = GetDeviceAttributes(DeviceObject, Irp);
    break;

  case IOCTL_HID_WRITE_REPORT:
    /* Transmits a class driver-supplied report to the device. */
    DebugPrint(("IOCTL_HID_WRITE_REPORT\n"));
    ntStatus = STATUS_NOT_SUPPORTED;
    break;

  case IOCTL_HID_SET_FEATURE:
    /* This sends a HID class feature report to a top-level collection of a HID class device. */
    DebugPrint(("IOCTL_HID_SET_FEATURE\n"));
    ntStatus = STATUS_NOT_SUPPORTED;
    break;

  case IOCTL_HID_GET_FEATURE:
    DebugPrint(("IOCTL_HID_GET_FEATURE\n"));
    ntStatus = GetFeature(DeviceObject, Irp);                
    break;

  case IOCTL_HID_GET_STRING:
    /* Requests that the HID minidriver retrieve a human-readable string for either the manufacturer ID, 
     * the product ID, or the serial number from the string descriptor of the device. 
     * The minidriver must send a Get String Descriptor request to the device, in order to retrieve 
     * the string descriptor, then it must extract the string at the appropriate index from the string descriptor 
     * and return it in the output buffer indicated by the IRP. 
     * Before sending the Get String Descriptor request, the minidriver must retrieve the appropriate 
     * index for the manufacturer ID, the product ID or the serial number from the device extension of a top 
     * level collection associated with the device. */
    DebugPrint(("IOCTL_HID_GET_STRING\n"));
    ntStatus = STATUS_NOT_SUPPORTED;
    break;

  case IOCTL_HID_ACTIVATE_DEVICE:
    /* Makes the device ready for I/O operations. */
    DebugPrint(("IOCTL_HID_ACTIVATE_DEVICE\n"));
    ntStatus = STATUS_NOT_SUPPORTED;
    break;

  case IOCTL_HID_DEACTIVATE_DEVICE:
    /* Causes the device to cease operations and terminate all outstanding I/O requests. */
    DebugPrint(("IOCTL_HID_DEACTIVATE_DEVICE\n"));
    ntStatus = STATUS_NOT_SUPPORTED;
    break;

  default:
    DebugPrint(("Unknown or unsupported IOCTL (%x)\n", IrpStack->Parameters.DeviceIoControl.IoControlCode));
    ntStatus = STATUS_NOT_SUPPORTED;
    break;
  }

  /* Set real return status in Irp. */
  Irp->IoStatus.Status = ntStatus;
  IoCompleteRequest(Irp, IO_NO_INCREMENT);
  ntStatus = STATUS_SUCCESS;

  return ntStatus;
} 

/** Handles Ioctls for get feature for all the collection. 
 * For control collection (custom defined collection) it handles the user-defined control codes for sideband communication.
 *
 * @param DeviceObject                 Pointer to a device object.
 * @param Irp                          Pointer to Interrupt Request Packet.
 * @returns                            NT status code. */
NTSTATUS GetFeature(PDEVICE_OBJECT DeviceObject, PIRP Irp) {
  PIO_STACK_LOCATION        IrpStack;
  PHID_XFER_PACKET          transferPacket = NULL;
  PAEM_DEVICE_EXTENSION     deviceInfo;
  PAEM_FEATURE_REPORT       featureReport;
  KIRQL                     irql;

  IrpStack = IoGetCurrentIrpStackLocation(Irp);
  deviceInfo = GET_MINIDRIVER_DEVICE_EXTENSION(DeviceObject);
  transferPacket = (PHID_XFER_PACKET) Irp->UserBuffer;

  if(transferPacket->reportId == AEM_CONTROL_REPORT_ID) {
    DebugPrint(("Report Id AEM_CONTROL_REPORT_ID devinfo=0x%x irql=%d\n", deviceInfo, KeGetCurrentIrql()));
    if(transferPacket->reportBufferLen < sizeof(AEM_FEATURE_REPORT))
      return STATUS_BUFFER_TOO_SMALL;

    featureReport = (PAEM_FEATURE_REPORT) transferPacket->reportBuffer;
    switch(featureReport->ControlCode) {
    case AEM_CONTROL_CODE_MOVE: {
      PAEM_MOVE_FEATURE_REPORT report = (PAEM_MOVE_FEATURE_REPORT) transferPacket->reportBuffer;
      DWORD32 newQueueEnd;
      if(transferPacket->reportBufferLen < sizeof(AEM_MOVE_FEATURE_REPORT))
        return STATUS_BUFFER_TOO_SMALL;
      KeAcquireSpinLock(&deviceInfo->MessageQueueLock, &irql);
      newQueueEnd = (deviceInfo->MessageQueueEnd + 1) % AEM_MESSAGE_QUEUE_SIZE;
      if(newQueueEnd != deviceInfo->MessageQueueStart) {
        RtlCopyMemory(&deviceInfo->MessageQueue[deviceInfo->MessageQueueEnd], transferPacket->reportBuffer, sizeof(AEM_MOVE_FEATURE_REPORT));
        deviceInfo->MessageQueueEnd = newQueueEnd;
      } else
        report->Report.ControlCode = AEM_CONTROL_CODE_ERROR;   
      KeReleaseSpinLock(&deviceInfo->MessageQueueLock, irql);
      break;
    }
    case AEM_CONTROL_CODE_INFO: {
      if(transferPacket->reportBufferLen < sizeof(AEM_INFO_FEATURE_REPORT))
        return STATUS_BUFFER_TOO_SMALL;
      RtlCopyMemory(transferPacket->reportBuffer, &deviceInfo->InfoReport, sizeof(AEM_INFO_FEATURE_REPORT));
      break;
    }
    case AEM_CONTROL_CODE_CLEAR_QUEUE: {
      KeAcquireSpinLock(&deviceInfo->MessageQueueLock, &irql);
      deviceInfo->MessageQueueStart = 0;
      deviceInfo->MessageQueueEnd = 0;
      KeReleaseSpinLock(&deviceInfo->MessageQueueLock, irql);
      break;
    }
    case AEM_CONTROL_CODE_QUEUE_SIZE: {
      PAEM_DWORD_FEATURE_REPORT report = (PAEM_DWORD_FEATURE_REPORT) transferPacket->reportBuffer;
      int value;
      if(transferPacket->reportBufferLen < sizeof(AEM_DWORD_FEATURE_REPORT))
        return STATUS_BUFFER_TOO_SMALL;
      value = deviceInfo->MessageQueueEnd - deviceInfo->MessageQueueStart;
      report->Value = value < 0 ? value + AEM_MESSAGE_QUEUE_SIZE : value;
      break;
    }
    case AEM_CONTROL_CODE_INTERVAL: {
      DWORD32                   newDelay;
      PAEM_DWORD_FEATURE_REPORT report = (PAEM_DWORD_FEATURE_REPORT) transferPacket->reportBuffer;
      if(transferPacket->reportBufferLen < sizeof(AEM_DWORD_FEATURE_REPORT))
        return STATUS_BUFFER_TOO_SMALL;
      newDelay = report->Value;
      report->Value = deviceInfo->MessageCheckInterval;
      if(newDelay >= AEM_MINIMAL_MESSAGE_CHECK_INTERVAL)
        deviceInfo->MessageCheckInterval = newDelay;
      else
        report->Report.ControlCode = AEM_CONTROL_CODE_ERROR;
      break;
    }
    default:
      return STATUS_NOT_SUPPORTED;
    }

    return STATUS_SUCCESS;
  } else
    return STATUS_NOT_SUPPORTED;
}


/** Finds the HID descriptor and copies it into the buffer provided by the Irp.
 * 
 * @param DeviceObject                 Pointer to a device object.
 * @param Irp                          Pointer to Interrupt Request Packet.
 * @returns                            NT status code. */
NTSTATUS GetHidDescriptor(PDEVICE_OBJECT DeviceObject, PIRP Irp) {
  NTSTATUS                  ntStatus = STATUS_SUCCESS;
  PAEM_DEVICE_EXTENSION deviceInfo;
  PIO_STACK_LOCATION        IrpStack;
  ULONG                     bytesToCopy;

  DebugPrint(("GetHIDDescriptor Entry\n"));
  deviceInfo = GET_MINIDRIVER_DEVICE_EXTENSION(DeviceObject);
  IrpStack = IoGetCurrentIrpStackLocation(Irp);

  DebugPrint(("HIDCLASS Buffer = 0x%x, Buffer length = 0x%x\n", Irp->UserBuffer, IrpStack->Parameters.DeviceIoControl.OutputBufferLength));
  bytesToCopy = IrpStack->Parameters.DeviceIoControl.OutputBufferLength;
  if(!bytesToCopy)
    return STATUS_BUFFER_TOO_SMALL;

  /* Since HidDescriptor.bLength could be >= sizeof(HID_DESCRIPTOR) we just check for HidDescriptor.bLength and 
   * copy MIN (OutputBufferLength, DeviceExtension->HidDescriptor->bLength) */
  if(bytesToCopy > deviceInfo->HidDescriptor.bLength)
    bytesToCopy = deviceInfo->HidDescriptor.bLength;

  DebugPrint(("Copying %d bytes to HIDCLASS buffer\n", bytesToCopy));

  RtlCopyMemory((PUCHAR) Irp->UserBuffer, (PUCHAR) &deviceInfo->HidDescriptor, bytesToCopy);                

  DebugPrint(("   bLength: 0x%x \n"
              "   bDescriptorType: 0x%x \n"
              "   bcdHID: 0x%x \n"
              "   bCountry: 0x%x \n"
              "   bNumDescriptors: 0x%x \n"
              "   bReportType: 0x%x \n"
              "   wReportLength: 0x%x \n",
              deviceInfo->HidDescriptor.bLength, 
              deviceInfo->HidDescriptor.bDescriptorType,
              deviceInfo->HidDescriptor.bcdHID,
              deviceInfo->HidDescriptor.bCountry,
              deviceInfo->HidDescriptor.bNumDescriptors,
              deviceInfo->HidDescriptor.DescriptorList[0].bReportType,
              deviceInfo->HidDescriptor.DescriptorList[0].wReportLength));

  /* Report how many bytes were copied. */
  Irp->IoStatus.Information = bytesToCopy;

  DebugPrint(("HidMiniGetHIDDescriptor Exit = 0x%x\n", ntStatus));
  return ntStatus;
}


/** Creates reports and sends it back to the requester.
 *
 * @param DeviceObject                 Pointer to a device object.
 * @param Irp                          Pointer to Interrupt Request Packet.
 * @returns                            NT status code. */
NTSTATUS ReadReport(PDEVICE_OBJECT DeviceObject, PIRP Irp) {
  NTSTATUS                  ntStatus = STATUS_PENDING;
  PAEM_DEVICE_EXTENSION deviceInfo;
  PIO_STACK_LOCATION        IrpStack;
  LARGE_INTEGER             timeout;
  PREAD_TIMER               readTimer;

  //DebugPrint(("ReadReport Entry, irql=%d\n", KeGetCurrentIrql()));
  deviceInfo = GET_MINIDRIVER_DEVICE_EXTENSION(DeviceObject);

  /* Allocate the Timer structure. */
  readTimer = ExAllocatePoolWithTag(NonPagedPool, sizeof(READ_TIMER), AEM_POOL_TAG);

  if(!readTimer) {
    DebugPrint(("Mem allocation for readTimer failed\n"));
    Irp->IoStatus.Status = ntStatus = STATUS_INSUFFICIENT_RESOURCES;
    IoCompleteRequest(Irp, IO_NO_INCREMENT);
  } else {
    RtlZeroMemory(readTimer, sizeof(READ_TIMER));
    
    /* Since the IRP will be completed later in the DPC, mark the IRP pending and return STATUS_PENDING. */
    IoMarkIrpPending(Irp);
    
    /* Remember the Irp & DeviceObject. */
    readTimer->Irp = Irp;
    readTimer->DeviceObject = DeviceObject;

    /* Initialize the DPC structure and Timer. */
    KeInitializeDpc(&readTimer->ReadTimerDpc, ReadTimerDpcRoutine, (PVOID) readTimer);
    KeInitializeTimer(&readTimer->ReadTimer);

    /* Queue the timer DPC. */
    timeout.HighPart = -1;
    timeout.LowPart = -(LONG) (10 * deviceInfo->MessageCheckInterval); /* In 100 ns. */
    KeSetTimer(&readTimer->ReadTimer, timeout, &readTimer->ReadTimerDpc);
  }
  
  //DebugPrint(("ReadReport Exit = 0x%x\n", ntStatus));
  return ntStatus;
}

VOID ReadTimerDpcRoutine(PKDPC Dpc, PVOID DeferredContext, PVOID SystemArgument1, PVOID SystemArgument2) {
  NTSTATUS                  ntStatus = STATUS_SUCCESS;
  PAEM_DEVICE_EXTENSION     deviceInfo;
  PIRP                      Irp;
  PIO_STACK_LOCATION        IrpStack;
  PREAD_TIMER               readTimer;
  ULONG                     reportSize = AEM_INPUT_REPORT_SIZE + 1;
  PUCHAR                    readReport;
  LARGE_INTEGER             timeout;
  AEM_MOVE_FEATURE_REPORT   moveReport;
  KIRQL                     irql;

  readTimer = (PREAD_TIMER) DeferredContext;
  Irp = readTimer->Irp;
  IrpStack = IoGetCurrentIrpStackLocation(Irp);
  deviceInfo = GET_MINIDRIVER_DEVICE_EXTENSION(readTimer->DeviceObject);
  readReport = (PUCHAR) Irp->UserBuffer;

  //DebugPrint(("ReadTimerDpcRoutine Entry, devinfo=0x%x, irql=%d\n", deviceInfo, KeGetCurrentIrql()));

  if(IrpStack->Parameters.DeviceIoControl.OutputBufferLength < reportSize) {
    /* First check the size of the output buffer. */
    DebugPrint(("ReadReport: Buffer too small, output=0x%x need=0x%x\n", IrpStack->Parameters.DeviceIoControl.OutputBufferLength, reportSize));
    ntStatus = STATUS_BUFFER_TOO_SMALL;
  } else if(deviceInfo->MessageQueueStart == deviceInfo->MessageQueueEnd) {
    /* Then check whether there is any input. */
    //DebugPrint(("ReadReport: nothing to output\n"));

    /* Ignore this one. */
    readReport[0] = AEM_CONTROL_REPORT_ID;

    /* Report how many bytes were copied. */
    Irp->IoStatus.Information = reportSize;
  } else {
    KeAcquireSpinLock(&deviceInfo->MessageQueueLock, &irql);
    RtlCopyMemory(&moveReport, &deviceInfo->MessageQueue[deviceInfo->MessageQueueStart], sizeof(AEM_MOVE_FEATURE_REPORT));
    deviceInfo->MessageQueueStart = (deviceInfo->MessageQueueStart + 1) % AEM_MESSAGE_QUEUE_SIZE;
    KeReleaseSpinLock(&deviceInfo->MessageQueueLock, irql);
    
    /* Create input report. */
    //DebugPrint(("%d %d %d %d\n", (int) moveReport.Report.ReportId, (int) moveReport.Point.X, (int) moveReport.Point.Y, (int) moveReport.Buttons));
    readReport[0] = AEM_POINTER_REPORT_ID;
    readReport[1] = moveReport.Buttons;
#ifdef AEM_RELATIVE_MOTION
    readReport[2] = (UCHAR) moveReport.Point.X;
    readReport[3] = (UCHAR) moveReport.Point.Y;
#else
    *((PSHORT_POINT) (readReport + 2)) = moveReport.Point;
#endif
    
    /* Copy input report to the Irp buffer. */
    RtlCopyMemory(Irp->UserBuffer, readReport, reportSize);
    
    /* Report how many bytes were copied. */
    Irp->IoStatus.Information = reportSize;
  }

  /* Set real return status in Irp. */
  Irp->IoStatus.Status = ntStatus;
  IoCompleteRequest(Irp, IO_NO_INCREMENT);

  /* Free the DPC structure. */
  ExFreePool(readTimer);
  
  //DebugPrint(("ReadTimerDpcRoutine Exit = 0x%x\n", ntStatus));
}


/** Finds the Report descriptor and copies it into the buffer provided by the Irp.
 *
 * @param DeviceObject                 Pointer to a device object.
 * @param Irp                          Pointer to Interrupt Request Packet.
 * @returns                            NT status code. */
NTSTATUS GetReportDescriptor(PDEVICE_OBJECT DeviceObject, PIRP Irp) {
  NTSTATUS                  ntStatus = STATUS_SUCCESS;
  PAEM_DEVICE_EXTENSION deviceInfo;
  PIO_STACK_LOCATION        IrpStack;
  ULONG                     bytesToCopy;

  DebugPrint(("HidMiniGetReportDescriptor Entry\n"));
  IrpStack = IoGetCurrentIrpStackLocation(Irp);
  deviceInfo = GET_MINIDRIVER_DEVICE_EXTENSION(DeviceObject);

  /* Copy device descriptor to HIDCLASS buffer. */
  DebugPrint(("HIDCLASS Buffer = 0x%x, Buffer length = 0x%x\n", Irp->UserBuffer, IrpStack->Parameters.DeviceIoControl.OutputBufferLength));
  bytesToCopy = IrpStack->Parameters.DeviceIoControl.OutputBufferLength;
  if(!bytesToCopy)
    return STATUS_BUFFER_TOO_SMALL;
  if(bytesToCopy > deviceInfo->HidDescriptor.DescriptorList[0].wReportLength)
      bytesToCopy = deviceInfo->HidDescriptor.DescriptorList[0].wReportLength;
  DebugPrint(("Copying 0x%x bytes to HIDCLASS buffer\n", bytesToCopy));
  RtlCopyMemory((PUCHAR) Irp->UserBuffer, (PUCHAR) deviceInfo->ReportDescriptor, bytesToCopy);

  /* Report how many bytes were copied. */
  Irp->IoStatus.Information = bytesToCopy;

  DebugPrint(("HidMiniGetReportDescriptor Exit = 0x%x\n", ntStatus));
  return ntStatus;
}


/** Fill in the given struct _HID_DEVICE_ATTRIBUTES.
 * 
 * @param DeviceObject                 Pointer to a device object.
 * @param Irp                          Pointer to Interrupt Request Packet.
 * @returns                            NT status code. */
NTSTATUS GetDeviceAttributes(PDEVICE_OBJECT DeviceObject, PIRP Irp) {
  NTSTATUS               ntStatus = STATUS_SUCCESS;
  PIO_STACK_LOCATION     irpStack;
  PHID_DEVICE_ATTRIBUTES deviceAttributes;

  DebugPrint(("HidMiniGetDeviceAttributes Entry\n"));

  irpStack = IoGetCurrentIrpStackLocation(Irp);
  deviceAttributes = (PHID_DEVICE_ATTRIBUTES) Irp->UserBuffer;
  if(sizeof(HID_DEVICE_ATTRIBUTES) > irpStack->Parameters.DeviceIoControl.OutputBufferLength)
    return STATUS_BUFFER_TOO_SMALL;
  
  /* Report how many bytes were copied. */
  Irp->IoStatus.Information = sizeof(HID_DEVICE_ATTRIBUTES);
  
  /* Write result. */
  deviceAttributes->Size = sizeof(HID_DEVICE_ATTRIBUTES);
  deviceAttributes->VendorID = AEM_VENDOR_ID;
  deviceAttributes->ProductID = AEM_PRODUCT_ID;
  deviceAttributes->VersionNumber = AEM_VERSION;

  DebugPrint(("HidMiniGetAttributes Exit = 0x%x\n", ntStatus));
  return ntStatus;
}

#if DBG
PCHAR PnPMinorFunctionString(UCHAR MinorFunction) {
  switch (MinorFunction) {
  case IRP_MN_START_DEVICE:
    return "IRP_MN_START_DEVICE";
  case IRP_MN_QUERY_REMOVE_DEVICE:
    return "IRP_MN_QUERY_REMOVE_DEVICE";
  case IRP_MN_REMOVE_DEVICE:
    return "IRP_MN_REMOVE_DEVICE";
  case IRP_MN_CANCEL_REMOVE_DEVICE:
    return "IRP_MN_CANCEL_REMOVE_DEVICE";
  case IRP_MN_STOP_DEVICE:
    return "IRP_MN_STOP_DEVICE";
  case IRP_MN_QUERY_STOP_DEVICE:
    return "IRP_MN_QUERY_STOP_DEVICE";
  case IRP_MN_CANCEL_STOP_DEVICE:
    return "IRP_MN_CANCEL_STOP_DEVICE";
  case IRP_MN_QUERY_DEVICE_RELATIONS:
    return "IRP_MN_QUERY_DEVICE_RELATIONS";
  case IRP_MN_QUERY_INTERFACE:
    return "IRP_MN_QUERY_INTERFACE";
  case IRP_MN_QUERY_CAPABILITIES:
    return "IRP_MN_QUERY_CAPABILITIES";
  case IRP_MN_QUERY_RESOURCES:
    return "IRP_MN_QUERY_RESOURCES";
  case IRP_MN_QUERY_RESOURCE_REQUIREMENTS:
    return "IRP_MN_QUERY_RESOURCE_REQUIREMENTS";
  case IRP_MN_QUERY_DEVICE_TEXT:
    return "IRP_MN_QUERY_DEVICE_TEXT";
  case IRP_MN_FILTER_RESOURCE_REQUIREMENTS:
    return "IRP_MN_FILTER_RESOURCE_REQUIREMENTS";
  case IRP_MN_READ_CONFIG:
    return "IRP_MN_READ_CONFIG";
  case IRP_MN_WRITE_CONFIG:
    return "IRP_MN_WRITE_CONFIG";
  case IRP_MN_EJECT:
    return "IRP_MN_EJECT";
  case IRP_MN_SET_LOCK:
    return "IRP_MN_SET_LOCK";
  case IRP_MN_QUERY_ID:
    return "IRP_MN_QUERY_ID";
  case IRP_MN_QUERY_PNP_DEVICE_STATE:
    return "IRP_MN_QUERY_PNP_DEVICE_STATE";
  case IRP_MN_QUERY_BUS_INFORMATION:
    return "IRP_MN_QUERY_BUS_INFORMATION";
  case IRP_MN_DEVICE_USAGE_NOTIFICATION:
    return "IRP_MN_DEVICE_USAGE_NOTIFICATION";
  case IRP_MN_SURPRISE_REMOVAL:
    return "IRP_MN_SURPRISE_REMOVAL";
  default:
    return "unknown_pnp_irp";
  }
}
#endif

