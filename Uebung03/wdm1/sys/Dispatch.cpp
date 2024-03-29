//////////////////////////////////////////////////////////////////////////////
//	Copyright � 1998 Chris Cant, PHD Computer Consultants Ltd
//	WDM Book for R&D Books, Miller Freeman Inc
//
//	Wdm1 example
/////////////////////////////////////////////////////////////////////////////
//	dispatch.cpp:	Other IRP handlers
/////////////////////////////////////////////////////////////////////////////
//	Wdm1Create			Handle Create/Open file IRP
//	Wdm1Close			Handle Close file IRPs
//	Wdm1Read			Handle Read IRPs
//	Wdm1Write			Handle Write IRPs
//	Wdm1DeviceControl	Handle DeviceIoControl IRPs
//	Wdm1SystemControl	Handle WMI IRPs
/////////////////////////////////////////////////////////////////////////////
//	Version history
//	27-Apr-99	1.0.0	CC	creation
/////////////////////////////////////////////////////////////////////////////

#include "wdm1.h"
#include "Ioctl.h"

#define   SER_MCR(x)   ((x)+4)
#define     SR_MCR_DTR 0x01
#define     SR_MCR_RTS 0x02
#define   SER_MSR(x)   ((x)+6)
#define     SR_MSR_CTS 0x10
#define     SR_MSR_DSR 0x20

/////////////////////////////////////////////////////////////////////////////
/////////////////////////////////////////////////////////////////////////////
//	Buffer and BufferSize and guarding spin lock globals (in unpaged memory)

KSPIN_LOCK BufferLock;
PUCHAR	Buffer = NULL;
ULONG	BufferSize = 0;

int const dateTimeSize = 21;
char dateTimeBuffer[dateTimeSize];

PUCHAR COM1_BASEADRESS = ((PUCHAR)(ULONG_PTR)0x3F8);

/////////////////////////////////////////////////////////////////////////////
//	Wdm1Create:
//
//	Description:
//		Handle IRP_MJ_CREATE requests
//
//	Arguments:
//		Pointer to our FDO
//		Pointer to the IRP
//			IrpStack->Parameters.Create.xxx has create parameters
//			IrpStack->FileObject->FileName has file name of device
//
//	Return Value:
//		This function returns STATUS_XXX

NTSTATUS Wdm1Create(IN PDEVICE_OBJECT fdo,
	IN PIRP Irp)
{
	PIO_STACK_LOCATION IrpStack = IoGetCurrentIrpStackLocation(Irp);
	DebugPrint("Create File is %T", &(IrpStack->FileObject->FileName));

	// Complete successfully
	return CompleteIrp(Irp, STATUS_SUCCESS, 0);
}

/////////////////////////////////////////////////////////////////////////////
//	Wdm1Close:
//
//	Description:
//		Handle IRP_MJ_CLOSE requests
//
//	Arguments:
//		Pointer to our FDO
//		Pointer to the IRP
//
//	Return Value:
//		This function returns STATUS_XXX

NTSTATUS Wdm1Close(IN PDEVICE_OBJECT fdo,
	IN PIRP Irp)
{
	DebugPrintMsg("Close");

	// Complete successfully
	return CompleteIrp(Irp, STATUS_SUCCESS, 0);
}

/////////////////////////////////////////////////////////////////////////////
//	Wdm1Read:
//	
//	Description:
//		Handle IRP_MJ_READ requests
//
//	Arguments:
//		Pointer to our FDO
//		Pointer to the IRP
//			IrpStack->Parameters.Read.xxx has read parameters
//			User buffer at:	AssociatedIrp.SystemBuffer	(buffered I/O)
//							MdlAddress					(direct I/O)
//
//	Return Value:
//		This function returns STATUS_XXX

