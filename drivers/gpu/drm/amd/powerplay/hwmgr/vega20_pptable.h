/*
 * Copyright 2018 Advanced Micro Devices, Inc.
 *
 * Permission is hereby granted, free of charge, to any person obtaining a
 * copy of this software and associated documentation files (the "Software"),
 * to deal in the Software without restriction, including without limitation
 * the rights to use, copy, modify, merge, publish, distribute, sublicense,
 * and/or sell copies of the Software, and to permit persons to whom the
 * Software is furnished to do so, subject to the following conditions:
 *
 * The above copyright notice and this permission notice shall be included in
 * all copies or substantial portions of the Software.
 *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
 * IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
 * FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT.  IN NO EVENT SHALL
 * THE COPYRIGHT HOLDER(S) OR AUTHOR(S) BE LIABLE FOR ANY CLAIM, DAMAGES OR
 * OTHER LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE,
 * ARISING FROM, OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR
 * OTHER DEALINGS IN THE SOFTWARE.
 *
 */
#ifndef _VEGA20_PPTABLE_H_
#define _VEGA20_PPTABLE_H_

#pragma pack(push, 1)

#define ATOM_VEGA20_PP_THERMALCONTROLLER_NONE           0
#define ATOM_VEGA20_PP_THERMALCONTROLLER_VEGA20     26

#define ATOM_VEGA20_PP_PLATFORM_CAP_POWERPLAY                   0x1
#define ATOM_VEGA20_PP_PLATFORM_CAP_SBIOSPOWERSOURCE            0x2
#define ATOM_VEGA20_PP_PLATFORM_CAP_HARDWAREDC                  0x4
#define ATOM_VEGA20_PP_PLATFORM_CAP_BACO                        0x8
#define ATOM_VEGA20_PP_PLATFORM_CAP_BAMACO                      0x10
#define ATOM_VEGA20_PP_PLATFORM_CAP_ENABLESHADOWPSTATE          0x20

#define ATOM_VEGA20_TABLE_REVISION_VEGA20         11
#define ATOM_VEGA20_ODFEATURE_MAX_COUNT         32
#define ATOM_VEGA20_ODSETTING_MAX_COUNT         32
#define ATOM_VEGA20_PPCLOCK_MAX_COUNT           16

enum ATOM_VEGA20_ODFEATURE_ID {
  ATOM_VEGA20_ODFEATURE_GFXCLK_LIMITS = 0,
  ATOM_VEGA20_ODFEATURE_GFXCLK_CURVE,
  ATOM_VEGA20_ODFEATURE_UCLK_MAX,
  ATOM_VEGA20_ODFEATURE_POWER_LIMIT,
  ATOM_VEGA20_ODFEATURE_FAN_ACOUSTIC_LIMIT,    //FanMaximumRpm
  ATOM_VEGA20_ODFEATURE_FAN_SPEED_MIN,         //FanMinimumPwm
  ATOM_VEGA20_ODFEATURE_TEMPERATURE_FAN,       //FanTargetTemperature
  ATOM_VEGA20_ODFEATURE_TEMPERATURE_SYSTEM,    //MaxOpTemp
  ATOM_VEGA20_ODFEATURE_COUNT,
};

enum ATOM_VEGA20_ODSETTING_ID {
  ATOM_VEGA20_ODSETTING_GFXCLKFMAX = 0,
  ATOM_VEGA20_ODSETTING_GFXCLKFMIN,
  ATOM_VEGA20_ODSETTING_VDDGFXCURVEFREQ_P1,
  ATOM_VEGA20_ODSETTING_VDDGFXCURVEVOLTAGEOFFSET_P1,
  ATOM_VEGA20_ODSETTING_VDDGFXCURVEFREQ_P2,
  ATOM_VEGA20_ODSETTING_VDDGFXCURVEVOLTAGEOFFSET_P2,
  ATOM_VEGA20_ODSETTING_VDDGFXCURVEFREQ_P3,
  ATOM_VEGA20_ODSETTING_VDDGFXCURVEVOLTAGEOFFSET_P3,
  ATOM_VEGA20_ODSETTING_UCLKFMAX,
  ATOM_VEGA20_ODSETTING_POWERPERCENTAGE,
  ATOM_VEGA20_ODSETTING_FANRPMMIN,
  ATOM_VEGA20_ODSETTING_FANRPMACOUSTICLIMIT,
  ATOM_VEGA20_ODSETTING_FANTARGETTEMPERATURE,
  ATOM_VEGA20_ODSETTING_OPERATINGTEMPMAX,
  ATOM_VEGA20_ODSETTING_COUNT,
};
typedef enum ATOM_VEGA20_ODSETTING_ID ATOM_VEGA20_ODSETTING_ID;

