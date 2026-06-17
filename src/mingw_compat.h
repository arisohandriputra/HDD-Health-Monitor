/* ============================================================================
 *  HDDHealth Monitor - MinGW compatibility shims
 *  ---------------------------------------------------------------------------
 *  100% Free and Open Source Software (FOSS).
 *
 *  Author  : Ari Soandri Putra
 *  Company : ARImetic Inc.
 *  Sponsor : https://github.com/sponsors/arisohandriputra/
 *  License : MIT
 *
 *  Purpose:
 *    The official MinGW headers historically shipped without several SCSI
 *    pass-through definitions that are needed by smart.cpp when reading
 *    S.M.A.R.T. data from RAID / NVMe controllers.  This header:
 *      1. Pins the minimum Windows target version (Vista / 0x0600).
 *      2. Provides fallback definitions for IOCTL_SCSI_PASS_THROUGH* and
 *         the SCSI_PASS_THROUGH_DIRECT / SRB_IO_CONTROL structures if the
 *         MinGW headers have not already declared them.
 *
 *  The fallback is wrapped in #ifndef _NTDDSCSI_H_ so that the official
 *  headers take precedence when they are available (e.g. when building
 *  with the Windows SDK instead of MinGW).
 * ============================================================================
 */

#pragma once
#ifndef MINGW_COMPAT_H
#define MINGW_COMPAT_H

/* ---- Pin the minimum Windows target -------------------------------------- *
 * Vista (0x0600) is the lowest version that exposes the Storage Property
 * Query API used by smart.cpp for NVMe controller discovery. */
#ifndef WINVER
#  define WINVER         0x0600
#endif
#ifndef _WIN32_WINNT
#  define _WIN32_WINNT   0x0600
#endif
#ifndef _WIN32_IE
#  define _WIN32_IE      0x0600
#endif
#ifndef NTDDI_VERSION
#  define NTDDI_VERSION  0x06000000
#endif

#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#include <winioctl.h>

/* ---- SCSI pass-through fallback definitions ------------------------------ *
 * Only emitted if the active toolchain headers have not already done so. */
#ifndef _NTDDSCSI_H_
#define _NTDDSCSI_H_

#define IOCTL_SCSI_BASE                 FILE_DEVICE_CONTROLLER
#define IOCTL_SCSI_PASS_THROUGH         CTL_CODE(IOCTL_SCSI_BASE, 0x0401, METHOD_BUFFERED,   FILE_READ_ACCESS | FILE_WRITE_ACCESS)
#define IOCTL_SCSI_PASS_THROUGH_DIRECT  CTL_CODE(IOCTL_SCSI_BASE, 0x0405, METHOD_BUFFERED,   FILE_READ_ACCESS | FILE_WRITE_ACCESS)

/* Direction constants used by SCSI_IOCTL_DATA_IN in the pass-through
   structures below. */
#define SCSI_IOCTL_DATA_OUT          0
#define SCSI_IOCTL_DATA_IN           1
#define SCSI_IOCTL_DATA_UNSPECIFIED  2

#pragma pack(push, 1)
typedef struct _SCSI_PASS_THROUGH_DIRECT {
    USHORT  Length;
    UCHAR   ScsiStatus;
    UCHAR   PathId;
    UCHAR   TargetId;
    UCHAR   Lun;
    UCHAR   CdbLength;
    UCHAR   SenseInfoLength;
    UCHAR   DataIn;
    ULONG   DataTransferLength;
    ULONG   TimeOutValue;
    PVOID   DataBuffer;
    ULONG   SenseInfoOffset;
    UCHAR   Cdb[16];
} SCSI_PASS_THROUGH_DIRECT, *PSCSI_PASS_THROUGH_DIRECT;

/* SRB_IO_CONTROL is the envelope used by RAID controller miniport
   drivers (e.g. Intel RST, AMD RAIDXpert) for vendor-specific IOCTLs. */
typedef struct _SRB_IO_CONTROL {
    ULONG  HeaderLength;
    UCHAR  Signature[8];
    ULONG  Timeout;
    ULONG  ControlCode;
    ULONG  ReturnCode;
    ULONG  Length;
} SRB_IO_CONTROL, *PSRB_IO_CONTROL;
#pragma pack(pop)

#endif /* _NTDDSCSI_H_ */

#endif /* MINGW_COMPAT_H */