NTSTATUS Wdm1Read(IN PDEVICE_OBJECT fdo,
	IN PIRP Irp)
{
	PIO_STACK_LOCATION IrpStack = IoGetCurrentIrpStackLocation(Irp);
	NTSTATUS status = STATUS_SUCCESS;
	LONG BytesTxd = 0;

	// Get call parameters
	LONGLONG FilePointer = IrpStack->Parameters.Read.ByteOffset.QuadPart;
	ULONG ReadLen = IrpStack->Parameters.Read.Length;
	DebugPrint("Read %d bytes from file pointer %d", (int)ReadLen, (int)FilePointer);

	// Get access to the shared buffer
	KIRQL irql;
	KeAcquireSpinLock(&BufferLock, &irql);

	// Check file pointer
	if (FilePointer < 0)
		status = STATUS_INVALID_PARAMETER;
	if (FilePointer >= (LONGLONG)BufferSize)
		status = STATUS_END_OF_FILE;

	if (status == STATUS_SUCCESS)
	{
		// Get transfer count
		if (((ULONG)FilePointer) + ReadLen > BufferSize)
		{
			BytesTxd = BufferSize - (ULONG)FilePointer;
			if (BytesTxd < 0) BytesTxd = 0;
		}
		else
			BytesTxd = ReadLen;

		// Read from shared buffer
		if (BytesTxd>0 && Buffer != NULL)
			RtlCopyMemory(Irp->AssociatedIrp.SystemBuffer, Buffer + FilePointer, BytesTxd);
	}

	// Release shared buffer
	KeReleaseSpinLock(&BufferLock, irql);

	DebugPrint("Read: %d bytes returned", (int)BytesTxd);

	// Complete IRP
	return CompleteIrp(Irp, status, BytesTxd);
}

/////////////////////////////////////////////////////////////////////////////
//	Wdm1Write:
//
//	Description:
//		Handle IRP_MJ_WRITE requests
//
//	Arguments:
//		Pointer to our FDO
//		Pointer to the IRP
//			IrpStack->Parameters.Write.xxx has write parameters
//			User buffer at:	AssociatedIrp.SystemBuffer	(buffered I/O)
//							MdlAddress					(direct I/O)
//
//	Return Value:
//		This function returns STATUS_XXX

NTSTATUS Wdm1Write(IN PDEVICE_OBJECT fdo,
	IN PIRP Irp)
{
	PIO_STACK_LOCATION IrpStack = IoGetCurrentIrpStackLocation(Irp);
	NTSTATUS status = STATUS_SUCCESS;
	LONG BytesTxd = 0;

	// Get call parameters
	LONGLONG FilePointer = IrpStack->Parameters.Write.ByteOffset.QuadPart;
	ULONG WriteLen = IrpStack->Parameters.Write.Length;
	DebugPrint("Write %d bytes from file pointer %d", (int)WriteLen, (int)FilePointer);

	if (FilePointer < 0)
		status = STATUS_INVALID_PARAMETER;
	else
	{
		// Get access to the shared buffer
		KIRQL irql;
		KeAcquireSpinLock(&BufferLock, &irql);

		BytesTxd = WriteLen;

		// (Re)allocate buffer if necessary
		if (((ULONG)FilePointer) + WriteLen > BufferSize)
		{
			ULONG NewBufferSize = ((ULONG)FilePointer) + WriteLen;
			PVOID NewBuffer = ExAllocatePool(NonPagedPool, NewBufferSize);
			if (NewBuffer == NULL)
			{
				BytesTxd = BufferSize - (ULONG)FilePointer;
				if (BytesTxd < 0) BytesTxd = 0;
			}
			else
			{
				RtlZeroMemory(NewBuffer, NewBufferSize);
				if (Buffer != NULL)
				{
					RtlCopyMemory(NewBuffer, Buffer, BufferSize);
					ExFreePool(Buffer);
				}
				Buffer = (PUCHAR)NewBuffer;
				BufferSize = NewBufferSize;
			}
		}

		// Write to shared memory
		if (BytesTxd>0 && Buffer != NULL)
			RtlCopyMemory(Buffer + FilePointer, Irp->AssociatedIrp.SystemBuffer, BytesTxd);

		// Release shared buffer
		KeReleaseSpinLock(&BufferLock, irql);
	}

	DebugPrint("Write: %d bytes written", (int)BytesTxd);

	// Complete IRP
	return CompleteIrp(Irp, status, BytesTxd);
}

