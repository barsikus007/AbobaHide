#include "hooks.h"
#include "undocumented.h"
#include "ssdt.h"
#include "hider.h"
#include "log.h"
#include "ntdll.h"
#include "threadhidefromdbg.h"

static UNICODE_STRING DeviceName;
static UNICODE_STRING Win32Device;

static void DriverUnload(IN PDRIVER_OBJECT DriverObject)
{
    IoDeleteSymbolicLink(&Win32Device);
    IoDeleteDevice(DriverObject->DeviceObject);
    Hooks::Deinitialize();
    NTDLL::Deinitialize();
}

static NTSTATUS DriverCreateClose(IN PDEVICE_OBJECT DeviceObject, IN PIRP Irp)
{
    UNREFERENCED_PARAMETER(DeviceObject);
    Irp->IoStatus.Status = STATUS_SUCCESS;
    Irp->IoStatus.Information = 0;
    IoCompleteRequest(Irp, IO_NO_INCREMENT);
    return STATUS_SUCCESS;
}

static NTSTATUS DriverDefaultHandler(IN PDEVICE_OBJECT DeviceObject, IN PIRP Irp)
{
    UNREFERENCED_PARAMETER(DeviceObject);
    Irp->IoStatus.Status = STATUS_NOT_SUPPORTED;
    Irp->IoStatus.Information = 0;
    IoCompleteRequest(Irp, IO_NO_INCREMENT);
    return Irp->IoStatus.Status;
}

static NTSTATUS DriverWrite(IN PDEVICE_OBJECT DeviceObject, IN PIRP Irp)
{
    UNREFERENCED_PARAMETER(DeviceObject);
    NTSTATUS RetStatus = STATUS_SUCCESS;
    PIO_STACK_LOCATION pIoStackIrp = IoGetCurrentIrpStackLocation(Irp);
    if(pIoStackIrp)
    {
        PVOID pInBuffer = (PVOID)Irp->AssociatedIrp.SystemBuffer;
        if(pInBuffer)
        {
            if(Hider::ProcessData(pInBuffer, pIoStackIrp->Parameters.Write.Length))
                Log("[ABOBAHIDE] HiderProcessData OK!\r\n");
            else
            {
                Log("[ABOBAHIDE] HiderProcessData failed...\r\n");
                RetStatus = STATUS_UNSUCCESSFUL;
            }
        }
    }
    else
    {
        Log("[ABOBAHIDE] Invalid IRP stack pointer...\r\n");
        RetStatus = STATUS_UNSUCCESSFUL;
    }
    Irp->IoStatus.Status = RetStatus;
    Irp->IoStatus.Information = 0;
    IoCompleteRequest(Irp, IO_NO_INCREMENT);
    return Irp->IoStatus.Status;
}

extern "C" NTSTATUS DriverEntry(IN PDRIVER_OBJECT DriverObject, IN PUNICODE_STRING  RegistryPath)
{
    UNREFERENCED_PARAMETER(RegistryPath);
    PDEVICE_OBJECT DeviceObject = NULL;
    NTSTATUS status;

    //set callback functions
    DriverObject->DriverUnload = DriverUnload;
    for(unsigned int i = 0; i <= IRP_MJ_MAXIMUM_FUNCTION; i++)
        DriverObject->MajorFunction[i] = DriverDefaultHandler;
    DriverObject->MajorFunction[IRP_MJ_CREATE] = DriverCreateClose;
    DriverObject->MajorFunction[IRP_MJ_CLOSE] = DriverCreateClose;
    DriverObject->MajorFunction[IRP_MJ_WRITE] = DriverWrite;

    //read ntdll.dll from disk so we can use it for exports
    if(!NT_SUCCESS(NTDLL::Initialize()))
    {
        Log("[ABOBAHIDE] Ntdll::Initialize() failed...\r\n");
        return STATUS_UNSUCCESSFUL;
    }

    //initialize undocumented APIs
    if(!Undocumented::UndocumentedInit())
    {
        Log("[ABOBAHIDE] UndocumentedInit() failed...\r\n");
        return STATUS_UNSUCCESSFUL;
    }
    Log("[ABOBAHIDE] UndocumentedInit() was successful!\r\n");

    //find the offset of CrossThreadFlags in ETHREAD
    status = FindCrossThreadFlagsOffset(&CrossThreadFlagsOffset);
    if(!NT_SUCCESS(status))
    {
        Log("[ABOBAHIDE] FindCrossThreadFlagsOffset() failed: 0x%lX\r\n", status);
        return status;
    }

    //create io device
    RtlInitUnicodeString(&DeviceName, L"\\Device\\AbobaHide");
    RtlInitUnicodeString(&Win32Device, L"\\DosDevices\\AbobaHide");
    status = IoCreateDevice(DriverObject,
                            0,
                            &DeviceName,
                            FILE_DEVICE_UNKNOWN,
                            FILE_DEVICE_SECURE_OPEN,
                            FALSE,
                            &DeviceObject);
    if(!NT_SUCCESS(status))
    {
        Log("[ABOBAHIDE] IoCreateDevice Error...\r\n");
        return status;
    }
    if(!DeviceObject)
    {
        Log("[ABOBAHIDE] Unexpected I/O Error...\r\n");
        return STATUS_UNEXPECTED_IO_ERROR;
    }
    Log("[ABOBAHIDE] Device %.*ws created successfully!\r\n", DeviceName.Length / sizeof(WCHAR), DeviceName.Buffer);

    //create symbolic link
    DeviceObject->Flags |= DO_BUFFERED_IO;
    DeviceObject->Flags &= (~DO_DEVICE_INITIALIZING);
    status = IoCreateSymbolicLink(&Win32Device, &DeviceName);
    if(!NT_SUCCESS(status))
    {
        Log("[ABOBAHIDE] IoCreateSymbolicLink Error...\r\n");
        return status;
    }
    Log("[ABOBAHIDE] Symbolic link %.*ws->%.*ws created!\r\n", Win32Device.Length / sizeof(WCHAR), Win32Device.Buffer, DeviceName.Length / sizeof(WCHAR), DeviceName.Buffer);

    //initialize hooking
    Log("[ABOBAHIDE] Hooks::Initialize() hooked %d functions\r\n", Hooks::Initialize());

    return STATUS_SUCCESS;
}
