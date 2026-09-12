/* ============================================================================
 *  HDDHealth Monitor 1.3 - Drive database (JSON-backed spec lookup)
 *
 *  Author  : Ari Sohandri Putra
 *  Sponsor : https://github.com/sponsors/arisohandriputra/
 *  License : MIT
 *
 *  Reads drive specs from a bundled JSON file (drives.json) that ships
 *  next to HDDHealth.exe. The JSON is maintained by the developer —
 *  end users see the info read-only in a dialog.
 *
 *  Lookup is done by model name (+ optional firmware match).
 *
 *  JSON file location (searched in this order):
 *    1. <exe folder>\drives.json     (bundled with the app)
 *    2. <Documents>\HDDH_Database\drives.json  (dev fallback)
 *
 *  See drives.json for the expected format.
 * ============================================================================
 */

#pragma once
#ifndef DRIVE_DB_H
#define DRIVE_DB_H

#define WIN32_LEAN_AND_MEAN
#include <windows.h>

#ifdef __cplusplus
extern "C" {
#endif

/* The database entry — one per drive model.
 * All fields are strings so the JSON can hold whatever spec data we have.
 * Empty string = field not present in JSON. */
typedef struct _DRIVE_DB_ENTRY {
    char szModel[128];          /* match key (from JSON) */
    char szFirmware[32];        /* optional match key */

    /* --- Identification --- */
    char szTrademark[64];
    char szCapacity[64];
    char szFormFactor[16];
    char szType[16];
    char szInterface[16];
    char szSataVersion[16];
    char szController[64];
    char szAltFirmwares[16];
    char szFirmwareUpgrade[32];
    char szAtaVersion[32];
    char szAdvancedFormat[32];
    char szUnitsTested[16];
    char szTimesTested[16];

    /* --- Performance --- */
    char szMaxInterfaceSpeed[32];
    char szMaxBufferedRead[32];
    char szMaxReadSpeed[32];
    char szMinAccessTime[32];
    char szCacheBuffer[32];
    char szVolatileWriteCache[32];
    char szReadLookAhead[32];
    char szNcq[32];
    char szApm[32];
    char szAam[32];
    char szTrim[32];
    char szRotationRate[32];

    /* --- Reliability --- */
    char szSensorCount[16];
    char szFreeFallControl[32];
    char szSct[32];
    char szFullSelftestTime[32];
    char szCritMaxTemp[32];
    char szRecMaxTemp[32];
    char szMaxTemp[32];
    char szAvgMaxTemp[32];

    /* --- Security --- */
    char szDriveAccessRestriction[32];
    char szEnhancedSecurityErase[32];
    char szDataDestructionTime[32];
    char szInstantDataDestruction[32];
    char szHpa[32];
    char szHpaPassword[32];
    char szTrustedComputing[32];

    BOOL bFound;  /* TRUE if a match was found in the JSON */
} DRIVE_DB_ENTRY;

/* Look up a drive by model name (+ optional firmware).
 * Returns TRUE if found, fills pEntry. Returns FALSE if not found
 * or if the JSON file doesn't exist (pEntry is zeroed, bFound=FALSE). */
BOOL DriveDB_Lookup(const char* szModel, const char* szFirmware,
                    DRIVE_DB_ENTRY* pEntry);

/* Shows the database info dialog for the currently selected drive.
 * If no match found, shows a "not in database" message. */
void DriveDB_ShowDialog(HWND hWnd);

#ifdef __cplusplus
}
#endif

#endif /* DRIVE_DB_H */
