//////////////////////////////////////////////////////////////////////////////
//	Copyright � 1998 Chris Cant, PHD Computer Consultants Ltd
//	WDM Book for R&D Books, Miller Freeman Inc
//
//	Wdm3 example
/////////////////////////////////////////////////////////////////////////////
//	Wdm3Msg.mc:		Message Definition file
/////////////////////////////////////////////////////////////////////////////
//	Version history
//	15-Dec-98	1.0.0	CC	creation
/////////////////////////////////////////////////////////////////////////////
//
//  Values are 32 bit values layed out as follows:
//
//   3 3 2 2 2 2 2 2 2 2 2 2 1 1 1 1 1 1 1 1 1 1
//   1 0 9 8 7 6 5 4 3 2 1 0 9 8 7 6 5 4 3 2 1 0 9 8 7 6 5 4 3 2 1 0
//  +---+-+-+-----------------------+-------------------------------+
//  |Sev|C|R|     Facility          |               Code            |
//  +---+-+-+-----------------------+-------------------------------+
//
//  where
//
//      Sev - is the severity code
//
//          00 - Success
//          01 - Informational
//          10 - Warning
//          11 - Error
//
//      C - is the Customer code flag
//
//      R - is a reserved bit
//
//      Facility - is the facility code
//
//      Code - is the facility's status code
//
//
// Define the facility codes
//
#define FACILITY_WDM3_ERROR_CODE         0x7


//
// Define the severity codes
//
#define STATUS_SEVERITY_WARNING          0x2
#define STATUS_SEVERITY_SUCCESS          0x0
#define STATUS_SEVERITY_INFORMATIONAL    0x1
#define STATUS_SEVERITY_ERROR            0x3


//
// MessageId: WDM3_MSG_LOGGING_STARTED
//
// MessageText:
//
//  Event logging enabled for Wdm3 Driver.
//
#define WDM3_MSG_LOGGING_STARTED         ((NTSTATUS)0x60070001L)

//
// MessageId: WDM3_MESSAGE
//
// MessageText:
//
//  Message: %2.
//
#define WDM3_MESSAGE                     ((NTSTATUS)0x60070002L)