/////////////////////////////////////////////////////////////////////////////
//	Wdm1DeviceControl:
//
//	Description:
//		Handle IRP_MJ_DEVICE_CONTROL requests
//
//	Arguments:
//		Pointer to our FDO
//		Pointer to the IRP
//			Buffered:	AssociatedIrp.SystemBuffer (and IrpStack->Parameters.DeviceIoControl.Type3InputBuffer)
//			Direct:		MdlAddress
//
//			IrpStack->Parameters.DeviceIoControl.InputBufferLength
//			IrpStack->Parameters.DeviceIoControl.OutputBufferLength 
//
//	Return Value:
//		This function returns STATUS_XXX

NTSTATUS Wdm1DeviceControl(IN PDEVICE_OBJECT fdo,
	IN PIRP Irp)
{
	PIO_STACK_LOCATION IrpStack = IoGetCurrentIrpStackLocation(Irp);
	NTSTATUS status = STATUS_SUCCESS;
	ULONG BytesTxd = 0;

	ULONG ControlCode = IrpStack->Parameters.DeviceIoControl.IoControlCode;
	ULONG InputLength = IrpStack->Parameters.DeviceIoControl.InputBufferLength;
	ULONG OutputLength = IrpStack->Parameters.DeviceIoControl.OutputBufferLength;

	DebugPrint("DeviceIoControl: Control code %x InputLength %d OutputLength %d",
		ControlCode, InputLength, OutputLength);

	// Get access to the shared buffer
	KIRQL irql;
	KeAcquireSpinLock(&BufferLock, &irql);
	switch (ControlCode)
	{
		///////	Zero Buffer
	case IOCTL_WDM1_ZERO_BUFFER:
		// Zero the buffer
		if (Buffer != NULL && BufferSize > 0)
			RtlZeroMemory(Buffer, BufferSize);
		break;

		///////	Remove Buffer
	case IOCTL_WDM1_REMOVE_BUFFER:
		if (Buffer != NULL)
		{
			ExFreePool(Buffer);
			Buffer = NULL;
			BufferSize = 0;
		}
		break;

		///////	Get Buffer Size as ULONG
	case IOCTL_WDM1_GET_BUFFER_SIZE:
		if (OutputLength < sizeof(ULONG))
			status = STATUS_INVALID_PARAMETER;
		else
		{
			BytesTxd = sizeof(ULONG);
			RtlCopyMemory(Irp->AssociatedIrp.SystemBuffer, &BufferSize, sizeof(ULONG));
		}
		break;

		///////	Get Buffer
	case IOCTL_WDM1_GET_BUFFER:
		if (OutputLength > BufferSize)
			status = STATUS_INVALID_PARAMETER;
		else
		{
			BytesTxd = OutputLength;
			RtlCopyMemory(Irp->AssociatedIrp.SystemBuffer, Buffer, BytesTxd);
		}
		break;

		///////	Get DateTime
	case IOCTL_WDM1_GET_BUILDTIME:
	{
		if (OutputLength < dateTimeSize){
			status = STATUS_INVALID_PARAMETER;
		}
		else {
			memset(dateTimeBuffer, 0, dateTimeSize);
			strcpy(dateTimeBuffer, __DATE__);
			strcat(dateTimeBuffer, " ");
			strcat(dateTimeBuffer, __TIME__);
			DebugPrint("DateTime: %s", dateTimeBuffer);
			BytesTxd = dateTimeSize;
			RtlCopyMemory(Irp->AssociatedIrp.SystemBuffer, dateTimeBuffer, dateTimeSize);
		}
	}
	break;

	case IOCTL_WDM1_READ_DATABIT:
	{		
		if (OutputLength < 1) 
		{
			status = STATUS_INVALID_PARAMETER;
		}
		else 
		{
			UCHAR dataBit = READ_PORT_UCHAR (SER_MSR(COM1_BASEADRESS));
			
			if (dataBit & 0x10)
			{
				dataBit = 1;
			}
			else
			{
				dataBit = 0;
			}
			
			BytesTxd = 1;
			RtlCopyMemory(Irp->AssociatedIrp.SystemBuffer, &dataBit, BytesTxd);			
		}
		
		
	}
	break;
	
	case IOCTL_WDM1_READ_CLOCKBIT:
	{
		if (OutputLength < 1) 
		{
			status = STATUS_INVALID_PARAMETER;
		}
		else 
		{
			UCHAR clockBit = READ_PORT_UCHAR (SER_MSR(COM1_BASEADRESS));
			
			if (clockBit & 0x20)
			{
				clockBit = 1;
			}
			else
			{
				clockBit = 0;
			}
			
			BytesTxd = 1;
			RtlCopyMemory(Irp->AssociatedIrp.SystemBuffer, &clockBit, BytesTxd);			
		}		
	}
	break;
	
	case IOCTL_WDM1_WRITE_DATABIT:
	{
		if (InputLength < 1)
		{
			status = STATUS_INVALID_PARAMETER;
		}
		else
		{
			UCHAR dataBit = 0;
			RtlCopyMemory(&dataBit, Irp->AssociatedIrp.SystemBuffer, 1);
			BytesTxd = 1;
			
			UCHAR byte = READ_PORT_UCHAR (SER_MCR(COM1_BASEADRESS));
			if (dataBit)
			{
				byte = byte | 0x01;
			}
			else
			{
				byte = byte & 0xFE;
			}			
			
			WRITE_PORT_UCHAR (SER_MCR(COM1_BASEADRESS), byte);
		}		
	}
	break;	

	case IOCTL_WDM1_WRITE_CLOCKBIT:
	{
		if (InputLength < 1)
		{
			status = STATUS_INVALID_PARAMETER;
		}
		else
		{
			UCHAR clockBit = 0;
			RtlCopyMemory(&clockBit, Irp->AssociatedIrp.SystemBuffer, 1);
			BytesTxd = 1;
		
			UCHAR byte = READ_PORT_UCHAR (SER_MCR(COM1_BASEADRESS));
			if (clockBit)
			{
				byte = byte | 0x02;
			}
			else
			{
				byte = byte & 0xFD;
			}			
		
			WRITE_PORT_UCHAR (SER_MCR(COM1_BASEADRESS), byte);
		}		
	}
	break;

		///////	Invalid request
	default:
		status = STATUS_INVALID_DEVICE_REQUEST;
	}
	// Release shared buffer
	KeReleaseSpinLock(&BufferLock, irql);

	DebugPrint("DeviceIoControl: %d bytes written", (int)BytesTxd);

	// Complete IRP
	return CompleteIrp(Irp, status, BytesTxd);
}