typedef struct _ATOM_VEGA20_OVERDRIVE8_RECORD
{
  UCHAR ucODTableRevision;
  ULONG ODFeatureCount;
  UCHAR ODFeatureCapabilities [ATOM_VEGA20_ODFEATURE_MAX_COUNT];   //OD feature support flags
  ULONG ODSettingCount;
  ULONG ODSettingsMax [ATOM_VEGA20_ODSETTING_MAX_COUNT];           //Upper Limit for each OD Setting
  ULONG ODSettingsMin [ATOM_VEGA20_ODSETTING_MAX_COUNT];           //Lower Limit for each OD Setting
} ATOM_VEGA20_OVERDRIVE8_RECORD;

enum ATOM_VEGA20_PPCLOCK_ID {
  ATOM_VEGA20_PPCLOCK_GFXCLK = 0,
  ATOM_VEGA20_PPCLOCK_VCLK,
  ATOM_VEGA20_PPCLOCK_DCLK,
  ATOM_VEGA20_PPCLOCK_ECLK,
  ATOM_VEGA20_PPCLOCK_SOCCLK,
  ATOM_VEGA20_PPCLOCK_UCLK,
  ATOM_VEGA20_PPCLOCK_FCLK,
  ATOM_VEGA20_PPCLOCK_DCEFCLK,
  ATOM_VEGA20_PPCLOCK_DISPCLK,
  ATOM_VEGA20_PPCLOCK_PIXCLK,
  ATOM_VEGA20_PPCLOCK_PHYCLK,
  ATOM_VEGA20_PPCLOCK_COUNT,
};
typedef enum ATOM_VEGA20_PPCLOCK_ID ATOM_VEGA20_PPCLOCK_ID;

typedef struct _ATOM_VEGA20_POWER_SAVING_CLOCK_RECORD
{
  UCHAR ucTableRevision;
  ULONG PowerSavingClockCount;                                 // Count of PowerSavingClock Mode
  ULONG PowerSavingClockMax  [ATOM_VEGA20_PPCLOCK_MAX_COUNT];      // PowerSavingClock Mode Clock Maximum array In MHz
  ULONG PowerSavingClockMin  [ATOM_VEGA20_PPCLOCK_MAX_COUNT];      // PowerSavingClock Mode Clock Minimum array In MHz
} ATOM_VEGA20_POWER_SAVING_CLOCK_RECORD;

typedef struct _ATOM_VEGA20_POWERPLAYTABLE
{
      struct atom_common_table_header sHeader;
      UCHAR  ucTableRevision;
      USHORT usTableSize;
      ULONG  ulGoldenPPID;
      ULONG  ulGoldenRevision;
      USHORT usFormatID;

      ULONG  ulPlatformCaps;

      UCHAR  ucThermalControllerType;

      USHORT usSmallPowerLimit1;
      USHORT usSmallPowerLimit2;
      USHORT usBoostPowerLimit;
      USHORT usODTurboPowerLimit;
      USHORT usODPowerSavePowerLimit;
      USHORT usSoftwareShutdownTemp;

      ATOM_VEGA20_POWER_SAVING_CLOCK_RECORD PowerSavingClockTable;    //PowerSavingClock Mode Clock Min/Max array

      ATOM_VEGA20_OVERDRIVE8_RECORD OverDrive8Table;       //OverDrive8 Feature capabilities and Settings Range (Max and Min)

      USHORT usReserve[5];

      PPTable_t smcPPTable;

} ATOM_Vega20_POWERPLAYTABLE;

#pragma pack(pop)

#endif
