/* ============================================================================
 *  HDDHealth Monitor - S.M.A.R.T. public interface
 *  ---------------------------------------------------------------------------
 *  100% Free and Open Source Software (FOSS).
 *
 *  Author  : Ari Sohandri Putra
 *  Company : ARImetic Inc.
 *  Sponsor : https://github.com/sponsors/arisohandriputra/
 *  License : MIT
 *
 *  This header exposes the public surface of smart.cpp:
 *    - DRIVE_INFO structure (one per detected physical drive)
 *    - SMART_ATTRIBUTE structure (one per attribute, up to 30 per drive)
 *    - ScanDrives() enumeration entry point
 *
 *  IOCTL / ATA / NVMe constants that some MinGW headers omit are also
 *  defined here for convenience.
 * ============================================================================
 */

#pragma once
#ifndef SMART_H
#define SMART_H

#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#include <winioctl.h>

#ifdef __cplusplus
extern "C" {
#endif

/* ============================================================
 * IOCTL definitions
 * ============================================================ */
#ifndef SMART_GET_VERSION
#define SMART_GET_VERSION           CTL_CODE(IOCTL_DISK_BASE, 0x0020, METHOD_BUFFERED, FILE_READ_ACCESS)
#endif
#ifndef SMART_SEND_DRIVE_COMMAND
#define SMART_SEND_DRIVE_COMMAND    CTL_CODE(IOCTL_DISK_BASE, 0x0021, METHOD_BUFFERED, FILE_READ_ACCESS | FILE_WRITE_ACCESS)
#endif
#ifndef SMART_RCV_DRIVE_DATA
#define SMART_RCV_DRIVE_DATA        CTL_CODE(IOCTL_DISK_BASE, 0x0022, METHOD_BUFFERED, FILE_READ_ACCESS | FILE_WRITE_ACCESS)
#endif

#ifndef IOCTL_ATA_PASS_THROUGH
#define IOCTL_ATA_PASS_THROUGH      CTL_CODE(IOCTL_SCSI_BASE, 0x040b, METHOD_BUFFERED, FILE_READ_ACCESS | FILE_WRITE_ACCESS)
#endif
#ifndef IOCTL_ATA_PASS_THROUGH_DIRECT
#define IOCTL_ATA_PASS_THROUGH_DIRECT CTL_CODE(IOCTL_SCSI_BASE, 0x040c, METHOD_BUFFERED, FILE_READ_ACCESS | FILE_WRITE_ACCESS)
#endif

/* ATA commands */
#define ATAPI_ID_CMD                0xA1
#define ID_CMD                      0xEC
#define SMART_CMD                   0xB0
#define SMART_CYL_LOW               0x4F
#define SMART_CYL_HI                0xC2
#define SMART_READ_DATA             0xD0
#define SMART_READ_THRESHOLDS       0xD1
#define SMART_READ_LOG              0xD5
#define SMART_ENABLE                0xD8
#define SMART_DISABLE               0xD9
#define SMART_RETURN_STATUS         0xDA

#define READ_ATTRIBUTE_BUFFER_SIZE  512
#define IDENTIFY_BUFFER_SIZE        512
#define READ_THRESHOLD_BUFFER_SIZE  512

/* SMART Log addresses */
#define SMART_LOG_SUMMARY           0x01
#define SMART_LOG_COMP_ERROR        0x02
#define SMART_LOG_COMP_SELF_TEST    0x03
#define SMART_LOG_SELECTIVE_SELF_TEST 0x04
#define SMART_LOG_EXTENDED_COMP_ERROR 0x03  /* ATA8+ extended error log */

/* SAT (SCSI/ATA Translation) */
#define SAT_ATA_PASSTHROUGH_12      0xA1
#define SAT_ATA_PASSTHROUGH_16      0x85

#define SAT_PROTO_HARD_RESET        (0 << 1)
#define SAT_PROTO_SRST              (1 << 1)
#define SAT_PROTO_NON_DATA          (3 << 1)
#define SAT_PROTO_PIO_IN            (4 << 1)
#define SAT_PROTO_PIO_OUT           (5 << 1)
#define SAT_PROTO_DMA               (6 << 1)
#define SAT_PROTO_DMA_QUEUED        (7 << 1)
#define SAT_PROTO_DEV_DIAGNOSTIC    (8 << 1)
#define SAT_PROTO_DEV_RESET         (9 << 1)
#define SAT_PROTO_UDMA_IN           (10 << 1)
#define SAT_PROTO_UDMA_OUT          (11 << 1)

#define SAT_FLAGS_CK_COND           0x20
#define SAT_FLAGS_TDIR_FROM_DEV     0x08
#define SAT_FLAGS_BYTE_BLOCK        0x04
#define SAT_FLAGS_TLEN_SECTOR_CNT   0x02
#define SAT_FLAGS_TLEN_BYTES        0x01

/* USB bridge chip identification */
typedef enum _USB_BRIDGE_TYPE {
    USB_BRIDGE_UNKNOWN     = 0,
    USB_BRIDGE_SAT         = 1,   /* Standard SAT (A1h/85h) */
    USB_BRIDGE_JMICRON     = 2,   /* JMicron (152D:0578, 152D:0565, etc.) */
    USB_BRIDGE_SUNPLUS     = 3,   /* Sunplus (04FC:0C25, etc.) */
    USB_BRIDGE_CYPRESS     = 4,   /* Cypress/Iodata (04B4:6830, etc.) */
    USB_BRIDGE_IO_DATA     = 5,   /* I-O Data */
    USB_BRIDGE_LOGITEC     = 6,   /* Logitec */
    USB_BRIDGE_PROLIFIC    = 7,   /* Prolific (067B:2773, etc.) */
    USB_BRIDGE_NVME_JMICRON = 8,  /* JMicron NVMe (152D:0583, 152D:0586, etc.) */
    USB_BRIDGE_NVME_ASMEDIA = 9,  /* ASMedia NVMe (174C:2362, etc.) */
    USB_BRIDGE_NVME_REALTEK = 10, /* Realtek NVMe (0BDA:9210, etc.) */
    USB_BRIDGE_ASM1352R    = 11,  /* ASMedia ASM1352R RAID (174C:1352) */
    USB_BRIDGE_NVME_VLI    = 12,  /* VLI VL716/VL717 NVMe (2109:0900, etc.) */
    USB_BRIDGE_NVME_FMA    = 13   /* FMA NL6221 NVMe (0BDA:9220, etc.) */
} USB_BRIDGE_TYPE;

/* NVMe critical warning bits */
#define NVME_CRIT_WARN_SPARE_BELOW_THRESH   0x01
#define NVME_CRIT_WARN_TEMP_THRESHOLD       0x02
#define NVME_CRIT_WARN_RELIABILITY_DEGRADED 0x04
#define NVME_CRIT_WARN_READ_ONLY            0x08
#define NVME_CRIT_WARN_VOLATILE_MEM_BACKUP  0x10
#define NVME_CRIT_WARN_PMR_RO               0x20

/* ============================================================
 * Drive health status
 * ============================================================ */
typedef enum _DRIVE_HEALTH_STATUS {
    HEALTH_STATUS_UNKNOWN  = 0,
    HEALTH_STATUS_GOOD     = 1,   /* All attributes within thresholds */
    HEALTH_STATUS_CAUTION  = 2,   /* One or more attributes near threshold or raw > 0 for critical */
    HEALTH_STATUS_BAD      = 3,   /* Attribute at or below threshold, or NVMe critical warning */
    HEALTH_STATUS_WARNING  = 4    /* SMART predictive failure returned */
} DRIVE_HEALTH_STATUS;

/* ============================================================
 * Drive vendor identification
 * ============================================================ */
typedef enum _DRIVE_VENDOR {
    VENDOR_UNKNOWN     = 0,
    VENDOR_SAMSUNG     = 1,
    VENDOR_WDC         = 2,   /* Western Digital */
    VENDOR_SEAGATE     = 3,
    VENDOR_TOSHIBA     = 4,
    VENDOR_HITACHI     = 5,   /* HGST */
    VENDOR_INTEL       = 6,
    VENDOR_MICRON      = 7,   /* Crucial */
    VENDOR_KINGSTON    = 8,
    VENDOR_SANDISK     = 9,
    VENDOR_SKHYNIX     = 10,
    VENDOR_KIOXIA      = 11,  /* Formerly Toshiba Memory */
    VENDOR_ADATA       = 12,
    VENDOR_PNY         = 13,
    VENDOR_CORSAIR     = 14,
    VENDOR_LEXAR       = 15,
    VENDOR_SILICON_POWER = 16,
    VENDOR_TEAMGROUP   = 17,
    VENDOR_GOODRAM     = 18,
    VENDOR_PLEXTOR     = 19,
    VENDOR_OCZ         = 20,
    VENDOR_OTHER       = 99
} DRIVE_VENDOR;

/* ============================================================
 * Drive types
 * ============================================================ */
typedef enum _DRIVE_TYPE {
    DRIVE_TYPE_UNKNOWN  = 0,
    DRIVE_TYPE_HDD      = 1,
    DRIVE_TYPE_SSD_SATA = 2,
    DRIVE_TYPE_NVME     = 3,
    DRIVE_TYPE_USB      = 4,
    DRIVE_TYPE_M2_SATA  = 5,
    DRIVE_TYPE_EMMC     = 6,
    DRIVE_TYPE_SD       = 7,
    DRIVE_TYPE_SCSI     = 8
} DRIVE_TYPE;

/* Access method used to read SMART/health data */
typedef enum _SMART_ACCESS_METHOD {
    SMART_ACCESS_NONE              = 0,
    SMART_ACCESS_LEGACY_IOCTL      = 1,  /* SMART_RCV_DRIVE_DATA */
    SMART_ACCESS_ATA_PASSTHROUGH   = 2,  /* IOCTL_ATA_PASS_THROUGH(_DIRECT) */
    SMART_ACCESS_SAT12             = 3,  /* SCSI A1h */
    SMART_ACCESS_SAT16             = 4,  /* SCSI 85h */
    SMART_ACCESS_STORAGE_QUERY     = 5,  /* IOCTL_STORAGE_QUERY_PROPERTY */
    SMART_ACCESS_NVME_PROTOCOL     = 6,  /* StorageDeviceProtocolSpecificProperty */
    SMART_ACCESS_NVME_PASSTHROUGH  = 7   /* IOCTL_STORAGE_PROTOCOL_COMMAND */
} SMART_ACCESS_METHOD;

#pragma pack(push, 1)

/* ============================================================
 * NVMe Health Log (Log Page 02h) - 512 bytes
 * ============================================================ */
typedef struct _NVME_HEALTH_INFO_LOG {
    BYTE    CriticalWarning;
    BYTE    CompositeTemperature[2];
    BYTE    AvailableSpare;
    BYTE    AvailableSpareThreshold;
    BYTE    PercentageUsed;
    BYTE    EnduranceGroupSummary;
    BYTE    Reserved7[25];
    BYTE    DataUnitsRead[16];
    BYTE    DataUnitsWritten[16];
    BYTE    HostReadCommands[16];
    BYTE    HostWriteCommands[16];
    BYTE    ControllerBusyTime[16];
    BYTE    PowerCycles[16];
    BYTE    PowerOnHours[16];
    BYTE    UnsafeShutdowns[16];
    BYTE    MediaErrors[16];
    BYTE    NumErrLogEntries[16];
    ULONG   WarningCompTempTime;
    ULONG   CriticalCompTempTime;
    USHORT  TempSensor[8];
    ULONG   ThermalMgmtTemp1TransCnt;
    ULONG   ThermalMgmtTemp2TransCnt;
    ULONG   TotalTimeThermalMgmtTemp1;
    ULONG   TotalTimeThermalMgmtTemp2;
    BYTE    Reserved232[280];
} NVME_HEALTH_INFO_LOG;

/* ============================================================
 * NVMe Identify Controller (partial - key fields)
 * ============================================================ */
typedef struct _NVME_IDENTIFY_CONTROLLER {
    BYTE    Reserved0[4];
    BYTE    SerialNumber[20];
    BYTE    ModelNumber[40];
    BYTE    FirmwareRevision[8];
    BYTE    RecommendedArbitrationBurst;
    BYTE    Reserved73[3];
    BYTE    OACS[2];           /* Optional Admin Command Support */
    BYTE    ACL;               /* Abort Command Limit */
    BYTE    AERL;              /* Asynchronous Event Request Limit */
    BYTE    FRMW;              /* Firmware Updates */
    BYTE    LPA;               /* Log Page Attributes */
    BYTE    ELPE;              /* Error Log Page Entries */
    BYTE    NPSS;              /* Number of Power States Support */
    BYTE    AVSCC;             /* Admin Vendor Specific Command Configuration */
    BYTE    APSTA;             /* Autonomous Power State Transition Attributes */
    BYTE    WCTEMP[2];        /* Warning Composite Temperature Threshold */
    BYTE    CCTEMP[2];        /* Critical Composite Temperature Threshold */
    BYTE    Reserved100[80];
    BYTE    Reserved180[16];
    BYTE    SQES;              /* Submission Queue Entry Size */
    BYTE    CQES;              /* Completion Queue Entry Size */
    BYTE    Reserved198[2];
    BYTE    NN[4];             /* Number of Namespaces */
    BYTE    ONCS[2];          /* Optional NVM Command Support */
    BYTE    FUSES[2];         /* Fused Operation Support */
    BYTE    FNA;               /* Format NVM Attributes */
    BYTE    VWC;               /* Volatile Write Cache */
    BYTE    AWUN[2];          /* Atomic Write Unit Normal */
    BYTE    AWUPF[2];         /* Atomic Write Unit Power Fail */
    BYTE    NVSCC;             /* NVM Vendor Specific Command Configuration */
    BYTE    Reserved223;
    BYTE    ACWU[2];          /* Atomic Compare & Write Unit */
    BYTE    Reserved226[2];
    BYTE    SGLS[4];          /* SGL Support */
    BYTE    Reserved232[224];
    BYTE    Subnqn[256];
    BYTE    Reserved768[3072];
    BYTE    VS[1024];         /* Vendor Specific */
} NVME_IDENTIFY_CONTROLLER;

/* ============================================================
 * Legacy ATA SMART structures
 * ============================================================ */
typedef struct _SMART_ATTRIBUTE {
    BYTE  bAttrID;
    WORD  wStatusFlags;
    BYTE  bAttrValue;
    BYTE  bWorstValue;
    BYTE  bRawValue[6];
    BYTE  bReserved;
} SMART_ATTRIBUTE;

typedef struct _SMART_THRESHOLD {
    BYTE  bAttrID;
    BYTE  bThresholdValue;
    BYTE  bReserved[10];
} SMART_THRESHOLD;

typedef struct _SMART_ATTRIBUTE_DATA {
    WORD            wRevisionNumber;
    SMART_ATTRIBUTE stAttributes[30];
    BYTE            bReserved[150];
} SMART_ATTRIBUTE_DATA;

typedef struct _SMART_THRESHOLD_DATA {
    WORD            wRevisionNumber;
    SMART_THRESHOLD stThresholds[30];
    BYTE            bReserved[149];
} SMART_THRESHOLD_DATA;

/* ============================================================
 * SMART Error Log structures (ATA8-ACS)
 * ============================================================ */
typedef struct _SMART_ERROR_DESCRIPTOR {
    BYTE   bErrorRegister;
    BYTE   bSectorCount;
    BYTE   bLBA_Low;
    BYTE   bLBA_Mid;
    BYTE   bLBA_High;
    BYTE   bDevice;
    BYTE   bStatus;
    BYTE   bExtendedError[7]; /* Extended info for 48-bit LBA */
    BYTE   bState;            /* Error log entry state */
    BYTE   bTimestamp[2];     /* Time since power-up in milliseconds (overflow) */
} SMART_ERROR_DESCRIPTOR;

typedef struct _SMART_ERROR_LOG {
    BYTE               bRevisionNumber[2];
    BYTE               bErrorLogIndex;
    BYTE               bReserved1;
    SMART_ERROR_DESCRIPTOR stErrors[5]; /* Last 5 errors */
    BYTE               bReserved2[371];
    BYTE               bChecksum;
} SMART_ERROR_LOG;

/* ============================================================
 * SMART Self-Test Log structures
 * ============================================================ */
typedef struct _SMART_SELF_TEST_DESCRIPTOR {
    BYTE   bLBA_Low;
    BYTE   bLBA_Mid;
    BYTE   bLBA_High;
    BYTE   bStatusByte;     /* Self-test status */
    BYTE   bLifeTimestamp;   /* Power-on hours at start */
    BYTE   bTimestamp[2];    /* Checkpoint */
    BYTE   bFailingLBA[4];  /* LBA of first failure */
} SMART_SELF_TEST_DESCRIPTOR;

typedef struct _SMART_SELF_TEST_LOG {
    BYTE                       bRevisionNumber[2];
    BYTE                       bSelfTestIndex;
    BYTE                       bReserved1;
    SMART_SELF_TEST_DESCRIPTOR stEntries[21];
    BYTE                       bReserved2[116];
    BYTE                       bChecksum;
} SMART_SELF_TEST_LOG;

#pragma pack(pop)

/* ============================================================
 * Main drive info structure
 * ============================================================ */
typedef struct _DRIVE_INFO {
    char        szModel[41];
    char        szSerial[21];
    char        szFirmware[9];
    DWORD       dwCapacityMB;
    BOOL        bSMART_Supported;
    BOOL        bSMART_Enabled;
    int         nHealthPercent;
    int         nPerformancePercent;
    BOOL        bPredictFailure;
    int         nDriveIndex;
    BOOL        bIsUSB;
    DRIVE_TYPE  eType;
    int         nTemperatureC;
    BOOL        bIsNVMe;
    int         nReadSpeedMBs;
    int         nWriteSpeedMBs;
    SMART_ACCESS_METHOD eAccessMethod;
    DWORD       dwPowerOnHours;
    DWORD       dwPowerCycleCount;
    NVME_HEALTH_INFO_LOG nvmeHealth;
    SMART_ATTRIBUTE_DATA attrData;
    SMART_THRESHOLD_DATA threshData;

    /* USB bridge identification (from SCSI INQUIRY) */
    char        szBridgeVendor[9];   /* T10 vendor ID, 8 chars */
    char        szBridgeProduct[17]; /* T10 product ID, 16 chars */
    WORD        wUsbVid;
    WORD        wUsbPid;
    USB_BRIDGE_TYPE eUsbBridgeType;  /* Detected USB bridge chip type */

    /* Diagnostics: last GetLastError() per probe path */
    DWORD       dwErrNvmeProtocol;
    DWORD       dwErrNvmePassthrough;
    DWORD       dwErrSat16;
    DWORD       dwErrSat12;
    DWORD       dwErrStorageProtocol;
    DWORD       dwErrLogSense;

    /* ---- New fields  ---- */
    DRIVE_HEALTH_STATUS eHealthStatus;   /* Good / Caution / Bad */
    DRIVE_VENDOR eVendor;                /* Detected drive vendor */

    /* NVMe extended info */
    NVME_IDENTIFY_CONTROLLER nvmeIdent;
    BOOL        bGotNVMeIdent;
    USHORT      wNVMeWarnTempThreshold;  /* WCTEMP from Identify */
    USHORT      wNVMeCritTempThreshold;  /* CCTEMP from Identify */
    unsigned __int64 qwNVMeDataUnitsRead;
    unsigned __int64 qwNVMeDataUnitsWritten;
    unsigned __int64 qwNVMeMediaErrors;
    unsigned __int64 qwNVMeUnsafeShutdowns;
    unsigned __int64 qwNVMeHostReads;
    unsigned __int64 qwNVMeHostWrites;
    unsigned __int64 qwNVMeControllerBusyTime;
    DWORD       dwNVMePowerOnHours64;    /* Full 64-bit (low 32 bits) */
    unsigned __int64 qwNVMePowerOnHours; /* Full 64-bit power-on hours */

    /* SMART Error Log */
    BOOL        bGotErrorLog;
    int         nErrorLogCount;          /* Number of entries in error log */
    SMART_ERROR_LOG errorLog;

    /* SMART Self-Test Log */
    BOOL        bGotSelfTestLog;
    int         nSelfTestStatus;         /* Current self-test status code */
    SMART_SELF_TEST_LOG selfTestLog;

    /* SSD-specific life indicators */
    int         nSSDLifeLeft;            /* 0-100, -1 if N/A */
    int         nSSDTotalWritesGB;       /* Total NAND writes in GB, -1 if N/A */
    int         nSSDAvgEraseCount;       /* Average erase count, -1 if N/A */
    int         nSSDMaxEraseCount;       /* Max erase count, -1 if N/A */
    int         nSSDMinEraseCount;       /* Min erase count, -1 if N/A */
    int         nSSDWearLevelingCount;   /* Wear leveling count, -1 if N/A */

    /* ATA IDENTIFY word 217 rotation rate */
    WORD        wRotationRate;           /* 0x0001=SSD, >=0x0401=HDD RPM */

    /* Temperature sensors */
    int         nTempSensor[8];          /* NVMe temp sensors 1-8, -1 if N/A */
} DRIVE_INFO;

/* ============================================================
 * Attribute classification
 * ============================================================ */

/* Attribute criticality levels*/
typedef enum _ATTR_CRITICALITY {
    ATTR_CRIT_NONE      = 0,   /* Informational only */
    ATTR_CRIT_ADVISORY  = 1,   /* Advisory: raw > 0 means Caution */
    ATTR_CRIT_CRITICAL  = 2,   /* Critical: raw > 0 means Bad or threshold exceeded */
    ATTR_CRIT_PERFORMANCE = 3  /* Performance-affecting */
} ATTR_CRITICALITY;

/* Attribute data interpretation type */
typedef enum _ATTR_INTERP {
    INTERP_NORMAL      = 0,   /* Standard: value/worst/threshold/raw */
    INTERP_LIFE_PCT    = 1,   /* SSD life percentage (value = remaining %) */
    INTERP_TEMPERATURE = 2,   /* Temperature in Celsius (raw[0] or value) */
    INTERP_COUNTER32   = 3,   /* 32-bit monotonic counter */
    INTERP_COUNTER48   = 4,   /* 48-bit monotonic counter */
    INTERP_RATE        = 5,   /* Error rate (raw = number of errors) */
    INTERP_DURATION    = 6,   /* Duration in ms (spin-up time etc.) */
    INTERP_EVENT_COUNT = 7    /* Event counter (raw = total events) */
} ATTR_INTERP;

typedef struct _ATTR_NAME {
    BYTE            bID;
    const char*     szName;
    ATTR_CRITICALITY eCritLevel;
    ATTR_INTERP     eInterp;
} ATTR_NAME;

extern const ATTR_NAME g_AttrNames[];

/* ============================================================
 * Public API
 * ============================================================ */
const char* GetAttrName(BYTE bID);
ATTR_CRITICALITY GetAttrCriticality(BYTE bID);
ATTR_INTERP GetAttrInterpretation(BYTE bID);
BOOL        IsAttrCritical(BYTE bID);
const char* GetDriveTypeName(DRIVE_TYPE eType);
const char* GetAccessMethodName(SMART_ACCESS_METHOD eMethod);
const char* GetHealthStatusName(DRIVE_HEALTH_STATUS eStatus);
const char* GetVendorName(DRIVE_VENDOR eVendor);
DRIVE_VENDOR DetectDriveVendor(const char* szModel);

BOOL  OpenDrive(int nDrive, HANDLE* phDrive);
BOOL  OpenDriveReadOnly(int nDrive, HANDLE* phDrive);
BYTE  GetStorageBusType(HANDLE hDrive);
BOOL  IsUSBDrive(HANDLE hDrive);
BOOL  IsNVMeDrive(HANDLE hDrive);
BOOL  IsEMMCDrive(HANDLE hDrive);

/* USB bridge identification */
BOOL  GetBridgeIdentity(HANDLE hDrive, DRIVE_INFO* pInfo);
BOOL  GetUSBVidPid(HANDLE hDrive, WORD* pwVid, WORD* pwPid);
USB_BRIDGE_TYPE DetectUsbBridgeType(HANDLE hDrive, DRIVE_INFO* pInfo);

/* SCSI LOG SENSE */
BOOL  GetSMARTViaLogSense(HANDLE hDrive, DRIVE_INFO* pInfo);

/* Identify (multiple paths) */
BOOL  GetIdentifyData(HANDLE hDrive, int nDrive, DRIVE_INFO* pInfo);
BOOL  GetIdentifyDataATAPassthrough(HANDLE hDrive, DRIVE_INFO* pInfo);
BOOL  GetIdentifyDataSAT(HANDLE hDrive, DRIVE_INFO* pInfo);
BOOL  GetIdentifyDataUSB(HANDLE hDrive, DRIVE_INFO* pInfo);
BOOL  GetDeviceDescriptor(HANDLE hDrive, DRIVE_INFO* pInfo);

/* SMART data (multiple paths) */
BOOL  EnableSMART(HANDLE hDrive, int nDrive);
BOOL  EnableSMARTSAT(HANDLE hDrive);
BOOL  GetSMARTAttributes(HANDLE hDrive, int nDrive, DRIVE_INFO* pInfo);
BOOL  GetSMARTThresholds(HANDLE hDrive, int nDrive, DRIVE_INFO* pInfo);
BOOL  GetSMARTAttributesATAPassthrough(HANDLE hDrive, DRIVE_INFO* pInfo);
BOOL  GetSMARTThresholdsATAPassthrough(HANDLE hDrive, DRIVE_INFO* pInfo);
BOOL  GetSMARTAttributesSAT(HANDLE hDrive, DRIVE_INFO* pInfo);
BOOL  GetSMARTThresholdsSAT(HANDLE hDrive, DRIVE_INFO* pInfo);
BOOL  GetSMARTViaStorageProtocol(HANDLE hDrive, DRIVE_INFO* pInfo);
BOOL  GetSMARTPredictFailure(HANDLE hDrive, int nDrive, BOOL* pbFail);

/* SMART Log reading */
BOOL  GetSMARTErrorLog(HANDLE hDrive, int nDrive, DRIVE_INFO* pInfo);
BOOL  GetSMARTErrorLogATAPassthrough(HANDLE hDrive, DRIVE_INFO* pInfo);
BOOL  GetSMARTErrorLogSAT(HANDLE hDrive, DRIVE_INFO* pInfo);
BOOL  GetSMARTSelfTestLog(HANDLE hDrive, int nDrive, DRIVE_INFO* pInfo);
BOOL  GetSMARTSelfTestLogATAPassthrough(HANDLE hDrive, DRIVE_INFO* pInfo);
BOOL  GetSMARTSelfTestLogSAT(HANDLE hDrive, DRIVE_INFO* pInfo);

/* NVMe */
BOOL  GetNVMeIdentifyController(HANDLE hDrive, DRIVE_INFO* pInfo);
BOOL  GetNVMeHealthLog(HANDLE hDrive, DRIVE_INFO* pInfo);
BOOL  GetNVMeHealthLogFallback(HANDLE hDrive, DRIVE_INFO* pInfo);
BOOL  GetNVMeHealthLogPassthrough(HANDLE hDrive, DRIVE_INFO* pInfo);
BOOL  GetNVMeHealthLogEx(HANDLE hDrive, DRIVE_INFO* pInfo);

/* Detection & calculation */
DRIVE_TYPE DetectDriveType(HANDLE hDrive, DRIVE_INFO* pInfo);
int   CalculateHealth(DRIVE_INFO* pInfo);
int   CalculateHealthNVMe(DRIVE_INFO* pInfo);
DRIVE_HEALTH_STATUS DetermineHealthStatus(DRIVE_INFO* pInfo);
int   CalculatePerformance(DRIVE_INFO* pInfo);

/* SSD-specific analysis */
void  ExtractSSDIndicators(DRIVE_INFO* pInfo);

/* Aggregate scan */
int   ScanDrives(DRIVE_INFO* pDrives, int nMaxDrives);
BOOL  RefreshDriveSmart(int nDriveIndex, DRIVE_INFO* pInfo);

/* Utilities */
void  FormatSize(DWORD dwMB, char* szBuf, int nBufLen);
DWORD GetRawValue(BYTE* pRaw);
unsigned __int64 GetRawValue48(BYTE* pRaw);
int   MeasureReadSpeed(int nDriveIndex);
int   MeasureWriteSpeed(int nDriveIndex);

#ifdef __cplusplus
}
#endif

#endif /* SMART_H */