/////////////////////////////////////////////////////////////////////////////
//	Wdm1SystemControl:
//
//	Description:
//		Handle IRP_MJ_SYSTEM_CONTROL requests
//
//	Arguments:
//		Pointer to our FDO
//		Pointer to the IRP
//			Various minor parameters
//			IrpStack->Parameters.WMI.xxx has WMI parameters
//
//	Return Value:
//		This function returns STATUS_XXX

NTSTATUS Wdm1SystemControl(IN PDEVICE_OBJECT fdo,
	IN PIRP Irp)
{
	DebugPrintMsg("SystemControl");

	// Just pass to lower driver
	IoSkipCurrentIrpStackLocation(Irp);
	PWDM1_DEVICE_EXTENSION dx = (PWDM1_DEVICE_EXTENSION)fdo->DeviceExtension;
	return IoCallDriver(dx->NextStackDevice, Irp);
}

/////////////////////////////////////////////////////////////////////////////
//	Wdm1Cleanup:
//
//	Description:
//		Handle IRP_MJ_CLEANUP requests
//		Cancel queued IRPs which match given FileObject
//
//	Arguments:
//		Pointer to our FDO
//		Pointer to the IRP
//			IrpStack->FileObject has handle to file
//
//	Return Value:
//		This function returns STATUS_XXX

//	Not needed for Wdm1

/////////////////////////////////////////////////////////////////////////////
//	CompleteIrp:	Sets IoStatus and completes the IRP

NTSTATUS CompleteIrp(PIRP Irp, NTSTATUS status, ULONG info)
{
	Irp->IoStatus.Status = status;
	Irp->IoStatus.Information = info;
	IoCompleteRequest(Irp, IO_NO_INCREMENT);
	return status;
}

/////////////////////////////////////////////////////////////////////////////

