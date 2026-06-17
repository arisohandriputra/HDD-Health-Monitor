/* ============================================================================
 *  HDDHealth Monitor - Resource identifier definitions
 *  ---------------------------------------------------------------------------
 *  100% Free and Open Source Software (FOSS).
 *
 *  Author  : Ari Soandri Putra
 *  Company : ARImetic Inc.
 *  Sponsor : https://github.com/sponsors/arisohandriputra/
 *  License : MIT
 *
 *  This header assigns numeric IDs to the resources embedded in app.rc:
 *    - IDI_APPICON : main application icon
 *    - IDI_HDD     : drive / tray icon (currently reuses IDI_APPICON)
 *    - IDR_MAINMENU: (reserved, currently unused - menu is built at runtime)
 *    - IDR_VERSION : resource ID for the VS_VERSION_INFO block
 * ============================================================================
 */

#pragma once
#ifndef RESOURCE_H
#define RESOURCE_H

#define IDI_APPICON     101
#define IDI_HDD         102
#define IDR_MAINMENU    103

#define IDR_VERSION     1

#endif
