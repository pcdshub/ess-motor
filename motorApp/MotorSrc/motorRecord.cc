/*
FILENAME...     motorRecord.cc
USAGE...        Motor Record Support.

*/

/*
 *      Original Author: Jim Kowalkowski
 *      Previous Author: Tim Mooney
 *      Current Author: Ron Sluiter
 *
 *      Experimental Physics and Industrial Control System (EPICS)
 *
 *      Copyright 1995, 1996 the University of Chicago Board of Governors.
 *
 *      This software was produced under U.S. Government contract:
 *      (W-31-109-ENG-38) at Argonne National Laboratory.
 *
 *      Developed by
 *              The Beamline Controls and Data Acquisition Group
 *              Experimental Facilities Division
 *              Advanced Photon Source
 *              Argonne National Laboratory
 *
 *      Co-developed with
 *              The Controls and Computing Group
 *              Accelerator Systems Division
 *              Advanced Photon Source
 *              Argonne National Laboratory
 *
 *
 * Modification Log:
 * -----------------
 * .01 09-17-02 rls Joe Sullivan's port to R3.14.x and OSI.
 * .02 09-30-02 rls Bug fix for another "invalid state" scenario (i.e., new
 *                      target position while MIP != DONE, see README).
 * .03 10-29-02 rls - NTM field added for Soft Channel device.
 *                  - Update "last" target position in do_work() when stop
 *                      command is sent.
 * .04 03-21-03 rls - Elminate three redundant DMOV monitor postings.
 *                  - Consolidate do_work() backlash correction logic.
 * .05 04-16-03 rls - Home velocity field (HVEL) added.
 * .06 06-03-03 rls - Set DBE_LOG on all calls to db_post_events().
 * .07 10-29-03 rls - If move is in the preferred direction and the backlash
 *                    speed and acceleration are the same as the slew speed and
 *                    acceleration, then skip the backlash move and go directly
 *                    to the target position.  Bug fix for doing backlash in
 *                    wrong direction when MRES < 0.
 * .08 10-31-03 rls - Fix for bug introduced with R4.5.1; record locks-up when
 *                      BDST != 0, DLY != 0 and new target position before
 *                      backlash correction move.
 *                  - Update readback after DLY timeout.
 * .09 11-06-03 rls - Fix backlash after jog; added more state nodes to MIP so
 *                      that commands can be broken up.
 * .10 12-11-03 rls - Bug fix for tweaks ignored. When TWV < MRES and user
 *                      does TWF followed by TWR; then, a single TWF
 *                      followed by a single TWR appear to be ignored.
 * .11 12-12-03 rls - Changed MSTA access to bit field.
 * .12 12-12-03 rls - Added status update field (STUP).
 * .13 12-23-03 rls - Prevent STUP from activating DLY or setting DMOV true.
 * .14 02-10-03 rls - Update lval in load_pos() if FOFF is set to FROZEN.
 * .15 02-12-03 rls - Allow sign(MRES) != sign(ERES).
 * .16 06-16-04 rls - JAR validity check.
 * .17 09-20-04 rls - Do status update if nothing else to do.
 * .18 10-08-04 rls - Bug fix for backlashing into limit switch; update CDIR.
 * .19 12-01-04 rls - make epicsExportAddress extern "C" linkage for Windows
 *                      compatibility.
 * .20 12-02-04 rls - fixed incorrect slew acceleration calculation.
 * .21 03-13-05 rls - Removed record level round-up position code in do_work().
 * .22 04-04-05 rls - Clear homing and jog request after LS or travel limit error.
 * .23 05-31-05 rls - Bug fix for DMOV going true before last readback update
 *                      when LS error occurs.
 * .24 06-17-05 rls - Bug fix for STOP not working after target position changed.
 *                  - Don't send SET_ACCEL command when acceleration = 0.0.
 *                  - Avoid STUP errors from devices that do not have "GET_INFO"
 *                    command (e.g. Soft Channel).
 * .25 10-11-05 rls - CDIR not set correctly when jogging with DIR="Neg".
 * .26 11-23-05 rls - Malcolm Walters bug fixes for;
 *                    - get_units() returned wrong units for VMAX.
 *                    - get_graphic_double() and get_control_double() returned
 *                      incorrect values for VELO.
 * .27 02-14-06 rls - Bug fix for record issuing moves when |DIFF| < |RDBD|.
 *                  - removed "slop" from do_work.
 * .28 03-08-06 rls - Moved STUP processing to top of do_work() so that things
 *                    like LVIO true and SPMG set to PAUSE do not prevent
 *                    status update.
 * .29 06-30-06 rls - Change do_work() test for "don't move if within RDBD",
 *                    from float to integer; avoid equality test errors.
 * .30 03-16-07 rls - Clear home request when soft-limit violation occurs.
 * .31 04-06-07 rls - RDBD was being used in motordevCom.cc
 *                    motor_init_record_com() before the validation check.
 * .40 11-02-07 pnd - Use absolute value of mres to calculate rdbdpos
 * .41 11-06-07 rls - Do relative moves only if retries are enabled (RTRY != 0).
 *                  - Change NTM logic to use reference positions rather than
 *                    feedback (readback). This eliminates unwanted MR stop
 *                    commands with DC servoing.
 *                  - Do not post process previous move on "tdir" detection.
 *                    Clear post process indicator (pp). This fixes long moves
 *                    at backlash velocity after a new target position.
 * .42 11-23-07 pnd - Correct use of MRES in NTM logic to use absolute value
 * .43 11-27-07 rls - Set VBAS before jogging.
 * .44 02-28-08 rls - Prevent multiple LOAD_POS actions due to STUP's.
 *                  - Remove redundant DMOV posting from special().
 *                  - NTM logic is restored to using feedbacks; NTMF added.
 * .45 03-24-08 rls - Set DRBV based on RRBV only if URIP = NO.
 * .46 05-08-08 rls - Missing "break" in special().
 * .47 10-15-08 rls - scanOnce declaration changed from (void * precord) to
 *                    (struct dbCommon *) with EPICS base R3.14.10
 * .48 10-27-08 mrp - In alarm_sub test for EA_SLIP_STALL and RA_PROBLEM in MSTA
 *                    and put record into MAJOR STATE alarm.
 *                    Fix for the bug where the retry count is not incremented 
 *                    when doing retries.
 * .49 11-19-08 rls - RMOD field added for arthmetic and geometric sequence
 *                    retries; i.e., 10/10, 9/10, 8/10,...
 *                  - post changes to TWF/TWR.
 *                  - ramifications of ORing MIP_MOVE with MIP_RETRY.
 * .50 03-18-09 rls - Prevents multiple STOP commands by adding check for
 *                    !MIP_STOP to NTM logic.
 *                  - Unconditionally set postprocess indicator TRUE in
 *                    do_work() so postProcess() can do backlash.
 * .51 06-11-09 rls - Error since R5-3; PACT cleared early when MIP_DELAY_REQ
 *                    set.
 *                  - Prevent redundant DMOV postings when using DLY field.
 * .52 10-19-09 rls - Bug fix for homing in the wrong direction when MRES<0.
 * .53 10-20-09 rls - Pre-R3.14.10 compatibility for scanOnce() deceleration
 *                    change in dbScan.h
 * .54 10-27-09 rls - reverse which limit switch is used in do_work() home
 *                    search error check based on DIR field.
 * .55 02-18-10 rls - Fix for backlash not done when MRES<0 and DIR="Neg".
 * .56 03-18-10 rls - MSTA wrong at boot-up; force posting from init_record().
 * .57 03-24-10 rls - removed monitor()'s subroutine; post_MARKed_fields().
 *                  - removed unused and under used MMAP and NMAP indicators;
 *                    added MMAP indicator for STOP field.
 *                  - removed depreciated RES field.
 *                  - changed MDEL/ADEL support for RBV so that record behaves
 *                    the same as before when MDEL and ADEL are zero.
 * .58 04-15-10 rls - Added SYNC field to synchronize VAL/DVAL/RVAL with
 *                    RBV/DRBV/RRBV
 * .59 09-08-10 rls - clean-up RCNT change value posting in do_work().
 *                  - bug fix for save/restore not working when URIP=Yes. DRBV
 *                    not getting initialized. Fixed in initial call to
 *                    process_motor_info().
 * .60 06-23-11 kmp - Added a check for a non-zero MIP before doing retries.
 * .61 06-24-11 rls - No retries after backlash or jogging. Move setting
 *                    MIP <- DONE and reactivating Jog request from
 *                    postProcess() to maybeRetry().
 * .62 10-20-11 rls - Disable soft travel limit error check during home search.
 *                  - Use home velocity (HVEL), base velocity (BVEL) and accel.
 *                    time (ACCL) fields to calculate home acceleration rate.
 * .63 04-10-12 kmp - Inverted the priority of sync and status update in do_work().
 * .64 07-13-12 mrp - Fixed problem with using DLY field. If a process due to device support
 *                    happened before the DLY timer expired, then the put callback 
 *                    returned prematurely. Also, if there was no process due to
 *                    device support, then the record could get stuck at the end of the move
 *                    because it wasn't setting DMOV back to True or processing forward links.
 * .65 07-23-12 rls - The motor record's process() function was not processing
 *                    alarms, events and the forward scan link in the same order
 *                    as specified in the "EPICS Application Developer's Guide".
 * .66 09-06-12 rls - Refix of DLY problem (see 64 above). Hold DMOV false until DLY times out.
 * .67 06-12-13 rls - Ignore RDBD on 1st move.
 *                  - Toggle DMOV on tweaks (TWF/TWR).
 *                  - Remove soft travel-limit error checks from home search request.
 *                  - Moved synch'ing target position with readback to subroutine.
 *                  - Allow moving (new target position, jog or home search) out of invalid
 *                    soft limit travel range toward valid soft limit travel range.
 *                  - Added In-position retry mode for servos.
 * .68 06-20-13 rls - bug fix for backlash using relative moves when RMOD = "In-Position".
 *                  - bug fix for "can't tweak in either direction near soft-travel limit".
 *                    No need in process() to test MIP_MOVE type moves for soft-travel limits.
 *                  - Need "preferred_dir" for LVIO test. Moved LVIO test in do_work() to
 *                    after "preferred_dir" is set.
 * .69 05-19-14 rls - Set "stop" field true if driver returns RA_PROBLEM true. (Motor record
 *                    stops motion when controller signals error but does not stop motion; e.g.,
 *                    maximum velocity exceeded.)
 * .70 07-30-14 rls - Removed postProcess flag (pp) from LOAD_POS. Fixes bug where target positions
 *                    were not updating.
 *                  - Removed redundant postings of RMP and REP by moving them to device support's
 *                    motor_update_values() and update_values().
 *                  - Fix for LOAD_POS not posting RVAL.
 *                  - Reversed order of issuing SET_VEL_BASE and SET_VELOCITY commands. Fixes MAXv
 *                    command errors.
 * .71 02-25-15 rls - Fix for excessive motor record forward link processing.
 * .72 03-13-15 rls - Changed RDBL to set RRBV rather than DRBV.
 * .73 02-15-16 rls - JOGF/R soft limit error check was using the wrong coordinate sytem limits.
 *                    Changed error checks from dial to user limits.
 * .74 09-28-16 rls - Reverted .71 FLNK change. Except for the condition that DMOV == FALSE, FLNK
 *                    processing was standard. If processing is needed on a DMOV false to true
 *                    transition, a new motor record field should be added.
 * .75 05-18-17 rls - Stop motor if URIP is Yes and RDBL read returns an error. 
 */                                                          

#define VERSION 6.92

#include    <stdlib.h>
#include    <string.h>
#include    <stdarg.h>
#include    <alarm.h>
#include    <dbDefs.h>
#include    <callback.h>
#include    <dbAccess.h>
#include    <dbScan.h>
#include    <recGbl.h>
#include    <recSup.h>
#include    <dbEvent.h>
#include    <devSup.h>
#include    <math.h>
#include    <motor_priv.h>

#define GEN_SIZE_OFFSET
#include    "motorRecord.h"
#undef GEN_SIZE_OFFSET

#include    "motor.h"
#include    "epicsExport.h"
#include    "errlog.h"
#include    "motorDevSup.h"

#define DEBUG
#ifdef DEBUG
volatile int motorRecordDebug = 4;
extern "C" {epicsExportAddress(int, motorRecordDebug);}
#endif

/*----------------debugging-----------------*/

static const char *fieldIndexToStr(int fieldIndex);
static inline void Debug(int level, const char *format, ...) EPICS_PRINTF_STYLE(2,3);
static inline void Debug(int level, const char *format, ...){
  #ifdef DEBUG
    if (level < motorRecordDebug) {
      va_list pVar;
      va_start(pVar, format);
      vfprintf(stdout, format, pVar);
      va_end(pVar);
    }
  #endif
}

static const char *fieldIndexToStr(int fieldIndex)
{
#ifdef DEBUG
    switch (fieldIndex)
    {
        case motorRecordNAME: return "NAME";
        case motorRecordDESC: return "DESC";
        case motorRecordASG: return "ASG";
        case motorRecordSCAN: return "SCAN";
        case motorRecordPINI: return "PINI";
        case motorRecordPHAS: return "PHAS";
        case motorRecordEVNT: return "EVNT";
        case motorRecordTSE: return "TSE";
        case motorRecordTSEL: return "TSEL";
        case motorRecordDTYP: return "DTYP";
        case motorRecordDISV: return "DISV";
        case motorRecordDISA: return "DISA";
        case motorRecordSDIS: return "SDIS";
        case motorRecordMLOK: return "MLOK";
        case motorRecordMLIS: return "MLIS";
        case motorRecordDISP: return "DISP";
        case motorRecordPROC: return "PROC";
        case motorRecordSTAT: return "STAT";
        case motorRecordSEVR: return "SEVR";
        case motorRecordNSTA: return "NSTA";
        case motorRecordNSEV: return "NSEV";
        case motorRecordACKS: return "ACKS";
        case motorRecordACKT: return "ACKT";
        case motorRecordDISS: return "DISS";
        case motorRecordLCNT: return "LCNT";
        case motorRecordPACT: return "PACT";
        case motorRecordPUTF: return "PUTF";
        case motorRecordRPRO: return "RPRO";
        case motorRecordASP: return "ASP";
        case motorRecordPPN: return "PPN";
        case motorRecordPPNR: return "PPNR";
        case motorRecordSPVT: return "SPVT";
        case motorRecordRSET: return "RSET";
        case motorRecordDSET: return "DSET";
        case motorRecordDPVT: return "DPVT";
        case motorRecordRDES: return "RDES";
        case motorRecordLSET: return "LSET";
        case motorRecordPRIO: return "PRIO";
        case motorRecordTPRO: return "TPRO";
        case motorRecordBKPT: return "BKPT";
        case motorRecordUDF: return "UDF";
        case motorRecordUDFS: return "UDFS";
        case motorRecordTIME: return "TIME";
        case motorRecordFLNK: return "FLNK";
        case motorRecordVERS: return "VERS";
        case motorRecordOFF: return "OFF";
        case motorRecordFOFF: return "FOFF";
        case motorRecordFOF: return "FOF";
        case motorRecordVOF: return "VOF";
        case motorRecordDIR: return "DIR";
        case motorRecordSET: return "SET";
        case motorRecordSSET: return "SSET";
        case motorRecordSUSE: return "SUSE";
        case motorRecordVELO: return "VELO";
        case motorRecordVBAS: return "VBAS";
        case motorRecordVMAX: return "VMAX";
        case motorRecordS: return "S";
        case motorRecordSBAS: return "SBAS";
        case motorRecordSMAX: return "SMAX";
        case motorRecordACCL: return "ACCL";
        case motorRecordBDST: return "BDST";
        case motorRecordBVEL: return "BVEL";
        case motorRecordSBAK: return "SBAK";
        case motorRecordBACC: return "BACC";
        case motorRecordFRAC: return "FRAC";
        case motorRecordOUT: return "OUT";
        case motorRecordCARD: return "CARD";
        case motorRecordRDBL: return "RDBL";
        case motorRecordDOL: return "DOL";
        case motorRecordOMSL: return "OMSL";
        case motorRecordRLNK: return "RLNK";
        case motorRecordSREV: return "SREV";
        case motorRecordUREV: return "UREV";
        case motorRecordMRES: return "MRES";
        case motorRecordERES: return "ERES";
        case motorRecordRRES: return "RRES";
        case motorRecordUEIP: return "UEIP";
        case motorRecordURIP: return "URIP";
        case motorRecordPREC: return "PREC";
        case motorRecordEGU: return "EGU";
        case motorRecordHLM: return "HLM";
        case motorRecordLLM: return "LLM";
        case motorRecordDHLM: return "DHLM";
        case motorRecordDLLM: return "DLLM";
        case motorRecordHOPR: return "HOPR";
        case motorRecordLOPR: return "LOPR";
        case motorRecordHLS: return "HLS";
        case motorRecordLLS: return "LLS";
        case motorRecordRHLS: return "RHLS";
        case motorRecordRLLS: return "RLLS";
        case motorRecordHIHI: return "HIHI";
        case motorRecordLOLO: return "LOLO";
        case motorRecordHIGH: return "HIGH";
        case motorRecordLOW: return "LOW";
        case motorRecordHHSV: return "HHSV";
        case motorRecordLLSV: return "LLSV";
        case motorRecordHSV: return "HSV";
        case motorRecordLSV: return "LSV";
        case motorRecordHLSV: return "HLSV";
        case motorRecordMISV: return "MISV";
        case motorRecordRDBD: return "RDBD";
        case motorRecordSDBD: return "SDBD";
        case motorRecordRCNT: return "RCNT";
        case motorRecordRTRY: return "RTRY";
        case motorRecordMISS: return "MISS";
        case motorRecordSPMG: return "SPMG";
        case motorRecordLSPG: return "LSPG";
        case motorRecordSTOP: return "STOP";
        case motorRecordHOMF: return "HOMF";
        case motorRecordHOMR: return "HOMR";
        case motorRecordJOGF: return "JOGF";
        case motorRecordJOGR: return "JOGR";
        case motorRecordTWF: return "TWF";
        case motorRecordTWR: return "TWR";
        case motorRecordTWV: return "TWV";
        case motorRecordVAL: return "VAL";
        case motorRecordDVAL: return "DVAL";
        case motorRecordRVAL: return "RVAL";
        case motorRecordRLV: return "RLV";
        case motorRecordRBV: return "RBV";
        case motorRecordDRBV: return "DRBV";
        case motorRecordDIFF: return "DIFF";
        case motorRecordRDIF: return "RDIF";
        case motorRecordCDIR: return "CDIR";
        case motorRecordRRBV: return "RRBV";
        case motorRecordRMP: return "RMP";
        case motorRecordREP: return "REP";
        case motorRecordRVEL: return "RVEL";
        case motorRecordDMOV: return "DMOV";
        case motorRecordMOVN: return "MOVN";
        case motorRecordMSTA: return "MSTA";
        case motorRecordMFLG: return "MFLG";
        case motorRecordLVIO: return "LVIO";
        case motorRecordTDIR: return "TDIR";
        case motorRecordATHM: return "ATHM";
        case motorRecordPP: return "PP";
        case motorRecordMIP: return "MIP";
        case motorRecordMMAP: return "MMAP";
        case motorRecordNMAP: return "NMAP";
        case motorRecordDLY: return "DLY";
        case motorRecordCBAK: return "CBAK";
        case motorRecordpriv: return "priv";
        case motorRecordPCOF: return "PCOF";
        case motorRecordICOF: return "ICOF";
        case motorRecordDCOF: return "DCOF";
        case motorRecordCNEN: return "CNEN";
        case motorRecordINIT: return "INIT";
        case motorRecordPREM: return "PREM";
        case motorRecordPOST: return "POST";
        case motorRecordSTOO: return "STOO";
        case motorRecordDINP: return "DINP";
        case motorRecordRINP: return "RINP";
        case motorRecordJVEL: return "JVEL";
        case motorRecordJAR: return "JAR";
        case motorRecordLOCK: return "LOCK";
        case motorRecordNTM: return "NTM";
        case motorRecordNTMF: return "NTMF";
        case motorRecordHVEL: return "HVEL";
        case motorRecordSTUP: return "STUP";
        case motorRecordRMOD: return "RMOD";
        case motorRecordADEL: return "ADEL";
        case motorRecordMDEL: return "MDEL";
        case motorRecordSYNC: return "SYNC";
        case motorRecordIGSET: return "IGSET";
        default: return "Unknown";
        }
#else
    return "";
#endif
}



/*** Forward references ***/

static int homing_wanted_and_allowed(motorRecord *pmr);
static RTN_STATUS do_work(motorRecord *, CALLBACK_VALUE);
static void alarm_sub(motorRecord *);
static void monitor(motorRecord *);
static void process_motor_info(motorRecord *, bool);
static void load_pos(motorRecord *);
static void check_resolution(motorRecord *);
static void check_speed(motorRecord *);
static void set_dial_highlimit(motorRecord *);
static void set_dial_lowlimit(motorRecord *);
static void set_user_highlimit(motorRecord *);
static void set_user_lowlimit(motorRecord *);
static void set_userlimits(motorRecord *);
static void range_check(motorRecord *, double *, double, double);
static void clear_buttons(motorRecord *);
static void syncTargetPosition(motorRecord *);

/*** Record Support Entry Table (RSET) functions. ***/

static long init_record(dbCommon *, int);
static long process(dbCommon *);
static long special(DBADDR *, int);
static long get_units(const DBADDR *, char *);
static long get_precision(const DBADDR *, long *);
static long get_graphic_double(const DBADDR *, struct dbr_grDouble *);
static long get_control_double(const DBADDR *, struct dbr_ctrlDouble *);
static long get_alarm_double(const DBADDR  *, struct dbr_alDouble *);


rset motorRSET =
{
    RSETNUMBER,
    NULL,
    NULL,
    (RECSUPFUN) init_record,
    (RECSUPFUN) process,
    (RECSUPFUN) special,
    NULL,
    NULL,
    NULL,
    NULL,
    (RECSUPFUN) get_units,
    (RECSUPFUN) get_precision,
    NULL,
    NULL,
    NULL,
    (RECSUPFUN) get_graphic_double,
    (RECSUPFUN) get_control_double,
    (RECSUPFUN) get_alarm_double
};
extern "C" {epicsExportAddress(rset, motorRSET);}


/*******************************************************************************
Support for keeping track of which record fields have been changed, so we can
eliminate redundant db_post_events() without having to think, and without having
to keep lots of "last value of field xxx" fields in the record.  The idea is
to say...

        MARK(M_XXXX);

when you mean...

        db_post_events(pmr, &pmr->xxxx, monitor_mask);

Before leaving, you have to call monitor() to actually post the
field to all listeners.

        --- NOTE WELL ---
        The macros below assume that the variable "pmr" exists and points to a
        motor record, like so:
                motorRecord *pmr;
        No check is made in this code to ensure that this really is true.
*******************************************************************************/
/* Bit field for "mmap". */
typedef union
{
    epicsUInt32 All;
    struct
    {
        unsigned int M_VAL      :1;
        unsigned int M_DVAL     :1;
        unsigned int M_HLM      :1;
        unsigned int M_LLM      :1;
        unsigned int M_DMOV     :1;
        unsigned int M_SPMG     :1;
        unsigned int M_RCNT     :1;
        unsigned int M_MRES     :1;
        unsigned int M_ERES     :1;
        unsigned int M_UEIP     :1;
        unsigned int M_STOP     :1;
        unsigned int M_LVIO     :1;
        unsigned int M_RVAL     :1;
        unsigned int M_RLV      :1;
        unsigned int M_OFF      :1;
        unsigned int M_RBV      :1;
        unsigned int M_DHLM     :1;
        unsigned int M_DLLM     :1;
        unsigned int M_DRBV     :1;
        unsigned int M_MOVN     :1;
        unsigned int M_HLS      :1;
        unsigned int M_LLS      :1;
        unsigned int M_RRBV     :1;
        unsigned int M_MSTA     :1;
        unsigned int M_ATHM     :1;
        unsigned int M_TDIR     :1;
        unsigned int M_MIP      :1;
        unsigned int M_DIFF     :1;
        unsigned int M_RDIF     :1;
    } Bits;
} mmap_field;

/* Bit field for "nmap". */
typedef union
{
    epicsUInt32 All;
    struct
    {
        unsigned int M_SBAS     :1;
        unsigned int M_SREV     :1;
        unsigned int M_UREV     :1;
        unsigned int M_VELO     :1;
        unsigned int M_VBAS     :1;
        unsigned int M_MISS     :1;
        unsigned int M_STUP     :1;
        unsigned int M_JOGF     :1;
        unsigned int M_JOGR     :1;
        unsigned int M_HOMF     :1;
        unsigned int M_HOMR     :1;
        unsigned int M_CDIR     :1;
    } Bits;
} nmap_field;


/* How to move, either use VELO/ACCL or BVEL/BACC */
enum moveMode{
  moveModePosition,
  moveModeBacklash
};


/******************************************************************************
 * Debug MIP and MIP changes
 * Not yet used (needs better printing)
*******************************************************************************/
static void mipSetBit(motorRecord *pmr, unsigned v)
{
  pmr->mip |= v;
}

static void mipClrBit(motorRecord *pmr, unsigned v)
{
  pmr->mip &= ~v;
}

static void mipSetMip(motorRecord *pmr, unsigned v)
{
  pmr->mip = v;
}

#ifdef DEBUG

static void dbgMipToString(unsigned v, char *buf, size_t buflen)
{
  memset(buf, 0, buflen);
  snprintf(buf, buflen-1,
           "'%s%s%s%s%s%s%s%s%s%s%s%s%s%s%s%s'",
           v & MIP_JOGF      ? "Jf " : "",
           v & MIP_JOGR      ? "Jr " : "",
           v & MIP_JOG_BL1   ? "J1 " : "",
           v & MIP_HOMF      ? "Hf " : "",
           v & MIP_HOMR      ? "Jr " : "",
           v & MIP_MOVE      ? "Mo " : "",
           v & MIP_RETRY     ? "Ry " : "",
           v & MIP_LOAD_P    ? "Lp " : "",
           v & MIP_MOVE_BL   ? "Mb " : "",
           v & MIP_STOP      ? "St " : "",
           v & MIP_DELAY_REQ ? "Dr " : "",
           v & MIP_DELAY_ACK ? "Da " : "",
           v & MIP_JOG_REQ   ? "jR " : "",
           v & MIP_JOG_STOP  ? "jS " : "",
           v & MIP_JOG_BL2   ? "J2 " : "",
           v & MIP_EXTERNAL  ? "Ex " : "");
}

/*  abbreviated Bits: */
/* Jf Jr J1 Hf Hr Mo Rt Lp MB St Dr Da JR Js J2 Ex */
/* 16 bits * 3 + NUL + spare */
#define MBLE 50

#define MIP_SET_BIT(v)                               \
  do {                                               \
    char dbuf[MBLE];                                 \
    char obuf[MBLE];                                 \
    char nbuf[MBLE];                                 \
    epicsUInt16 old = pmr->mip;                      \
    mipSetBit(pmr,(v));                              \
    dbgMipToString(v, dbuf, sizeof(dbuf));           \
    dbgMipToString(old, obuf, sizeof(obuf));         \
    dbgMipToString(pmr->mip, nbuf, sizeof(nbuf));    \
    fprintf(stdout,                                  \
            "%s:%d %s mipSetBit %s old=%s new=%s\n", \
            __FILE__, __LINE__, pmr->name,           \
            dbuf, obuf, nbuf);                       \
    fflush(stdout);                                  \
  }                                                  \
  while(0)


#define MIP_CLR_BIT(v)                               \
  do {                                               \
    char dbuf[MBLE];                                 \
    char obuf[MBLE];                                 \
    char nbuf[MBLE];                                 \
    epicsUInt16 old = pmr->mip;                      \
    mipClrBit(pmr,(v));                              \
    dbgMipToString(v, dbuf, sizeof(dbuf));           \
    dbgMipToString(old, obuf, sizeof(obuf));         \
    dbgMipToString(pmr->mip, nbuf, sizeof(nbuf));    \
    fprintf(stdout,                                  \
            "%s:%d %s mipClrBit %s old=%s new=%s\n", \
            __FILE__, __LINE__, pmr->name,           \
            dbuf, obuf, nbuf);                       \
    fflush(stdout);                                  \
  }                                                  \
  while(0)

#define MIP_SET_VAL(v)                               \
  do {                                               \
    char dbuf[MBLE];                                 \
    char obuf[MBLE];                                 \
    char nbuf[MBLE];                                 \
    epicsUInt16 old = pmr->mip;                      \
    mipSetMip(pmr,(v));                              \
    dbgMipToString(v, dbuf, sizeof(dbuf));           \
    dbgMipToString(old, obuf, sizeof(obuf));         \
    dbgMipToString(pmr->mip, nbuf, sizeof(nbuf));    \
    fprintf(stdout,                                  \
            "%s:%d %s mipSetVal %s old=%s new=%s\n", \
            __FILE__, __LINE__, pmr->name,           \
            dbuf, obuf, nbuf);                       \
    fflush(stdout);                                  \
    }                                                \
  while(0)

#else
#define MIP_SET_BIT(v) mipSetBit(pmr,(v))
#define MIP_CLR_BIT(v) mipClrBit(pmr,(v))
#define MIP_SET_VAL(v) mipSetMip(pmr,(v))
#endif


#define MARK(FIELD) {mmap_field temp; temp.All = pmr->mmap; \
                    temp.Bits.FIELD = 1; pmr->mmap = temp.All;}
#define MARK_AUX(FIELD) {nmap_field temp; temp.All = pmr->nmap; \
                    temp.Bits.FIELD = 1; pmr->nmap = temp.All;}

#define UNMARK(FIELD) {mmap_field temp; temp.All = pmr->mmap; \
                    temp.Bits.FIELD = 0; pmr->mmap = temp.All;}
#define UNMARK_AUX(FIELD) {nmap_field temp; temp.All = pmr->nmap; \
                    temp.Bits.FIELD = 0; pmr->nmap = temp.All;}

/*
WARNING!!! The following macros assume that a variable (i.e., mmap_bits
        and/or nmap_bits) has been declared within the scope its' occurence
        AND initialized.
*/

#define MARKED(FIELD) (mmap_bits.Bits.FIELD)
#define MARKED_AUX(FIELD) (nmap_bits.Bits.FIELD)

#define UNMARK_ALL      pmr->mmap = pmr->nmap = 0


/*
The DLY feature uses the OSI facility, callbackRequestDelayed(), to issue a
callbackRequest() on the structure below.  This structure is dynamically
allocated by init_record().  init_record() saves the pointer to this structure
in the motorRecord.  See process() for use of this structure when Done Moving
field (DMOV) is TRUE.
*/

struct callback         /* DLY feature callback structure. */
{
    CALLBACK dly_callback;
    struct motorRecord *precord;
};

static void callbackFunc(struct callback *pcb)
{
    motorRecord *pmr = pcb->precord;

    /*
     * It's possible user has requested stop, or in some other way rescinded
     * the delay request that resulted in this callback.  Check to make sure
     * this callback hasn't been orphaned by events occurring between the time
     * the watchdog was started and the time this function was invoked.
     */
    if (pmr->mip & MIP_DELAY_REQ)
    {
        MIP_CLR_BIT(MIP_DELAY_REQ);      /* Turn off REQ. */
        MIP_SET_BIT(MIP_DELAY_ACK);      /* Turn on ACK. */
#if LT_EPICSBASE(3,14,10,0)
	scanOnce(pmr);
#else
	scanOnce((struct dbCommon *) pmr);
#endif
    }
}


static bool softLimitsDefined(motorRecord *pmr)
{
   if ((pmr->dhlm == pmr->dllm) && (pmr->dllm == 0.0))
       return FALSE;
   else
     return TRUE;
}
/******************************************************************************
        enforceMinRetryDeadband()

Calculate minumum retry deadband (.rdbd) achievable under current
circumstances, and enforce this minimum value.
Make RDBD >= MRES.
******************************************************************************/
static void enforceMinRetryDeadband(motorRecord * pmr)
{
    double old_sdbd = pmr->sdbd;
    double old_rdbd = pmr->rdbd;
    if (!pmr->rdbd && pmr->priv->configRO.motorRDBDDial > 0.0)
        pmr->rdbd = pmr->priv->configRO.motorRDBDDial;

    if (!pmr->sdbd)
    {
        if (pmr->priv->configRO.motorSDBDDial > 0.0)
            pmr->sdbd = pmr->priv->configRO.motorSDBDDial;
        else /* SDBD is 0, set it to MRES */
            pmr->sdbd = fabs(pmr->mres);
        /* if SDBD was 0.0, it must be less than RDBD */
        range_check(pmr, &pmr->sdbd, 0.0, pmr->rdbd);
    }
    if (pmr->sdbd != old_sdbd)
        db_post_events(pmr, &pmr->sdbd, DBE_VAL_LOG);
    if (pmr->rdbd != old_rdbd)
        db_post_events(pmr, &pmr->rdbd, DBE_VAL_LOG);
}


static void recalcLVIO(motorRecord *pmr)
{
  int old_lvio = pmr->lvio;
  int lvio = 0;
  if (!softLimitsDefined(pmr))
      ;
  else if ((pmr->drbv > pmr->dhlm + pmr->rdbd) ||
           (pmr->drbv < pmr->dllm - pmr->rdbd))
  {
      pmr->lvio = 1;
  }
  if (lvio != old_lvio)
  {
      pmr->lvio = lvio;
      MARK(M_LVIO);
  }
  Debug(3,"%s:%d %s recalcLVIO lvio=%d drbv=%f rdbd=%f dhlm=%f dllm=%f\n",
        __FILE__, __LINE__, pmr->name,
        lvio, pmr->drbv, pmr->rdbd, pmr->dhlm, pmr->dllm);
}

/******************************************************************************
        init_record()

Called twice after an EPICS database has been loaded, and then never called
again.

LOGIC:
    IF first call (pass == 0).
        Initialize VERS field to Motor Record version number.
        NORMAL RETURN.
    ENDIF
    ...
    ...
    ...
    Initialize Limit violation field false.
    IF (Software Travel limits are NOT disabled), AND,
            (Dial readback violates dial high limit), OR,
            (Dial readback violates dial low limit)
        Set Limit violation field true.
    ENDIF
    ...
    Call monitor().
    NORMAL RETURN.

*******************************************************************************/

static long initial_poll(motorRecord *pmr)
{
    process_motor_info(pmr, true);

    /*
     * If we're in closed-loop mode, initializing the user- and dial-coordinate
     * motor positions (.val and .dval) is someone else's job. Otherwise,
     * initialize them to the readback values (.rbv and .drbv) set by our
     * recent call to process_motor_info().
     */
    if (pmr->omsl != menuOmslclosed_loop)
    {
        pmr->val = pmr->rbv;
        MARK(M_VAL);
        pmr->dval = pmr->drbv;
        MARK(M_DVAL);
        pmr->rval = NINT(pmr->dval / pmr->mres);
        MARK(M_RVAL);
    }

    if (!softLimitsDefined(pmr))
    {
        /* The record has no soft limits, but the controller may have */
        if (pmr->priv->softLimitRO.motorDialLimitsValid)
        {
            pmr->dhlm = pmr->priv->softLimitRO.motorDialHighLimitRO;
            pmr->dllm = pmr->priv->softLimitRO.motorDialLowLimitRO;
        }
    }
    /* Reset limits in case database values are invalid. */
    set_dial_highlimit(pmr);
    set_dial_lowlimit(pmr);

    check_speed(pmr);
    enforceMinRetryDeadband(pmr);

    /* Initialize miscellaneous control fields. */
    pmr->dmov = TRUE;
    MARK(M_DMOV);
    pmr->movn = FALSE;
    MARK(M_MOVN);
    pmr->lspg = pmr->spmg = motorSPMG_Go;
    MARK(M_SPMG);
    pmr->diff = pmr->dval - pmr->drbv;
    MARK(M_DIFF);
    pmr->priv->last.val = pmr->val;
    pmr->priv->last.dval = pmr->dval;
    pmr->priv->last.rval = pmr->rval;
    pmr->lvio = 0;              /* init limit-violation field */

    recalcLVIO(pmr);
    MARK(M_MSTA);   /* MSTA incorrect at boot-up; force posting. */

    monitor(pmr);
    return(OK);
}


static long init_record(dbCommon* arg, int pass)
{
    motorRecord *pmr = (motorRecord *) arg;
    struct motor_dset *pdset;
    long status;
    CALLBACK_VALUE process_reason;
    struct callback *pcallback; /* v3.2 */
    const char errmsg[] = "motor:init_record()";

    if (pass == 0)
    {
        pmr->vers = VERSION;
        return(OK);
    }
    /* Check that we have a device-support entry table. */
    pdset = (struct motor_dset *) pmr->dset;
    if (pdset == NULL)
    {
        recGblRecordError(S_dev_noDSET, (void *) pmr, (char *) errmsg);
        return (S_dev_noDSET);
    }
    /* Check that DSET has pointers to functions we need. */
    if ((pdset->base.number < 8) ||
        (pdset->update_values == NULL) ||
        (pdset->start_trans == NULL) ||
        (pdset->build_trans == NULL) ||
        (pdset->end_trans == NULL))
    {
        recGblRecordError(S_dev_missingSup, (void *) pmr, (char *) errmsg);
        return (S_dev_missingSup);
    }

    /*** setup callback for readback settling time delay (v3.2) ***/
    pcallback = (struct callback *) (calloc(1, sizeof(struct callback)));
    pmr->cbak = (void *) pcallback;
    callbackSetCallback((void (*)(struct callbackPvt *)) callbackFunc,
                        &pcallback->dly_callback);
    callbackSetPriority(pmr->prio, &pcallback->dly_callback);
    pcallback->precord = pmr;
    pmr->priv = (struct motor_priv*)calloc(1, sizeof(struct motor_priv));

    if (pmr->eres == 0.0)
    {
        pmr->eres = pmr->mres;
        MARK(M_ERES);
    }

    /*
     * Reconcile two different ways of specifying speed and resolution; make
     * sure things are sane.
     */
    check_resolution(pmr);

    /* Call device support to initialize itself and the driver */
    if (pdset->base.init_record)
    {
        status = (*pdset->base.init_record) (pmr);
        if (status)
        {
            pmr->card = -1;
            return (status);
        }
        switch (pmr->out.type)
        {
            case (VME_IO):
                pmr->card = pmr->out.value.vmeio.card;
                break;
            case (CONSTANT):
            case (PV_LINK):
            case (DB_LINK):
            case (CA_LINK):
                pmr->card = -1;
                break;
            case (INST_IO):
                pmr->card = 0;
                break;
            default:
                recGblRecordError(S_db_badField, (void *) pmr, (char *) errmsg);
                return(ERROR);
        }
    }
    /*
     * .dol (Desired Output Location) is a struct containing either a link to
     * some other field in this database, or a constant intended to initialize
     * the .val field.  If the latter, get that initial value and apply it.
     */
    if (pmr->dol.type == CONSTANT)
    {
        pmr->udf = FALSE;
        recGblInitConstantLink(&pmr->dol, DBF_DOUBLE, &pmr->val);
    }

    /*
     * Get motor position, encoder position, status, and readback-link value by
     * calling process_motor_info().
     * 
     * v3.2 Fix so that first call to process() doesn't appear to be a callback
     * from device support.  (Reset ptrans->callback_changed to NO in devSup).
     */
    process_reason = (*pdset->update_values) (pmr);
    switch (process_reason) {
        case NOTHING_DONE:
	    if (pmr->dol.type == CONSTANT)
                 pmr->udf = TRUE;
            break;
        case CALLBACK_DATA:
            initial_poll(pmr);
            break;
        case CALLBACK_NEWLIMITS:
            break;
    }
    return OK;
}


/******************************************************************************
        postProcess()

Post process a command or motion after motor has stopped. We do this for
any of several reasons:
        1) This is the first call to process()
        2) User hit a "Stop" button, and motor has stopped.
        3) User released a "Jog*" button and motor has stopped.
        4) Hom* command has completed.
        5) User hit Hom* or Jog* while motor was moving, causing a
                'stop' to be sent to the motor, and the motor has stopped.
        6) User caused a new value to be written to the motor hardware's
                position register.
        7) We hit a limit switch.
LOGIC:
    Clear post process command field; PP.
    IF Output Mode Select field set to CLOSED_LOOP, AND,
       NOT a "move", AND, NOT a "backlash move".
        Make drive values agree with readback value;
            VAL  <- RBV
            DVAL <- DRBV
            RVAL <- DVAL converted to motor steps.
            DIFF <- RDIF <- 0
    ENDIF
    IF done with either load-position or load-encoder-ratio commands.
        Clear MIP.
    ELSE IF done homing.
        ...
        ...
    ELSE IF done stopping after jog, OR, done with move.
        IF |backlash distance| > |motor resolution|.
            IF Retry enabled, AND, [use encoder true, OR, use readback link true]
                Set relative positioning indicator true.
            ELSE
                Set relative positioning indicator false.
            ENDIF
            Do backlasth correction.
        ELSE
            Set MIP to DONE.
            IF there is a jog request and the corresponding LS is off.
                Set jog requesst on in MIP.
            ENDIF
        ENDIF
        ...
        ...
    ELSE IF done with 1st phase take out backlash after jog.
        Calculate backlash velocity, base velocity, backlash accel. and backlash position.
        IF Retry enabled, AND, [use encoder true, OR, use readback link true]
            Set relative positioning indicator true.
        ELSE
            Set relative positioning indicator false.
        ENDIF
        
    ELSE IF done with jog or move backlash.
        Clear MIP.
        IF (JOGF field true, AND, Hard High limit false), OR,
                (JOGR field true, AND, Hard Low  limit false)
            Set Jog request state true.
        ENDIF
    ENDIF
    
    
******************************************************************************/
static void doMoveDialPosition(motorRecord *pmr, enum moveMode moveMode,
                               double position)
{
    /* Use if encoder or ReadbackLink is in use. */
    bool use_rel = (pmr->rtry != 0 && pmr->rmod != motorRMOD_I && (pmr->ueip || pmr->urip));
    double diff = (position - pmr->drbv) * pmr->frac;
    double amres = fabs(pmr->mres);
    double vbase = pmr->vbas;
    double vel, accEGU;

    switch (moveMode) {
    case moveModePosition:
      vel = pmr->velo;
      accEGU = (vel - vbase) > 0 ? ((vel - vbase)/ pmr->accl) : (vel / pmr->accl);
      break;
    case moveModeBacklash:
      vel = pmr->bvel;
      accEGU = (vel - vbase) > 0 ? ((vel - vbase)/ pmr->bacc) : (vel / pmr->bacc);
      break;
    default:
      vel = accEGU = 0.0;
    }
    if (use_rel)
        devSupMoveRelRaw(pmr, vel/amres, vbase/amres, accEGU/amres, diff/pmr->mres);
    else
        devSupMoveAbsRaw(pmr, vel/amres, vbase/amres, accEGU/amres, position/pmr->mres);
    pmr->priv->last.commandedDval = position;
    setCDIRfromDialMove(pmr, diff < 0.0 ? 0 : 1);
}

/*****************************************************************************
  High level functions which are used by the state machine
*****************************************************************************/
static void doBackLash(motorRecord *pmr)
{
    /* Restore DMOV to false and UNMARK it so it is not posted. */
    pmr->dmov = FALSE;
    UNMARK(M_DMOV);

    if (pmr->mip & MIP_JOG_STOP)
    {
        doMoveDialPosition(pmr, moveModePosition, pmr->dval - pmr->bdst);
        MIP_SET_VAL(MIP_JOG_BL1);
    }
    else if(pmr->mip & MIP_MOVE)
    {
        /* First part of move done. Do backlash correction. */
        doMoveDialPosition(pmr, moveModeBacklash, pmr->dval);
        pmr->rval = NINT(pmr->dval);
        MIP_SET_VAL(MIP_MOVE_BL);
    }
    else if (pmr->mip & MIP_JOG_BL1)
    {
        /* First part of jog done. Do backlash correction. */
        doMoveDialPosition(pmr, moveModeBacklash, pmr->dval);
        pmr->rval = NINT(pmr->dval);
        MIP_SET_VAL(MIP_JOG_BL2);
    }
    pmr->pp = TRUE;
}

/* No WRITE_MSG(HOME_FOR) or HOME_REV */
#define HOME_FOR #ErrorHOME_FOR
#define HOME_REV #ErrorHOME_REV

/*****************************************************************************/

static long postProcess(motorRecord * pmr)
{
#ifdef DMR_SOFTMOTOR_MODS
    int dir_positive = (pmr->dir == motorDIR_Pos);
    int dir = dir_positive ? 1 : -1;
#endif

#ifdef DEBUG
    {
        char dbuf[MBLE];
        dbgMipToString(pmr->mip, dbuf, sizeof(dbuf));
        Debug(3, "%s:%d %s postProcess: entry mip=0x%0x(%s)\n",
              __FILE__, __LINE__, pmr->name, pmr->mip, dbuf);
    }
#endif

    pmr->pp = FALSE;

    if (pmr->omsl != menuOmslclosed_loop && !(pmr->mip & MIP_MOVE) &&
        !(pmr->mip & MIP_MOVE_BL) && !(pmr->mip & MIP_JOG_BL1) &&
        !(pmr->mip & MIP_JOG_BL2))
    {
        /* Make drive values agree with readback value. */
#ifdef DMR_SOFTMOTOR_MODS
        /* Mark Rivers - make val and dval agree with rrbv, rather than rbv or
           drbv */
        pmr->val = (pmr->rrbv * pmr->mres) * dir + pmr->off;
        pmr->dval = pmr->rrbv * pmr->mres;
#else
        pmr->val = pmr->rbv;
        pmr->dval = pmr->drbv;
#endif
        MARK(M_VAL);
        MARK(M_DVAL);
        pmr->rval = NINT(pmr->dval / pmr->mres);
        MARK(M_RVAL);
        pmr->diff = 0.;
        MARK(M_DIFF);
        pmr->rdif = 0;
        MARK(M_RDIF);
    }

    if (pmr->mip & MIP_LOAD_P)
        MIP_SET_VAL(MIP_DONE);    /* We sent LOAD_POS, followed by GET_INFO. */
    else if (pmr->mip & MIP_HOME)
    {
        /* Home command */
        if (pmr->mip & MIP_STOP)
        {
            /* Stopped and Hom* button still down.  Now do Hom*. */
            MIP_CLR_BIT(MIP_STOP);
            pmr->dmov = FALSE;
            MARK(M_DMOV);
            pmr->rcnt = 0;
            MARK(M_RCNT);
            doHomeSetcdir(pmr);
            pmr->pp = TRUE;
        }
        else
        {
            if (pmr->mip & MIP_HOMF)
            {
                MIP_CLR_BIT(MIP_HOMF);
                pmr->homf = 0;
                MARK_AUX(M_HOMF);
            }
            else if (pmr->mip & MIP_HOMR)
            {
    
                MIP_CLR_BIT(MIP_HOMR);
                pmr->homr = 0;
                MARK_AUX(M_HOMR);
            }
        }
    }
    else if (pmr->mip & MIP_JOG_STOP || pmr->mip & MIP_MOVE)
    {
        if (fabs(pmr->bdst) >=  fabs(pmr->sdbd))
        {
            doBackLash(pmr);
        }
        MIP_CLR_BIT(MIP_JOG_STOP);
        MIP_CLR_BIT(MIP_MOVE);
    }
    else if (pmr->mip & MIP_JOG_BL1)
    {
        doBackLash(pmr);
    }
    /* Save old values for next call. */
    pmr->priv->last.val = pmr->val;
    pmr->priv->last.dval = pmr->dval;
    pmr->priv->last.rval = pmr->rval;
    MIP_CLR_BIT(MIP_STOP);
    MARK(M_MIP);
    return(OK);
}


/******************************************************************************
        maybeRetry()

Compare target with actual position.  If retry is indicated, set variables so
that it will happen when we return.
******************************************************************************/
static void maybeRetry(motorRecord * pmr)
{
    bool user_cdir;
    double diff = pmr->priv->last.commandedDval - pmr->drbv;

    /* Commanded direction in user coordinates. */
    user_cdir = ((pmr->dir == motorDIR_Pos) == (pmr->mres >= 0)) ? pmr->cdir : !pmr->cdir;
    if ((fabs(diff) >= pmr->rdbd) && !(pmr->hls && user_cdir) && !(pmr->lls && !user_cdir))
    {
        /* No, we're not close enough.  Try again. */
#ifdef DEBUG
        {
            char dbuf[MBLE];
            dbgMipToString(pmr->mip, dbuf, sizeof(dbuf));
            Debug(3, "%s:%d %s maybeRetry: not close enough rdbd=%f diff=%f rcnt=%d mip=0x%0x(%s)\n",
                  __FILE__, __LINE__, pmr->name, pmr->rdbd, diff, pmr->rcnt, pmr->mip, dbuf);
        }
#endif
        /* If max retry count is zero, retry is disabled */
        if (pmr->rtry == 0)
            MIP_CLR_BIT(~MIP_JOG_REQ); /* Clear everything, except jog request;
                                      * for jog reactivation in postProcess(). */
        else
        {
            if (++(pmr->rcnt) > pmr->rtry)
            {
                /* Too many retries. */
                /* pmr->spmg = motorSPMG_Pause; MARK(M_SPMG); */
                MIP_SET_VAL(MIP_DONE);
                if ((pmr->jogf && !pmr->hls) || (pmr->jogr && !pmr->lls))
                    MIP_SET_BIT(MIP_JOG_REQ);

                pmr->priv->last.val = pmr->val;
                pmr->priv->last.dval = pmr->dval;
                pmr->priv->last.rval = pmr->rval;

                /* Alarms, if configured in MISV, are done in alarm_sub() */
                pmr->miss = 1;
                MARK_AUX(M_MISS);
            }
            else
            {
                pmr->dmov = FALSE;
                UNMARK(M_DMOV);
                MIP_SET_VAL(MIP_RETRY);
            }
            MARK(M_RCNT);
        }
    }
    else
    {
        /* Yes, we're close enough to the desired value. */
#ifdef DEBUG
        {
            char dbuf[MBLE];
            dbgMipToString(pmr->mip, dbuf, sizeof(dbuf));
            Debug(1, "%s:%d %s maybeRetry: close enough; rdbd=%f diff=%f mip=0x%0x(%s)\n",
                  __FILE__, __LINE__, pmr->name, pmr->rdbd, diff, pmr->mip, dbuf);
        }
#endif
        MIP_CLR_BIT(~MIP_JOG_REQ);/* Clear everything, except jog request; for
                                 * jog reactivation in postProcess(). */
        if (pmr->miss)
        {
            pmr->miss = 0;
            MARK_AUX(M_MISS);
        }

        /* If motion was initiated by "Move" button, pause. */
        if (pmr->spmg == motorSPMG_Move)
        {
            pmr->spmg = motorSPMG_Pause;
            MARK(M_SPMG);
        }
    }
    MARK(M_MIP);
}


/******************************************************************************
        process()

Called under many different circumstances for many different reasons.

1) Someone poked our .proc field, or some other field that is marked
'process-passive' in the motorRecord.ascii file.  In this case, we
determine which fields have changed since the last time we were invoked
and attempt to act accordingly.

2) Device support will call us periodically while a motor is moving, and
once after it stops.  In these cases, we infer that device support has
called us by looking at the flag it set, report the motor's state, and
fire off readback links.  If the motor has stopped, we fire off forward links
as well.

Note that this routine handles all motor records, and that several 'copies'
of this routine may execute 'simultaneously' (in the multitasking sense), as
long as they operate on different records.  This much is normal for an EPICS
record, and the normal mechanism for ensuring that a record does not get
processed by more than one 'simultaneous' copy of this routine (the .pact field)
works here as well.

However, it is normal for an EPICS record to be either 'synchronous' (runs
to completion at every invocation of process()) or 'asynchronous' (begins
processing at one invocation and forbids all further invocations except the
callback invocation from device support that completes processing).  This
record is worse than asynchronous because we can't forbid invocations while
a motor is moving (else a motor could not be stopped), nor can we complete
processing until a motor stops.

Backlash correction would complicate this picture further, since a motor
must stop before backlash correction starts and stops it again, but device
support and the Oregon Microsystems controller allow us to string two move
commands together--even with different velocities and accelerations.

Backlash-corrected jogs (move while user holds 'jog' button down) do
complicate the picture:  we can't string the jog command together with a
backlash correction because we don't know when the user is going to release
the jog button.  Worst of all, it is possible for the user to give us a
'jog' command while the motor is moving.  Then we have to do the following
in separate invocations of process():
        tell the motor to stop
        handle motor-in-motion callbacks while the motor slows down
        recognize the stopped-motor callback and begin jogging
        handle motor-in-motion callbacks while the motor jogs
        recognize when the user releases the jog button and tell the motor to stop
        handle motor-in-motion callbacks while the motor slows down
        recognize the stopped-motor callback and begin a backlash correction
        handle motor-in-motion callbacks while the motor is moving
        recognize the stopped-motor callback and fire off forward links
For this reason, a fair amount of code is devoted to keeping track of
where the motor is in a sequence of movements that comprise a single motion.

LOGIC:
    Initialize.
    IF this record is being processed by another task (i.e., PACT != 0).
        NORMAL RETURN.
    ENDIF
    Set Processing Active indicator field (PACT) true.
    Call device support update_values().
    IF motor status field (MSTA) was modified.
        Mark MSTA as changed.
    ENDIF
    IF function was invoked by a callback, OR, process delay acknowledged is true?
        Set process reason indicator to CALLBACK_DATA.
        Call process_motor_info().
        IF motor-in-motion indicator (MOVN) is true.
            Set the Done Moving field (DMOV) FALSE, and mark it as changed, if not already
            IF [New target monitoring is enabled], AND,
               [Sign of RDIF is NOT the same as sign of CDIR], AND,
               [|Dist. to target| > (NTMF x (|BDST| + RDBD)], AND,
               [MIP indicates this move is either (a result of a retry),OR,
                        (not from a Jog* or Hom*)], AND
               [Not already waiting for motor to stop]
                Send Stop Motor command.
                Set STOP indicator in MIP true.
                Mark MIP as changed.
            ENDIF
        ELSE
            IF Done Moving field is FALSE.
                Set Done Moving field (DMOV) TRUE and mark DMOV as changed.
            ENDIF
            IF the High or Low limit switch is TRUE.
                Set the Post Process field to TRUE.
            ENDIF
            IF the Post Process field is TRUE.
                IF target position has changed (VAL != LVAL).
                    Set MIP to DONE.
                ELSE
                    Call postProcess().
                ENDIF
            ENDIF
            IF a limit switch is activated, OR, a load-position command is in progress (MIP = MIP_LOAD_P)
                Set MIP to DONE and MARK it.
            ELSE IF Done Moving (DMOV) is TRUE
                IF process delay acknowledged is true, OR, ticks <= 0.
                    Clear process delay request and ack. indicators in MIP field.
                    Mark MIP as changed.
                    Call maybeRetry().
                ELSE
                    Set process delay request indicator true in MIP field.
                    Mark MIP as changed.
                    Start WatchDog?
                    Set the Done Moving field (DMOV) to FALSE.
                    Set Processing Active indicator field (PACT) false.
                    NORMAL RETURN.
                ENDIF
            ENDIF
        ENDIF
    ENDIF
    IF Software travel limits are disabled.
        Clear Limit violation field.
    ELSE
        IF Jog indicator is true in MIP field.
            Update Limit violation (LVIO) based on Jog direction (JOGF/JOGR) and velocity (JVEL).
        ELSE IF Homing indicator is true in MIP field.
            Set Limit violation (LVIO) FALSE.
        ENDIF
    ENDIF
    IF Limit violation (LVIO) has changed.
        Mark LVIO as changed.
        IF Limit violation (LVIO) is TRUE, AND, SET is false (i.e., Use/Set is Set).
            Set STOP field true.
            Clear jog and home requests.
        ENDIF
    ENDIF
    IF STOP field is true, OR,
       SPMG field Stop indicator is true, OR,
       SPMG field Pause indicator is true, OR,
       function was NOT invoked by a callback, OR,
       Done Moving field (DMOV) is TRUE, OR,
       RETRY indicator is true in MIP field.
        Call do_work().
    ENDIF
    Update Readback output link (RLNK), call dbPutLink().
Exit:
    Update record timestamp, call recGblGetTimeStamp().
    Process alarms, call alarm_sub().
    Monitor changes to record fields, call monitor().
 
    IF Done Moving field (DMOV) is TRUE
        Process the forward-scan-link record, call recGblFwdLink().
    ENDIF
    Set Processing Active indicator field (PACT) false.
    Exit.

*******************************************************************************/
static long process(dbCommon *arg)
{
    motorRecord *pmr = (motorRecord *) arg;
    long status = OK;
    CALLBACK_VALUE process_reason;
    int old_lvio = pmr->lvio;
    unsigned int old_msta = pmr->msta;
    struct motor_dset *pdset = (struct motor_dset *) (pmr->dset);
    struct callback *pcallback = (struct callback *) pmr->cbak; /* v3.2 */

    if (pmr->pact)
        return(OK);

    Debug(4, "%s:%d %s process:---------------------- begin\n",
          __FILE__, __LINE__, pmr->name);
    pmr->pact = 1;

    /*** Who called us? ***/
    /*
     * Call device support to get raw motor position/status and to see whether
     * this is a callback.
     */
    process_reason = (*pdset->update_values) (pmr);
    if (pmr->msta != old_msta)
        MARK(M_MSTA);

    if (process_reason == CALLBACK_NEWLIMITS)
    {
        if (pmr->priv->softLimitRO.motorDialLimitsValid)
        {
            double maxValue = pmr->priv->softLimitRO.motorDialHighLimitRO;
            double minValue = pmr->priv->softLimitRO.motorDialLowLimitRO;
            fprintf(stdout,
                    "%s:%d %s pmr->dhlm=%g maxValue=%g pmr->dllm=%g minValue=%g\n",
                    __FILE__, __LINE__, pmr->name,
                    pmr->dhlm, maxValue,
                    pmr->dllm, minValue);
            fflush(stdout);
            if (pmr->dllm < minValue) pmr->dllm = minValue;
            if (pmr->dhlm > maxValue) pmr->dhlm = maxValue;
        }
        process_reason = CALLBACK_DATA;
    }
    if (process_reason == CALLBACK_DATA)
    {
      if ((pmr->dol.type == CONSTANT) && pmr->udf)
	    pmr->udf = FALSE;
    }
    if ((process_reason == CALLBACK_DATA) || (pmr->mip & MIP_DELAY_ACK))
    {
        /*
         * This is, effectively, a callback from device support: a
         * motor-in-motion update, some asynchronous acknowledgement of a
         * command we sent in a previous life, or a callback thay we requested
         * to delay while readback device settled.
         */

        /*
         * If we were invoked by the readback-delay callback, then this is just
         * a continuation of the device-support callback.
         */
        process_reason = CALLBACK_DATA;

        /*
         * Get position and status from motor controller. Get readback-link
         * value if link exists.
         */
        process_motor_info(pmr, false);

        if (pmr->movn)
        {
            int sign_rdif = (pmr->rdif < 0) ? 0 : 1;
            double ntm_deadband =  pmr->ntmf * (fabs(pmr->bdst) + pmr->rdbd);
            bool move_or_retry;

            if ((pmr->mip & MIP_RETRY) != 0 || (pmr->mip & MIP_MOVE) != 0)
                move_or_retry = true;
            else
                move_or_retry = false;

            /* Since other sources can now initiate motor moves (e.g. Asyn
             * motors, written to by other records), make dmov track the state
             * of movn (inverted).
             */
            if (pmr->dmov) {
                pmr->dmov = FALSE;
                MARK(M_DMOV);
                MIP_SET_BIT(MIP_EXTERNAL);
                MARK(M_MIP);
                pmr->pp = TRUE;
            }

            /* Test for new target position in opposite direction of current
               motion. TB: This code needs review
             */
            if (pmr->ntm == menuYesNoYES &&
                (sign_rdif != pmr->cdir) &&
                (fabs(pmr->diff) > ntm_deadband) &&
                (move_or_retry == true) &&
                (pmr->mip & MIP_STOP) == 0)
            {

                /* We're going in the wrong direction. Readback problem? */
                fprintf(stdout, "%s:%d %s STOP tdir=%d\n",
                       __FILE__, __LINE__, pmr->name, pmr->tdir);

                devSupStop(pmr);
                MIP_SET_BIT(MIP_STOP);
                MARK(M_MIP);
                pmr->pp = FALSE; /* Don't post process the previous move. */
            }
            status = 0;
        }
        else if (pmr->stup != motorSTUP_BUSY)
        {
            mmap_field mmap_bits;

            /* Motor has stopped. */
            /* Assume we're done moving until we find out otherwise. */
            if (pmr->dmov == FALSE)
            {
#ifdef DEBUG
                {
                    char dbuf[MBLE];
                    dbgMipToString(pmr->mip, dbuf, sizeof(dbuf));
                    Debug(3, "%s:%d %s motor has stopped drbv=%f pp=%d mip=0x%0x(%s)\n",
                          __FILE__, __LINE__, pmr->name, pmr->drbv, pmr->pp, pmr->mip, dbuf);
                }
#endif
                pmr->dmov = TRUE;
                MARK(M_DMOV);
                if ((pmr->mip & ~MIP_JOG_REQ) == MIP_JOGF ||
		    (pmr->mip & ~MIP_JOG_REQ)== MIP_JOGR)
                {
                    /* Motor stopped while jogging and we didn't stop it */
                    MIP_SET_VAL(MIP_DONE);
                    MARK(M_MIP);
                    clear_buttons(pmr);
                    pmr->pp = TRUE;
                    goto process_exit;
                }
            }

            /* Do another update after LS error. */
            if (pmr->mip != MIP_DONE && ((pmr->rhls && pmr->cdir) || (pmr->rlls && !pmr->cdir)))
            {
                /* Restore DMOV to false and UNMARK it so it is not posted. */
                pmr->dmov = FALSE;
                UNMARK(M_DMOV);
                devSupGetInfo(pmr);
                pmr->pp = TRUE;
                MIP_SET_VAL(MIP_DONE);
                MARK(M_MIP);
                goto process_exit;
            }
            
            if (pmr->pp)
            {
                if ((pmr->val != pmr->priv->last.val) &&
                   !(pmr->mip & MIP_STOP)   &&
                   !(pmr->mip & MIP_JOG_STOP))
                {
                    MIP_SET_VAL(MIP_DONE);
                    /* Bug fix, record locks-up when BDST != 0, DLY != 0 and
                     * new target position before backlash correction move.*/
                    goto enter_do_work;
                }
                else
                    status = postProcess(pmr);
            }

            /* Should we test for a retry? Consider limit only if in direction of move.*/
            if (((pmr->rhls && pmr->cdir) || (pmr->rlls && !pmr->cdir)) || (pmr->mip == MIP_LOAD_P))
            {
                MIP_SET_VAL(MIP_DONE);
                MARK(M_MIP);
            }
            else if (pmr->dmov == TRUE)
            {
                mmap_bits.All = pmr->mmap; /* Initialize for MARKED. */

                if (pmr->mip & MIP_DELAY_ACK || (pmr->dly <= 0.0))
                {
#ifdef DEBUG
                    {
                        char dbuf[MBLE];
                        dbgMipToString(pmr->mip, dbuf, sizeof(dbuf));
                        Debug(3, "%s:%d %s (stopped) dmov==TRUE; no DLY; pp=%d mip=0x%0x(%s)\n",
                              __FILE__, __LINE__, pmr->name, pmr->pp, pmr->mip, dbuf);
                    }
#endif
                    if (pmr->mip & MIP_EXTERNAL) MIP_CLR_BIT(MIP_EXTERNAL);
                    if (pmr->mip & MIP_DELAY_ACK && !(pmr->mip & MIP_DELAY_REQ))
                    {
                        MIP_SET_BIT(MIP_DELAY);
                        devSupGetInfo(pmr);
                        /* Restore DMOV to false and UNMARK it so it is not posted. */
                        pmr->dmov = FALSE;
                        UNMARK(M_DMOV);
                        goto process_exit;
                    }
                    else if (pmr->stup != motorSTUP_ON && pmr->mip != MIP_DONE)
                    {
                        MIP_CLR_BIT(MIP_DELAY);
                        MARK(M_MIP);    /* done delaying */
                        maybeRetry(pmr);
                        if (pmr->mip == MIP_RETRY && pmr->rmod == motorRMOD_I)
                        {
                            MIP_SET_BIT(MIP_DELAY_REQ);
                            MARK(M_MIP);
                            Debug(3, "%s:%d %s callbackRequestDelayed() called\n\n",
                                  __FILE__, __LINE__, pmr->name);
                            callbackRequestDelayed(&pcallback->dly_callback, pmr->dly);
                        }
                    }
                }
                else if (MARKED(M_DMOV))
                {
                    if (!(pmr->mip & MIP_DELAY_REQ))
                    {
                        MIP_SET_BIT(MIP_DELAY_REQ);
                        MARK(M_MIP);
                        Debug(3, "%s:%d %s callbackRequestDelayed() called\n\n",
                              __FILE__, __LINE__, pmr->name);
                        callbackRequestDelayed(&pcallback->dly_callback, pmr->dly);
                    }

                    /* Restore DMOV to false and UNMARK it so it is not posted. */
                    pmr->dmov = FALSE;
                    UNMARK(M_DMOV);
                    goto process_exit;
                }
            }
        }
    }   /* END of (process_reason == CALLBACK_DATA). */

enter_do_work:

    /* check for soft-limit violation */
    if (!softLimitsDefined(pmr))
        pmr->lvio = false;
    else
    {
        if (pmr->mip & MIP_JOG)
            pmr->lvio = (pmr->jogf && (pmr->rbv > pmr->hlm - pmr->jvel)) ||
                        (pmr->jogr && (pmr->rbv < pmr->llm + pmr->jvel));
        else if (pmr->mip & MIP_HOME)
            pmr->lvio = false;  /* Disable soft-limit error check during home search. */
    }

    if (pmr->lvio != old_lvio)
    {
        MARK(M_LVIO);
        if (pmr->lvio && (!pmr->set && !pmr->igset))
        {
            pmr->stop = 1;
            MARK(M_STOP);
            clear_buttons(pmr);
            fprintf(stdout, "%s:%d %s STOP lvio\n",
                   __FILE__, __LINE__, pmr->name);
        }
    }

    /* Do we need to examine the record to figure out what work to perform? */
    if (pmr->stop || (pmr->spmg == motorSPMG_Stop) ||
        (pmr->spmg == motorSPMG_Pause) ||
        (process_reason != CALLBACK_DATA) || pmr->dmov || pmr->mip & MIP_RETRY)
    {
        status = do_work(pmr, process_reason);
    }

    /* Fire off readback link */
    status = dbPutLink(&(pmr->rlnk), DBR_DOUBLE, &(pmr->rbv), 1);
    
process_exit:
    if (process_reason == CALLBACK_DATA && pmr->stup == motorSTUP_BUSY)
    {
        pmr->stup = motorSTUP_OFF;
        MARK_AUX(M_STUP);
    }

    /*** We're done.  Report the current state of the motor. ***/
    recGblGetTimeStamp(pmr);
    alarm_sub(pmr);                     /* If we've violated alarm limits, yell. */
    monitor(pmr);               /* If values have changed, broadcast them. */

    if (pmr->dmov != 0)
        recGblFwdLink(pmr);                 /* Process the forward-scan-link record. */

    pmr->pact = 0;
    Debug(4, "%s:%d %s process:---------------------- end\n",
          __FILE__, __LINE__, pmr->name);
    return (status);
}


/*************************************************************************/
static int homing_wanted_and_allowed(motorRecord *pmr)
{
    int ret = 0;
    if (pmr->homf && !(pmr->mip & MIP_HOMF)) {
        ret = 1;
        if (pmr->mflg & MF_HOME_ON_LS)
           ; /* controller reported handle this fine */
        else if ((pmr->dir == motorDIR_Pos) ? pmr->hls : pmr->lls)
            ret = 0; /* sitting on the directed limit switch */
    }
    if (pmr->homr && !(pmr->mip & MIP_HOMR)) {
        ret = 1;
        if (pmr->mflg & MF_HOME_ON_LS)
           ; /* controller reported handle this fine */
        else if ((pmr->dir == motorDIR_Pos) ? pmr->lls : pmr->hls)
            ret = 0; /* sitting on the directed limit switch */
    }
    return ret;
}


/*************************************************************************/
static void doRetryOrDone(motorRecord *pmr, bool preferred_dir,
                          double relpos, double relbpos)
{
    double rbdst1 = fabs(pmr->bdst) + pmr->sdbd;
    bool use_rel;

    Debug(3, "%s:%d %s doRetryOrDone drbv=%f rcnt=%d preferred_dir=%d relpos=%f relbpos=%f drbv=%f\n",
          __FILE__, __LINE__, pmr->name, pmr->drbv, pmr->rcnt, preferred_dir,
          relpos, relbpos, pmr->drbv);

    /*** Use if encoder or ReadbackLink is in use. ***/
    if (pmr->rtry != 0 && pmr->rmod != motorRMOD_I && (pmr->ueip || pmr->urip))
        use_rel = true;
    else
        use_rel = false;

    if (fabs(relpos) < pmr->sdbd)
        relpos = (relpos > 0.0) ? pmr->sdbd : -pmr->sdbd;

    if (fabs(relbpos) < pmr->sdbd)
        relbpos = (relbpos > 0.0) ? pmr->sdbd : -pmr->sdbd;


    /* AJF fix for the bug where the retry count is not incremented when doing retries */
    /* This bug is seen when we use the readback link field                            */
    MIP_SET_BIT(MIP_MOVE);
    MARK(M_MIP);
    /* v1.96 Don't post dmov if special already did. */
    if (pmr->dmov)
    {
        pmr->dmov = FALSE;
        MARK(M_DMOV);
    }
    pmr->priv->last.dval = pmr->dval;
    pmr->priv->last.val = pmr->val;
    pmr->priv->last.rval = pmr->rval;

    /* Backlash disabled, OR, no need for seperate backlash move
     * since move is in preferred direction (preferred_dir==ON),
     * AND, backlash acceleration and velocity are the same as slew values
     * (BVEL == VELO, AND, BACC == ACCL). */
    if ((fabs(pmr->bdst) < pmr->sdbd) ||
        (preferred_dir == true && pmr->bvel == pmr->velo &&
         pmr->bacc == pmr->accl))
    {
        doMoveDialPosition(pmr, moveModePosition, pmr->drbv + relpos);
    }
    /* IF move is in preferred direction, AND, current position is within backlash range. */
    else if ((preferred_dir == true) &&
             ((use_rel == true  && relbpos <= pmr->sdbd) ||
              (use_rel == false && (fabs(pmr->dval - pmr->drbv) <= rbdst1))
             )
            )
    {
        doMoveDialPosition(pmr, moveModeBacklash, pmr->drbv + relpos);
    }
    else
    {
        doMoveDialPosition(pmr, moveModePosition, pmr->drbv + relbpos);
        pmr->pp = TRUE;              /* do backlash from posprocess(). */
    }
}

/*************************************************************************/
static void newMRES_ERES_UEIP(motorRecord *pmr)
{
    /* encoder pulses, motor pulses */
    double ep_mp[2];
    long m;
    msta_field msta;

    /* Set the encoder ratio.  Note this is blatantly device dependent. */
    msta.All = pmr->msta;
    if (msta.Bits.EA_PRESENT)
    {
        /* defend against divide by zero */
        if (fabs(pmr->mres) < 1.e-9)
        {
            pmr->mres = 1.;
            MARK(M_MRES);
        }
        if (pmr->eres == 0.0)
        {
            pmr->eres = pmr->mres;
            MARK(M_ERES);
        }
        /* Calculate encoder ratio. */
        for (m = 10000000; (m > 1) &&
             (fabs(m / pmr->eres) > 1.e6 || fabs(m / pmr->mres) > 1.e6); m /= 10);
        ep_mp[0] = fabs(m / pmr->eres);
        ep_mp[1] = fabs(m / pmr->mres);
    }
    else
    {
        ep_mp[0] = 1.;
        ep_mp[1] = 1.;
    }

    /* Make sure retry deadband is achievable */
    enforceMinRetryDeadband(pmr);

    if (msta.Bits.EA_PRESENT)
    {
        devSupSetEncRatio(pmr,ep_mp);
    }
    if (pmr->set)
    {
        pmr->pp = TRUE;
        devSupGetInfo(pmr);
    }
    else if ((pmr->mip & MIP_LOAD_P) == 0) /* Test for LOAD_POS completion. */
        load_pos(pmr);

}

/**********************************************************************/
static RTN_STATUS doDVALchangedOrNOTdoneMoving(motorRecord *pmr)
{
    int dir_positive = (pmr->dir == motorDIR_Pos);
    int dir = dir_positive ? 1 : -1;
    int old_lvio = pmr->lvio;
    bool too_small;
    bool preferred_dir = true;
    double diff = pmr->dval - pmr->drbv;
    double relpos = diff;
    double relbpos = ((pmr->dval - pmr->bdst) - pmr->drbv);
    double absdiff = fabs(diff);


    /*
     * Post new values, recalc .val to reflect the change in .dval. (We
     * no longer know the origin of the .dval change.  If user changed
     * .val, we're ok as we are, but if .dval was changed directly, we
     * must make .val agree.)
     */
    pmr->val = pmr->dval * dir + pmr->off;
    if (pmr->val != pmr->priv->last.val)
        MARK(M_VAL);
    pmr->rval = NINT(pmr->dval / pmr->mres);
    if (pmr->rval != pmr->priv->last.rval)
        MARK(M_RVAL);

    /* Don't move if we're within retry deadband. */

    too_small = false;
    if ((pmr->mip & MIP_RETRY) == 0)
    {
        if (absdiff < pmr->sdbd)
            too_small = true;
    }
    else if (absdiff < fabs(pmr->rdbd))
        too_small = true;

    if (pmr->miss)
    {
        pmr->miss = 0;
        MARK_AUX(M_MISS);
    }
    if (too_small == true)
    {
        if (pmr->dmov == FALSE && (pmr->mip == MIP_DONE || pmr->mip == MIP_RETRY))
        {
            pmr->dmov = TRUE;
            MARK(M_DMOV);
            if (pmr->mip != MIP_DONE)
            {
                MIP_SET_VAL(MIP_DONE);
                MARK(M_MIP);
            }
        }
        /* Update previous target positions. */
        pmr->priv->last.dval = pmr->dval;
        pmr->priv->last.val = pmr->val;
        pmr->priv->last.rval = pmr->rval;
        return(OK);
    }

    /* reset retry counter if this is not a retry */
    if ((pmr->mip & MIP_RETRY) == 0)
    {
        if (pmr->rcnt != 0)
            MARK(M_RCNT);
        pmr->rcnt = 0;
    }
    else if (pmr->rmod == motorRMOD_A) /* Do arthmetic sequence retries. */
    {
        double factor = (pmr->rtry - pmr->rcnt + 1.0) / pmr->rtry;
        relpos *= factor;
        relbpos *= factor;
    }
    else if (pmr->rmod == motorRMOD_G) /* Do geometric sequence retries. */
    {
        double factor;

        factor = 1 / pow(2.0, (pmr->rcnt - 1));
        relpos *= factor;
        relbpos *= factor;
    }
    else if (pmr->rmod == motorRMOD_I) /* DC motor like In-position retries. */
        return(OK);
    else if (pmr->rmod == motorRMOD_D) /* Do default, linear, retries. */
        ;
    else
        errPrintf(-1, __FILE__, __LINE__, "%s Invalid RMOD field value: = %d", pmr->name, pmr->rmod);

    /* No backlash distance: always preferred */
    if (pmr->bdst) {
        int newDir = diff > 0;
        if (newDir != (pmr->bdst > 0))
            preferred_dir = false;
    }
    /* Check for soft-travel limit violation */
    if (!softLimitsDefined(pmr))
        pmr->lvio = false;
    /* LVIO = TRUE, AND, Move request towards valid travel limit range. */
    else if (((pmr->dval > pmr->dhlm) && (pmr->dval < pmr->priv->last.dval)) ||
             ((pmr->dval < pmr->dllm) && (pmr->dval > pmr->priv->last.dval)))
        pmr->lvio = false;
    else
    {
        if (preferred_dir == true)
            pmr->lvio = ((pmr->dval > pmr->dhlm) || (pmr->dval < pmr->dllm));
        else
        {
            double bdstpos = pmr->dval - pmr->bdst;
            pmr->lvio = ((bdstpos > pmr->dhlm) || (bdstpos < pmr->dllm));
        }
    }

    if (pmr->lvio != old_lvio)
        MARK(M_LVIO);
    if (pmr->lvio)
    {
        pmr->val = pmr->priv->last.val;
        MARK(M_VAL);
        pmr->dval = pmr->priv->last.dval;
        MARK(M_DVAL);
        pmr->rval = pmr->priv->last.rval;
        MARK(M_RVAL);
        if ((pmr->mip & MIP_RETRY) != 0)
        {
            MIP_SET_VAL(MIP_DONE);
            MARK(M_MIP);
        }
        if (pmr->mip == MIP_DONE && pmr->dmov == FALSE)
        {
            pmr->dmov = TRUE;
            MARK(M_DMOV);
        }
        return(OK);
    }

    if (pmr->mip == MIP_DONE || pmr->mip == MIP_RETRY)
        doRetryOrDone(pmr, preferred_dir, relpos, relbpos);

    return(OK);
}

/******************************************************************************
        do_work()
Here, we do the real work of processing the motor record.

The equations that transform between user and dial coordinates follow.
Note: if user and dial coordinates differ in sign, we have to reverse the
sense of the limits in going between user and dial.

Dial to User:
userVAL = DialVAL * DIR + OFFset
userHLM = (DIR==+) ? DialHLM + OFFset : -DialLLM + OFFset
userLLM = (DIR==+) ? DialLLM + OFFset : -DialHLM + OFFset

User to Dial:
DialVAL = (userVAL - OFFset) / DIR
DialHLM = (DIR==+) ? userHLM - OFFset : -userLLM + OFFset
DialLLM = (DIR==+) ? userLLM - OFFset : -userHLM + OFFset

Offset:
OFFset  = userVAL - DialVAL * DIR

DEFINITIONS:
    preferred direction - the direction in which the motor moves during the
                            backlash-takeout part of a motor motion.
LOGIC:
    Initialize.

    IF Status Update request is YES.
        Send an INFO command.
    ENDIF

    IF Stop/Pause/Move/Go field has changed, OR, STOP field true.
        Update Last SPMG or clear STOP field.
        IF SPMG field set to STOP or PAUSE, OR, STOP was true.
            IF SPMG field set to STOP, OR, STOP was true.
                IF MIP state is DONE, STOP or RETRY.
                    Shouldn't be moving, but send a STOP command without
                        changing to the STOP state.
                    NORMAL RETURN.
                ELSE IF Motor is moving (MOVN).
                    Set Post process command TRUE. Clear Jog and Home requests.
                ELSE
                    Set VAL <- RBV and mark as changed.
                    Set DVAL <- DRBV and mark as changed.
                    Set RVAL <- RRBV and mark as changed.
                ENDIF
            ENDIF
            Clear any possible Home request.
            Set MIP field to indicate processing a STOP request.
            Mark MIP field changed.
            Send STOP_AXIS message to controller.
            NORMAL RETURN.
        ELSE IF SPMG field set to GO.
            IF either JOG request is true, AND, the corresponding limit is off.         
                Set MIP to JOG request (i.e., queue jog request).
            ELSE IF MIP state is STOP.
                Set MIP to DONE.
            ENDIF
        ELSE
            Clear MIP and RCNT. Mark both as changed.
        ENDIF
    ENDIF

    IF MRES, OR, ERES, OR, UEIP are marked as changed.
        IF UEIP set to YES, AND, MSTA indicates an encoder is present.
            IF |MRES| and/or |ERES| is very near zero.
                Set MRES and/or ERES to one (1.0).
            ENDIF
            Set sign of ERES to same sign as MRES.
            .....
            .....
        ELSE
            Set the [encoder (ticks) / motor (steps)] ratio to unity (1).
            Set RES <- MRES.
        ENDIF
        - call enforceMinRetryDeadband().
        IF MSTA indicates an encoder is present.
            Send the ticks/steps ratio motor command.
        ENDIF
        IF the SET position field is true.
            Set the PP field TRUE and send the update info. motor command.
        ELSE
            - call load_pos().
        ENDIF
        NORMAL RETURN
    ENDIF

    IF OMSL set to CLOSED_LOOP, AND, DOL type set to DB_LINK.
        Use DOL field to get DB link - call dbGetLink().
        IF error return from dbGetLink().
            Set Undefined Link indicator (UDF) TRUE.
            ERROR RETURN.
        ENDIF
        Set Undefined Link indicator (UDF) FALSE.
    ELSE
        IF Homing forward/OR/reverse request, AND, NOT processing Homing
            forward/OR/reverse, AND, NOT At High/OR/Low Limit Switch)
            IF (STOPPED, OR, PAUSED)
                Set DMOV FALSE (Home command will be processed from
                    postProcess() when SPMG is set to GO).
                NORMAL RETURN.
            ENDIF
 
            Set MIP based on HOMF or HOMR and MARK it.
            Set post process TRUE.
            IF Motor is moving (MOVN).
                Set MIP for STOP and MARK it.
                Send STOP command.
            ELSE
                Send Home velocity and acceleration commands.
                Send Home command.
                Set DMOV false and MARK it.
                Clear retry count (RCNT) and MARK it.
                Set commanded direction (CDIR) based on home direction MRES polarity.
            ENDIF
            NORMAL RETURN.
        ENDIF
        IF NOT currently jogging, AND, NOT (STOPPED, OR, PAUSED), AND, Jog Request is true.
            IF (Forward jog, AND, DVAL > [DHLM - VELO]), OR,
               (Reverse jog, AND, DVAL > [DLLM + VELO])
                Set limit violation (LVIO) true.
                NORMAL RETURN.
            ENDIF
            Set Jogging [forward/reverse] state true.
            ...
            ...
            NORMAL RETURN
        ENDIF
        IF Jog request is false, AND, jog is active.
            Set post process TRUE.
            Send STOP_AXIS message to controller.
        ELSE IF process jog stop or backlash.
            NORMAL RETURN.  NOTE: Don't want "DVAL has changed..." logic to
                            get processed.
        ENDIF
    ENDIF
    
    IF VAL field has changed.
        Mark VAL changed.
        IF the SET position field is true, AND, the FOFF field is "Variable".
            ....
        ELSE
            Calculate DVAL based on VAL, false and DIR.
        ENDIF
    ENDIF

    IF Software travel limits are disabled.
        Set LVIO false.
    ELSE
        Update LVIO field.
    ENDIF

    IF LVIO field has changed.
        Mark LVIO field.
    ENDIF

    IF Limit violation occurred.
        Restore VAL, DVAL and RVAL to previous, valid values.
        IF MIP state is RETRY
            Set MIP to DONE and mark as changes.
        ENDIF
        IF MIP state is DONE
            Set DMOV TRUE.
        ENDIF
    ENDIF

    IF Stop/Pause/Move/Go field set to STOP, OR, PAUSE.
        NORMAL RETURN.
    ENDIF

    IF DVAL field has changed, OR, NOT done moving.
        Mark DVAL as changed.
        Calculate new DIFF and RDIF fields and mark as changed.
        IF the SET position field is true.
            Load new raw motor position w/out moving it - call load_pos().
            NORMAL RETURN.
        ELSE
            Calculate....
            
            IF Retry enabled, AND, Retry mode is Not "In-Position", AND, [use encoder true, OR, use readback link true]
                Set relative positioning indicator true.
            ELSE
                Set relative positioning indicator false.
            ENDIF
            
            Set VAL and RVAL based on DVAL; mark VAL and RVAL for posting.
 
            Default too_small to FALSE.
            IF this is not a retry.
                IF the change in raw position < 1
                    Set too_small to TRUE.
                ENDIF
            ELSE IF the change in raw position < RDBD
                Set too_small to TRUE.
            ENDIF
 
            IF too_small is TRUE.
                IF not done moving, AND, [either no motion-in-progress, OR,
                                            retry-in-progress].
                    Set done moving TRUE.
                    NORMAL RETURN.
                    NOTE: maybeRetry() can send control here even though the
                        move is to the same raw position.
                ENDIF
                Restore previous target positions.
            ENDIF

            IF this is not a retry.
                Reset retry counter and mark RCNT for dbposting.
            ENDIF
            
            IF (relative move indicator is OFF, AND, sign of absolute move
                matches sign of backlash distance), OR, (relative move indicator
                is ON, AND, sign of relative move matches sign of backlash
                distance)
                Set preferred direction indicator ON.
            ELSE
                Set preferred direction indicator OFF.
            ENDIF
            
            IF the dial DIFF is within the retry deadband.
                IF MIP state is DONE.
                    Update last target positions.
                    Terminate move. Set DMOV TRUE.
                ENDIF               
                NORMAL RETURN.
            ENDIF
            ....
            ....
            IF motion in progress indicator is false.
                Set MIP MOVE indicator ON and mark for posting.
                IF DMOV is TRUE.
                    Set DMOV to FALSE and mark for posting.
                ENDIF
                Update last DVAL/VAL/RVAL.
                Initialize comm. transaction.
                IF backlash disabled, OR, no need for separate backlash move
                    since move is in preferred direction (preferred_dir==True),
                    AND, backlash acceleration and velocity are the same as
                    slew values (BVEL == VELO, AND, BACC == ACCL).
                    Initialize local velocity and acceleration variables to
                    slew values.
                    IF use relative positioning indicator is TRUE.
                        Set local position variable based on relative position.
                    ELSE
                        Set local position variable based on absolute position.
                    ENDIF
                ELSE IF move is in preferred direction (preferred_dir==True),
                        AND, |DVAL - LDVL| <= |BDST + MRES|.
                    Initialize local velocity and acceleration variables to
                    backlash values.
                    IF use relative positioning indicator is TRUE.
                        Set local position variable based on relative position.
                    ELSE
                        Set local position variable based on absolute position.
                    ENDIF
                ELSE
                    Initialize local velocity and acceleration variables to
                    slew values.
                    IF use relative positioning indicator is TRUE.
                        Set local position variable based on relative backlash position.
                    ELSE
                        Set local position variable based on absolute backlash position.
                    ENDIF
                    Set postprocess indicator TRUE so postprocess can do backlash.
                ENDIF            
                .....
                .....
                Send message to controller.
            ENDIF
        ENDIF
    ENDIF

    NORMAL RETURN.
    
    
*******************************************************************************/
static RTN_STATUS do_work(motorRecord * pmr, CALLBACK_VALUE proc_ind)
{
    int dir_positive = (pmr->dir == motorDIR_Pos);
    int dir = dir_positive ? 1 : -1;
    bool stop_or_pause = (pmr->spmg == motorSPMG_Stop ||
                             pmr->spmg == motorSPMG_Pause) ? true : false;
    mmap_field mmap_bits;

#ifdef DEBUG
    {
        char dbuf[MBLE];
        dbgMipToString(pmr->mip, dbuf, sizeof(dbuf));
        Debug(3, "%s:%d %s do_work: begin mip=0x%0x(%s)\n",
              __FILE__, __LINE__, pmr->name, pmr->mip, dbuf);
    }
#endif
    
    if (pmr->stup == motorSTUP_ON)
    {
        RTN_STATUS status;

        pmr->stup = motorSTUP_BUSY;
        MARK_AUX(M_STUP);
        status = devSupGetInfo(pmr);
        /* Avoid errors from devices that do not have "GET_INFO" (e.g. Soft
           Channel). */
        if (status == ERROR)
            pmr->stup = motorSTUP_OFF;
    }

    /*** Process Stop/Pause/Go_Pause/Go switch. ***
    *
    * STOP      means make the motor stop and, when it does, make the drive
    *       fields (e.g., .val) agree with the readback fields (e.g., .rbv)
    *       so the motor stays stopped until somebody gives it a new place
    *       to go and sets the switch to MOVE or GO.
    *
    * PAUSE     means stop the motor like the old steppermotorRecord stops
    *       a motor:  At the next call to process() the motor will continue
    *       moving to .val.
    *
    * MOVE      means Go to .val, but then wait for another explicit Go or
    *       Go_Pause before moving the motor, even if the .dval field
    *       changes.
    *
    * GO        means Go, and then respond to any field whose change causes
    *       .dval to change as if .dval had received a dbPut().
    *       (Implicit Go, as implemented in the old steppermotorRecord.)
    *       Note that a great many fields (.val, .rvl, .off, .twf, .homf,
    *       .jogf, etc.) can make .dval change.
    */
    if (pmr->spmg != pmr->lspg || pmr->stop != 0)
    {
        bool stop = (pmr->stop != 0) ? true : false;

        if (pmr->spmg != pmr->lspg)
            pmr->lspg = pmr->spmg;
        else
        {
            pmr->stop = 0;
            MARK(M_STOP);
        }

        if (stop_or_pause == true || stop == true)
        {
            /*
             * If STOP, make drive values agree with readback values (when the
             * motor actually stops).
             */
            if (pmr->spmg == motorSPMG_Stop || stop == true)
            {
                if ((pmr->mip == MIP_DONE) || (pmr->mip & MIP_STOP) ||
                    (pmr->mip & MIP_RETRY))
                {
                    if (pmr->mip & MIP_RETRY)
                    {
                        MIP_SET_VAL(MIP_DONE);
                        MARK(M_MIP);
                        pmr->dmov = TRUE;
                        MARK(M_DMOV);
                    }
                    /* Send message (just in case), but don't put MIP in STOP state. */
                    devSupStop(pmr);
                    return(OK);
                }
                else if (pmr->movn)
                {
                    pmr->pp = TRUE;     /* Do when motor stops. */
                    clear_buttons(pmr);
                }
                else
                    syncTargetPosition(pmr);   /* Synchronize target positions with readbacks. */
            }
            /* Cancel any operations. */
            if (pmr->mip & MIP_HOME)
                clear_buttons(pmr);

            if (!(pmr->mip & MIP_DELAY_REQ)) {
               /* When we wait for DLY, keep it. */
               /* Otherwise the record may lock up */
                MIP_SET_VAL(MIP_STOP);
                MARK(M_MIP);
            }
            devSupStop(pmr);
            return(OK);
        }
        else if (pmr->spmg == motorSPMG_Go)
        {
            /* Test for "queued" jog request. */
            if ((pmr->jogf && !pmr->hls) || (pmr->jogr && !pmr->lls))
            {
                MIP_SET_VAL(MIP_JOG_REQ);
                MARK(M_MIP);
            }
            else if (pmr->mip == MIP_STOP)
            {
                MIP_SET_VAL(MIP_DONE);
                MARK(M_MIP);
            }
        }
        else
        {
            MIP_SET_VAL(MIP_DONE);
            MARK(M_MIP);
            pmr->rcnt = 0;
            MARK(M_RCNT);
        }
    }

    /*** Handle changes in motor/encoder resolution, and in .ueip. ***/
    mmap_bits.All = pmr->mmap; /* Initialize for MARKED. */
    if (MARKED(M_MRES) || MARKED(M_ERES) || MARKED(M_UEIP))
    {
        newMRES_ERES_UEIP(pmr);
        return(OK);
    }
    /*** Collect .val (User value) changes from all sources. ***/
    if (pmr->omsl == menuOmslclosed_loop && pmr->dol.type == DB_LINK)
    {
        /** If we're in CLOSED_LOOP mode, get value from input link. **/
        long status;

        status = dbGetLink(&(pmr->dol), DBR_DOUBLE, &(pmr->val), 0, 0);
        if (!RTN_SUCCESS(status))
        {
            pmr->udf = TRUE;
            return(ERROR);
        }
        pmr->udf = FALSE;
        /* Later, we'll act on this new value of .val. */
    }
    else
    {
        /** Check out all the buttons and other sources of motion **/

        /* Send motor to home switch in wanted direction. */
        if (homing_wanted_and_allowed(pmr))
        {
            if (stop_or_pause == true)
            {
                pmr->dmov = FALSE;
                MARK(M_DMOV);
                return(OK);
            }

            MIP_SET_VAL(pmr->homf ? MIP_HOMF : MIP_HOMR);
            MARK(M_MIP);
            pmr->pp = TRUE;
            if (pmr->movn)
            {
                MIP_SET_BIT(MIP_STOP);
                MARK(M_MIP);
                devSupStop(pmr);
            }
            else
            {
                /* defend against divide by zero */
                if (pmr->eres == 0.0)
                {
                    pmr->eres = pmr->mres;
                    MARK(M_ERES);
                }
                doHomeSetcdir(pmr);
                pmr->dmov = FALSE;
                MARK(M_DMOV);
                pmr->rcnt = 0;
                MARK(M_RCNT);
            }
            return(OK);
        }
        /*
         * Jog motor.  Move continuously until we hit a software limit or a
         * limit switch, or until user releases button.
         */
        if (!(pmr->mip & MIP_JOG) && stop_or_pause == false &&
            (pmr->mip & MIP_JOG_REQ))
        {
            /* check for limit violation */
            if (!softLimitsDefined(pmr))
                ;
            else if ((pmr->jogf && (pmr->val > pmr->hlm - pmr->jvel)) ||
                     (pmr->jogr && (pmr->val < pmr->llm + pmr->jvel)))
            {
                pmr->lvio = 1;
                MARK(M_LVIO);
                return(OK);
            }
            MIP_SET_VAL(pmr->jogf ? MIP_JOGF : MIP_JOGR);
            MARK(M_MIP);
            if (pmr->movn)
            {
                pmr->pp = TRUE;
                MIP_SET_BIT(MIP_STOP);
                MARK(M_MIP);
                devSupStop(pmr);
            }
            else
            {
                double jogv = pmr->jvel * dir;
                pmr->dmov = FALSE;
                MARK(M_DMOV);
                pmr->pp = TRUE;
                if (pmr->jogr)
                {
                    jogv = -jogv;
                }
                devSupJogDial(pmr, jogv, pmr->jar);
            }
            return(OK);
        }
        /* Stop jogging. */
        if (((pmr->mip & MIP_JOG_REQ) == 0) && 
            ((pmr->mip & MIP_JOGF) || (pmr->mip & MIP_JOGR)))
        {
            /* Stop motor.  When stopped, process() will correct backlash. */
            pmr->pp = TRUE;
            MIP_SET_BIT(MIP_JOG_STOP);
            MIP_CLR_BIT(MIP_JOGF | MIP_JOGR);
            fprintf(stdout, "%s:%d %s STOP jogging\n",
                   __FILE__, __LINE__, pmr->name);
            devSupStop(pmr);
            return(OK);
        }
        else if (pmr->mip & (MIP_JOG_STOP | MIP_JOG_BL1 | MIP_JOG_BL2))
            return(OK); /* Normal return if process jog stop or backlash. */

        /*
         * Tweak motor forward (reverse).  Increment motor's position by a
         * value stored in pmr->twv.
         */
        if (pmr->twf || pmr->twr)
        {
            pmr->val += pmr->twv * (pmr->twf ? 1 : -1);
            /* Later, we'll act on this. */
            if (pmr->twf)
            {
                pmr->twf = 0;
                db_post_events(pmr, &pmr->twf, DBE_VAL_LOG);
            }
            if (pmr->twr)
            {
                pmr->twr = 0;
                db_post_events(pmr, &pmr->twr, DBE_VAL_LOG);
            }
        }
        /*
         * New relative value.  Someone has poked a value into the "move
         * relative" field (just like the .val field, but relative instead of
         * absolute.)
         */
        if (pmr->rlv != pmr->priv->last.rlv)
        {
            pmr->val += pmr->rlv;
            /* Later, we'll act on this. */
            pmr->rlv = 0.;
            MARK(M_RLV);
            pmr->priv->last.rlv = pmr->rlv;
        }
        /* New raw value.  Propagate to .dval and act later. */
        if (pmr->rval != pmr->priv->last.rval)
            pmr->dval = pmr->rval * pmr->mres;  /* Later, we'll act on this. */
    }

    /*** Collect .dval (Dial value) changes from all sources. ***
    * Now we either act directly on the .val change and return, or we
    * propagate it into a .dval change.
    */
    if (pmr->val != pmr->priv->last.val)
    {
        MARK(M_VAL);
        if ((pmr->set && !pmr->igset) && !pmr->foff)
        {
            /*
             * Act directly on .val. and return. User wants to redefine .val
             * without moving the motor and without making a change to .dval.
             * Adjust the offset and recalc user limits back into agreement
             * with dial limits.
             */
            pmr->off = pmr->val - pmr->dval * dir;
            pmr->rbv = pmr->drbv * dir + pmr->off;
            MARK(M_OFF);
            MARK(M_RBV);

            set_userlimits(pmr);        /* Translate dial limits to user limits. */

            pmr->priv->last.val = pmr->val;
            MIP_SET_VAL(MIP_DONE);
            MARK(M_MIP);
            pmr->dmov = TRUE;
            MARK(M_DMOV);
            return(OK);
        }
        else
            /*
             * User wants to move the motor, or to recalibrate both user and
             * dial.  Propagate .val to .dval.
             */
            pmr->dval = (pmr->val - pmr->off) / dir;    /* Later we'll act on this. */      
    }

    if (stop_or_pause == true)
        return(OK);
    
    /* IF DVAL field has changed, OR, NOT done moving. */
    if (pmr->dval != pmr->priv->last.dval || !pmr->dmov)
    {
        if (pmr->dval != pmr->priv->last.dval)
            MARK(M_DVAL);
        if (pmr->set)
        {
            if ((pmr->mip & MIP_LOAD_P) == 0) /* Test for LOAD_POS completion. */
                load_pos(pmr);
            /* device support will call us back when load is done. */
            return(OK);
        }
        return doDVALchangedOrNOTdoneMoving(pmr);
    } /* fast STOP follwoed by SYNC: Wait for STUP to be done */
    else if (pmr->sync && pmr->stup == motorSTUP_OFF && pmr->mip == MIP_DONE)
    {
        syncTargetPosition(pmr); /* Sync target positions with readbacks. */
        pmr->sync = 0;
        db_post_events(pmr, &pmr->sync, DBE_VAL_LOG);
    }
    else if (proc_ind == NOTHING_DONE && pmr->stup == motorSTUP_OFF)
    {
        RTN_STATUS status;
        
        pmr->stup = motorSTUP_BUSY;
        MARK_AUX(M_STUP);
        status = devSupGetInfo(pmr);
        if (status == ERROR)
            pmr->stup = motorSTUP_OFF;
    }

    return(OK);
}


/******************************************************************************
        special()
*******************************************************************************/
static long special(DBADDR *paddr, int after)
{
    motorRecord *pmr = (motorRecord *) paddr->precord;
    int dir_positive = (pmr->dir == motorDIR_Pos);
    int dir = dir_positive ? 1 : -1;
    bool changed = false;
    int fieldIndex = dbGetFieldIndex(paddr);
    double fabs_urev;
    motor_cmnd command;
    double temp_dbl;
    double *pcoeff;
    msta_field msta;

    msta.All = pmr->msta;

    Debug(3, "%s:%d %s special fieldIndex=%s (%d)  after=%d\n",
          __FILE__, __LINE__, pmr->name,
          fieldIndexToStr(fieldIndex), fieldIndex, after);

    /*
     * Someone wrote to drive field.  Blink .dmov unless record is disabled.
     */
    if (after == 0)
    {
        switch (fieldIndex)
        {
            case motorRecordVAL:
            case motorRecordDVAL:
            case motorRecordRVAL:
            case motorRecordRLV:
            case motorRecordTWF:
            case motorRecordTWR:
                if (pmr->disa == pmr->disv || pmr->disp)
                    return(OK);
                if (pmr->dmov == TRUE)
                {
                    pmr->dmov = FALSE;
                    db_post_events(pmr, &pmr->dmov, DBE_VAL_LOG);
                }
                return(OK);

            case motorRecordHOMF:
            case motorRecordHOMR:
                if (pmr->mip & MIP_HOME)
                    return(ERROR);      /* Prevent record processing. */
                break;
            case motorRecordSTUP:
                if (pmr->stup != motorSTUP_OFF)
                    return(ERROR);      /* Prevent record processing. */
        }
        return(OK);
    }

    fabs_urev = fabs(pmr->urev);

    switch (fieldIndex)
    {
        /* new vbas: make sbas agree */
    case motorRecordVBAS:
        if (pmr->vbas < 0.0)
        {
            pmr->vbas = 0.0;        
            db_post_events(pmr, &pmr->vbas, DBE_VAL_LOG);
        }

        if ((pmr->urev != 0.0) && (pmr->sbas != (temp_dbl = pmr->vbas / fabs_urev)))
        {
            pmr->sbas = temp_dbl;
            db_post_events(pmr, &pmr->sbas, DBE_VAL_LOG);
        }
        break;

        /* new sbas: make vbas agree */
    case motorRecordSBAS:
        if (pmr->sbas < 0.0)
        {
            pmr->sbas = 0.0;
            db_post_events(pmr, &pmr->sbas, DBE_VAL_LOG);
        }

        if (pmr->vbas != (temp_dbl = fabs_urev * pmr->sbas))
        {
            pmr->vbas = temp_dbl;
            db_post_events(pmr, &pmr->vbas, DBE_VAL_LOG);
        }
        break;

        /* new vmax: check against controller value and make smax agree */
    case motorRecordVMAX:
        if (pmr->vmax < 0.0)
        {
            pmr->vmax = pmr->priv->configRO.motorMaxVelocityDial;
            db_post_events(pmr, &pmr->vmax, DBE_VAL_LOG);
        }

        range_check(pmr, &pmr->vmax, 0.0, pmr->priv->configRO.motorMaxVelocityDial);

        if ((pmr->urev != 0.0) && (pmr->smax != (temp_dbl = pmr->vmax / fabs_urev)))
        {
            pmr->smax = temp_dbl;
            db_post_events(pmr, &pmr->smax, DBE_VAL_LOG);
        }
        break;

        /* new smax: make vmax agree */
    case motorRecordSMAX:
        if (pmr->smax < 0.0)
        {
            pmr->smax = 0.0;
            db_post_events(pmr, &pmr->smax, DBE_VAL_LOG);
        }

        if (pmr->vmax != (temp_dbl = fabs_urev * pmr->smax))
        {
            pmr->vmax = temp_dbl;
            db_post_events(pmr, &pmr->vmax, DBE_VAL_LOG);
        }
        break;

        /* new velo: make s agree */
    case motorRecordVELO:
        range_check(pmr, &pmr->velo, pmr->vbas, pmr->vmax);

        if ((pmr->urev != 0.0) && (pmr->s != (temp_dbl = pmr->velo / fabs_urev)))
        {
            pmr->s = temp_dbl;
            db_post_events(pmr, &pmr->s, DBE_VAL_LOG);
        }
        break;

        /* new s: make velo agree */
    case motorRecordS:
        range_check(pmr, &pmr->s, pmr->sbas, pmr->smax);

        if (pmr->velo != (temp_dbl = fabs_urev * pmr->s))
        {
            pmr->velo = temp_dbl;
            db_post_events(pmr, &pmr->velo, DBE_VAL_LOG);
        }
        break;

        /* new bvel: make sbak agree */
    case motorRecordBVEL:
        range_check(pmr, &pmr->bvel, pmr->vbas, pmr->vmax);

        if ((pmr->urev != 0.0) && (pmr->sbak != (temp_dbl = pmr->bvel / fabs_urev)))
        {
            pmr->sbak = temp_dbl;
            db_post_events(pmr, &pmr->sbak, DBE_VAL_LOG);
        }
        break;

        /* new sbak: make bvel agree */
    case motorRecordSBAK:
        range_check(pmr, &pmr->sbak, pmr->sbas, pmr->smax);

        if (pmr->bvel != (temp_dbl = fabs_urev * pmr->sbak))
        {
            pmr->bvel = temp_dbl;
            db_post_events(pmr, &pmr->bvel, DBE_VAL_LOG);
        }
        break;

        /* new accl */
    case motorRecordACCL:
        if (pmr->accl <= 0.0)
        {
            pmr->accl = 0.1;
            db_post_events(pmr, &pmr->accl, DBE_VAL_LOG);
        }
        break;

        /* new bacc */
    case motorRecordBACC:
        if (pmr->bacc <= 0.0)
        {
            pmr->bacc = 0.1;
            db_post_events(pmr, &pmr->bacc, DBE_VAL_LOG);
        }
        break;

        /* new sdbd ot rdbd */
    case motorRecordSDBD:
    case motorRecordRDBD:
        enforceMinRetryDeadband(pmr);
        break;

        /* new dir */
    case motorRecordDIR:
        if (pmr->foff)
        {
            pmr->val = pmr->dval * dir + pmr->off;
            MARK(M_VAL);
        }
        else
        {
            pmr->off = pmr->val - pmr->dval * dir;
            MARK(M_OFF);
        }
        pmr->rbv = pmr->drbv * dir + pmr->off;
        MARK(M_RBV);
        set_userlimits(pmr);    /* Translate dial limits to user limits. */
        break;

        /* new offset */
    case motorRecordOFF:
        pmr->val = pmr->dval * dir + pmr->off;
        pmr->priv->last.val = pmr->priv->last.dval * dir + pmr->off;
        pmr->rbv = pmr->drbv * dir + pmr->off;
        MARK(M_VAL);
        MARK(M_RBV);
        set_userlimits(pmr);    /* Translate dial limits to user limits. */
        break;

        /* new user high limit */
    case motorRecordHLM:
        set_user_highlimit(pmr);
        break;

        /* new user low limit */
    case motorRecordLLM:
        set_user_lowlimit(pmr);
        break;

        /* new dial high limit */
    case motorRecordDHLM:
        set_dial_highlimit(pmr);
        break;

        /* new dial low limit */
    case motorRecordDLLM:
        set_dial_lowlimit(pmr);
        break;

        /* new frac (move fraction) */
    case motorRecordFRAC:
        /* enforce limit */
        if (pmr->frac < 0.1)
        {
            pmr->frac = 0.1;
            changed = true;
        }
        if (pmr->frac > 1.5)
        {
            pmr->frac = 1.5;
            changed = true;
        }
        if (changed == true)
            db_post_events(pmr, &pmr->frac, DBE_VAL_LOG);
        break;

        /* new mres: make urev agree, and change (velo,bvel,vbas) to leave */
        /* (s,sbak,sbas) constant */
    case motorRecordMRES:
        MARK(M_MRES);           /* MARK it so we'll remember to tell device
                                 * support */
        if (pmr->urev != (temp_dbl = pmr->mres * pmr->srev))
        {
            pmr->urev = temp_dbl;
            fabs_urev = fabs(pmr->urev);        /* Update local |UREV|. */
            MARK_AUX(M_UREV);
        }
        goto velcheckB;

        /* new urev: make mres agree, and change (velo,bvel,vbas) to leave */
        /* (s,sbak,sbas) constant */

    case motorRecordUREV:
        if (pmr->mres != (temp_dbl = pmr->urev / pmr->srev))
        {
            pmr->mres = temp_dbl;
            MARK(M_MRES);
        }

velcheckB:
        if (pmr->velo != (temp_dbl = fabs_urev * pmr->s))
        {
            pmr->velo = temp_dbl;
            MARK_AUX(M_VELO);
        }
        if (pmr->vbas != (temp_dbl = fabs_urev * pmr->sbas))
        {
            pmr->vbas = temp_dbl;
            MARK_AUX(M_VBAS);
        }
        if (pmr->bvel != (temp_dbl = fabs_urev * pmr->sbak))
        {
            pmr->bvel = temp_dbl;
            db_post_events(pmr, &pmr->bvel, DBE_VAL_LOG);
        }
        if (pmr->vmax != (temp_dbl = fabs_urev * pmr->smax))
        {
            pmr->vmax = temp_dbl;
            db_post_events(pmr, &pmr->vmax, DBE_VAL_LOG);
        }
        break;

        /* new srev: make mres agree */
    case motorRecordSREV:
        if (pmr->srev <= 0)
        {
            pmr->srev = 200;
            MARK_AUX(M_SREV);
        }
        if (pmr->mres != pmr->urev / pmr->srev)
        {
            pmr->mres = pmr->urev / pmr->srev;
            MARK(M_MRES);
        }
        break;

        /* new eres (encoder resolution) */
    case motorRecordERES:
        if (pmr->eres == 0.0)   /* Don't allow ERES = 0. */
            pmr->eres = pmr->mres;
        MARK(M_ERES);
        break;

        /* new ueip flag */
    case motorRecordUEIP:
        if (pmr->ueip == motorUEIP_Yes)
        {
            if (msta.Bits.EA_PRESENT)
            {
                if (pmr->urip == motorUEIP_Yes)
                {
                    pmr->urip = motorUEIP_No;   /* Set URIP = No, if UEIP = Yes. */
                    db_post_events(pmr, &pmr->urip, DBE_VAL_LOG);
                }
            }
            else
            {
                pmr->ueip = motorUEIP_No;       /* Override UEIP = Yes if EA_PRESENT is false. */
                MARK(M_UEIP);
            }
        }
        break; 

        /* new urip flag */
    case motorRecordURIP:
        if ((pmr->urip == motorUEIP_Yes) && (pmr->ueip == motorUEIP_Yes))
        {
            pmr->ueip = motorUEIP_No;           /* Set UEIP = No, if URIP = Yes. */
            MARK(M_UEIP);
        }
        break; 

        /* Set to SET mode  */
    case motorRecordSSET:
        pmr->set = 1;
        db_post_events(pmr, &pmr->set, DBE_VAL_LOG);
        break;

        /* Set to USE mode  */
    case motorRecordSUSE:
        pmr->set = 0;
        db_post_events(pmr, &pmr->set, DBE_VAL_LOG);
        break;

        /* Set freeze-offset to freeze mode */
    case motorRecordFOF:
        pmr->foff = 1;
        db_post_events(pmr, &pmr->foff, DBE_VAL_LOG);
        break;

        /* Set freeze-offset to variable mode */
    case motorRecordVOF:
        pmr->foff = 0;
        db_post_events(pmr, &pmr->foff, DBE_VAL_LOG);
        break;

        /* New backlash distance.  Make sure retry deadband is achievable. */
    case motorRecordBDST:
        enforceMinRetryDeadband(pmr);
        break;

    case motorRecordPCOF:
        pcoeff = &pmr->pcof;
        command = SET_PGAIN;
        goto pidcof;
    case motorRecordICOF:
        pcoeff = &pmr->icof;
        command = SET_IGAIN;
        goto pidcof;
    case motorRecordDCOF:
        pcoeff = &pmr->dcof;
        command = SET_DGAIN;
pidcof:
        if (msta.Bits.GAIN_SUPPORT != 0)
        {
            if (*pcoeff < 0.0)  /* Validity check;  0.0 <= gain <= 1.0 */
            {
                *pcoeff = 0.0;
                changed = true;
            }
            else if (*pcoeff > 1.0)
            {
                *pcoeff = 1.0;
                changed = true;
            }
            if (devSupSetPID(pmr, command, pcoeff))
                changed = true;
            if (changed )
                db_post_events(pmr, pcoeff, DBE_VAL_LOG);
        }
        break;

    case motorRecordCNEN:
        if (msta.Bits.GAIN_SUPPORT != 0)
        {
            devSupCNEN(pmr, pmr->cnen);
        }
        break;

    case motorRecordJOGF:
        if (pmr->jogf == 0)
            MIP_CLR_BIT(MIP_JOG_REQ);
        else if (pmr->mip == MIP_DONE && !pmr->hls)
	    MIP_SET_BIT(MIP_JOG_REQ);
        break;

    case motorRecordJOGR:
        if (pmr->jogr == 0)
            MIP_CLR_BIT(MIP_JOG_REQ);
        else if (pmr->mip == MIP_DONE && !pmr->lls)
            MIP_SET_BIT(MIP_JOG_REQ);
        break;

    case motorRecordJVEL:
        range_check(pmr, &pmr->jvel, pmr->vbas, pmr->vmax);

        if ((pmr->mip & MIP_JOGF) || (pmr->mip & MIP_JOGR))
        {
            double jogv = (pmr->jvel * dir) / pmr->mres;
            double jacc = pmr->jar / fabs(pmr->mres);
            if (pmr->jogr)
                jogv = -jogv;

            devSupUpdateJogRaw(pmr, jogv, jacc);
        }
        break;

    case motorRecordJAR:
        // Valid JAR; 0 < JAR < JVEL [egu / sec] / 0.1 [sec]
        if (pmr->jar <= 0.0)
            pmr->jar = pmr->jvel / 0.1;
        break;

    case motorRecordHVEL:
        range_check(pmr, &pmr->hvel, pmr->vbas, pmr->vmax);
        break;

    case motorRecordSTUP:
        if (pmr->stup != motorSTUP_ON)
        {
            pmr->stup = motorSTUP_OFF;
            db_post_events(pmr, &pmr->stup, DBE_VAL_LOG);
            return(ERROR);      /* Prevent record processing. */
        }
        break;

    case motorRecordNTMF:
        if (pmr->ntmf < 2)
        {
            pmr->ntmf = 2;
            db_post_events(pmr, &pmr->ntmf, DBE_VAL_LOG);
            return(ERROR);      /* Prevent record processing. */
        }
        break;

    default:
        break;
    }

    switch (fieldIndex) /* Re-check slew (VBAS) and backlash (VBAS) velocities. */
    {
        case motorRecordVMAX:
        case motorRecordSMAX:
            if (pmr->vmax != 0.0 && pmr->vmax < pmr->vbas)
            {
                pmr->vbas = pmr->vmax;
                MARK_AUX(M_VBAS);
                pmr->sbas = pmr->smax;
                MARK_AUX(M_SBAS);
            }
            goto velcheckA;

        case motorRecordVBAS:
        case motorRecordSBAS:
            if (pmr->vmax != 0.0 && pmr->vbas > pmr->vmax)
            {
                pmr->vmax = pmr->vbas;
                db_post_events(pmr, &pmr->vmax, DBE_VAL_LOG);
                pmr->smax = pmr->sbas;
                db_post_events(pmr, &pmr->smax, DBE_VAL_LOG);
            }
velcheckA:
            range_check(pmr, &pmr->velo, pmr->vbas, pmr->vmax);
    
            if ((pmr->urev != 0.0) && (pmr->s != (temp_dbl = pmr->velo / fabs_urev)))
            {
                pmr->s = temp_dbl;
                db_post_events(pmr, &pmr->s, DBE_VAL_LOG);
            }
    
            range_check(pmr, &pmr->bvel, pmr->vbas, pmr->vmax);
    
            if ((pmr->urev != 0.0) && (pmr->sbak != (temp_dbl = pmr->bvel / fabs_urev)))
            {
                pmr->sbak = temp_dbl;
                db_post_events(pmr, &pmr->sbak, DBE_VAL_LOG);
            }

            range_check(pmr, &pmr->jvel, pmr->vbas, pmr->vmax);
            range_check(pmr, &pmr->hvel, pmr->vbas, pmr->vmax);
    }
    /* Do not process (i.e., clear) marked fields here.  PP fields (e.g., MRES) must remain marked. */
    return(OK);
}


/******************************************************************************
        get_units()
*******************************************************************************/
static long get_units(const DBADDR *paddr, char *units)
{
    motorRecord *pmr = (motorRecord *) paddr->precord;
    int siz = dbr_units_size - 1;       /* "dbr_units_size" from dbAccess.h */
    char s[30];
    int fieldIndex = dbGetFieldIndex(paddr);

    switch (fieldIndex)
    {

    case motorRecordVELO:
    case motorRecordVMAX:
    case motorRecordBVEL:
    case motorRecordVBAS:
    case motorRecordJVEL:
    case motorRecordHVEL:
        strncpy(s, pmr->egu, DB_UNITS_SIZE);
        strcat(s, "/sec");
        break;

    case motorRecordJAR:
        strncpy(s, pmr->egu, DB_UNITS_SIZE);
        strcat(s, "/s/s");
        break;

    case motorRecordACCL:
    case motorRecordBACC:
        strcpy(s, "sec");
        break;

    case motorRecordS:
    case motorRecordSBAS:
    case motorRecordSBAK:
        strcpy(s, "rev/sec");
        break;

    case motorRecordSREV:
        strcpy(s, "steps/rev");
        break;

    case motorRecordUREV:
        strncpy(s, pmr->egu, DB_UNITS_SIZE);
        strcat(s, "/rev");
        break;

    default:
        strncpy(s, pmr->egu, DB_UNITS_SIZE);
        break;
    }
    s[siz] = '\0';
    strncpy(units, s, siz + 1);
    return (0);
}

/******************************************************************************
        get_graphic_double()
*******************************************************************************/
static long get_graphic_double(const DBADDR *paddr, struct dbr_grDouble * pgd)
{
    motorRecord *pmr = (motorRecord *) paddr->precord;
    int fieldIndex = dbGetFieldIndex(paddr);

    switch (fieldIndex)
    {

    case motorRecordVAL:
    case motorRecordRBV:
        pgd->upper_disp_limit = pmr->hlm;
        pgd->lower_disp_limit = pmr->llm;
        break;

    case motorRecordDVAL:
    case motorRecordDRBV:
        pgd->upper_disp_limit = pmr->dhlm;
        pgd->lower_disp_limit = pmr->dllm;
        break;

    case motorRecordRVAL:
    case motorRecordRRBV:
        if (pmr->mres >= 0)
        {
            pgd->upper_disp_limit = pmr->dhlm / pmr->mres;
            pgd->lower_disp_limit = pmr->dllm / pmr->mres;
        }
        else
        {
            pgd->upper_disp_limit = pmr->dllm / pmr->mres;
            pgd->lower_disp_limit = pmr->dhlm / pmr->mres;
        }
        break;

    case motorRecordVELO:
        pgd->upper_disp_limit = pmr->vmax;
        pgd->lower_disp_limit = pmr->vbas;
        break;

    default:
        recGblGetGraphicDouble((dbAddr *) paddr, pgd);
        break;
    }

    return (0);
}

/******************************************************************************
        get_control_double()
*******************************************************************************/
static long
 get_control_double(const DBADDR *paddr, struct dbr_ctrlDouble * pcd)
{
    motorRecord *pmr = (motorRecord *) paddr->precord;
    int fieldIndex = dbGetFieldIndex(paddr);

    switch (fieldIndex)
    {

    case motorRecordVAL:
    case motorRecordRBV:
        pcd->upper_ctrl_limit = pmr->hlm;
        pcd->lower_ctrl_limit = pmr->llm;
        break;

    case motorRecordDVAL:
    case motorRecordDRBV:
        pcd->upper_ctrl_limit = pmr->dhlm;
        pcd->lower_ctrl_limit = pmr->dllm;
        break;

    case motorRecordRVAL:
    case motorRecordRRBV:
        if (pmr->mres >= 0)
        {
            pcd->upper_ctrl_limit = pmr->dhlm / pmr->mres;
            pcd->lower_ctrl_limit = pmr->dllm / pmr->mres;
        }
        else
        {
            pcd->upper_ctrl_limit = pmr->dllm / pmr->mres;
            pcd->lower_ctrl_limit = pmr->dhlm / pmr->mres;
        }
        break;

    case motorRecordVELO:
        pcd->upper_ctrl_limit = pmr->vmax;
        pcd->lower_ctrl_limit = pmr->vbas;
        break;

    default:
        recGblGetControlDouble((dbAddr *) paddr, pcd);
        break;
    }
    return (0);
}

/******************************************************************************
        get_precision()
*******************************************************************************/
static long get_precision(const DBADDR *paddr, long *precision)
{
    motorRecord *pmr = (motorRecord *) paddr->precord;
    int fieldIndex = dbGetFieldIndex(paddr);

    *precision = pmr->prec;
    switch (fieldIndex)
    {

    case motorRecordRRBV:
    case motorRecordRMP:
    case motorRecordREP:
        *precision = 0;
        break;

    case motorRecordVERS:
        *precision = 2;
        break;

    default:
        recGblGetPrec((dbAddr *) paddr, precision);
        break;
    }
    return (0);
}



/******************************************************************************
        get_alarm_double()
*******************************************************************************/
static long get_alarm_double(const DBADDR  *paddr, struct dbr_alDouble * pad)
{
    motorRecord *pmr = (motorRecord *) paddr->precord;
    int fieldIndex = dbGetFieldIndex(paddr);

    if (fieldIndex == motorRecordVAL || fieldIndex == motorRecordDVAL)
    {
        pad->upper_alarm_limit = pmr->hihi;
        pad->upper_warning_limit = pmr->high;
        pad->lower_warning_limit = pmr->low;
        pad->lower_alarm_limit = pmr->lolo;
    }
    else
    {
        recGblGetAlarmDouble((dbAddr *) paddr, pad);
    }
    return (0);
}


/******************************************************************************
        alarm_sub()
*******************************************************************************/
static void alarm_sub(motorRecord * pmr)
{
    msta_field msta;

    if (pmr->udf == TRUE)
    {
        recGblSetSevr((dbCommon *) pmr, UDF_ALARM, INVALID_ALARM);
        return;
    }
    /* Limit-switch and soft-limit violations. Consider limit switches also if not in
     * direction of move (limit hit by externally triggered move)*/
    if (pmr->hlsv && (pmr->hls || (pmr->dval > pmr->dhlm)))
    {
        recGblSetSevr((dbCommon *) pmr, HIGH_ALARM, pmr->hlsv);
        return;
    }
    if (pmr->hlsv && (pmr->lls || (pmr->dval < pmr->dllm)))
    {
        recGblSetSevr((dbCommon *) pmr, LOW_ALARM, pmr->hlsv);
        return;
    }
    
    msta.All = pmr->msta;

    if (msta.Bits.CNTRL_COMM_ERR != 0)
    {
        msta.Bits.CNTRL_COMM_ERR =  0;
        pmr->msta = msta.All;
        MARK(M_MSTA);
        recGblSetSevr((dbCommon *) pmr, COMM_ALARM, INVALID_ALARM);
    }

    if ((msta.Bits.EA_SLIP_STALL != 0) || (msta.Bits.RA_PROBLEM != 0))
    {
      recGblSetSevr((dbCommon *) pmr, STATE_ALARM, MAJOR_ALARM);
    }
    if (pmr->misv && pmr->miss)
    {
        recGblSetSevr((dbCommon *) pmr, STATE_ALARM, pmr->misv);
        return;
    }

    return;
}


/******************************************************************************
        monitor()

LOGIC:
    Initalize local variables for MARKED and UNMARKED macros.
    Set monitor_mask from recGblResetAlarms() return value.
    IF both Monitor (MDEL) and Archive (ADEL) Deadbands are zero.
        Set local_mask <- monitor_mask.
        IF RBV marked for value change
            bitwiseOR both DBE_VALUE and DBE_LOG into local_mask.
            dbpost RBV.
            Clear RBV marked for value change.
        ENDIF
    ELSE IF RBV marked for value change.
        Clear RBV marked for value change.
        Set local_mask <- monitor_mask.
        IF Monitor Deadband (MDEL) is zero.
            bitwiseOR DBE_VALUE into local_mask.
        ELSE
            IF |MLST - RBV| > MDEL
                bitwiseOR DBE_VALUE into local_mask.
                Update Last Value Monitored (MLST).
            ENDIF
        ENDIF
        IF Archive Deadband (ADEL) is zero.
            bitwiseOR DBE_LOG into local_mask.
        ELSE
            IF |ALST - RBV| > ADEL
                bitwiseOR DBE_LOG into local_mask.
                Update Last Value Archived (MLST).
            ENDIF            
        ENIDIF
        IF local_mask is nonzero.
            dbpost RBV.
        ENDIF            
    ENDIF

    dbpost frequently changing PV's.
    IF no PV's marked for value change.
        EXIT.
    ENDIF
    
    dbpost remaining PV's.
    Clear all PF's marked for value change.
    EXIT

*******************************************************************************/
static void monitor(motorRecord * pmr)
{
    unsigned short monitor_mask, local_mask;
    double delta = 0.0;
    mmap_field mmap_bits;
    nmap_field nmap_bits;

    mmap_bits.All = pmr->mmap; /* Initialize for MARKED. */
    nmap_bits.All = pmr->nmap; /* Initialize for MARKED_AUX. */

    monitor_mask = recGblResetAlarms(pmr);

    if (pmr->mdel == 0.0 && pmr->adel == 0.0)
    {
        if ((local_mask = monitor_mask | (MARKED(M_RBV) ? DBE_VAL_LOG : 0)))
        {
            db_post_events(pmr, &pmr->rbv, local_mask);
            UNMARK(M_RBV);
        }
    }
    else if (MARKED(M_RBV))
    {
        UNMARK(M_RBV);
        local_mask = monitor_mask;

        if (pmr->mdel == 0.0) /* check for value change */
            local_mask |= DBE_VALUE;
        else
        {
            delta = fabs(pmr->priv->last.mlst - pmr->rbv);
            if (delta > pmr->mdel)
            {
                local_mask |= DBE_VALUE;
                pmr->priv->last.mlst = pmr->rbv; /* update last value monitored */
            }
        }

        if (pmr->adel == 0.0) /* check for archive change */
            local_mask |= DBE_LOG;
        else
        {
            delta = fabs(pmr->priv->last.alst - pmr->rbv);
            if (delta > pmr->adel)
            {
                local_mask |= DBE_LOG;
                pmr->priv->last.alst = pmr->rbv; /* update last archive value monitored */
            }
        }

        if (local_mask)
            db_post_events(pmr, &pmr->rbv, local_mask);
    }
    

    if ((local_mask = monitor_mask | (MARKED(M_RRBV) ? DBE_VAL_LOG : 0)))
    {
        db_post_events(pmr, &pmr->rrbv, local_mask);
        UNMARK(M_RRBV);
    }
    
    if ((local_mask = monitor_mask | (MARKED(M_DRBV) ? DBE_VAL_LOG : 0)))
    {
        db_post_events(pmr, &pmr->drbv, local_mask);
        UNMARK(M_DRBV);
    }
    
    if ((local_mask = monitor_mask | (MARKED(M_DIFF) ? DBE_VAL_LOG : 0)))
    {
        db_post_events(pmr, &pmr->diff, local_mask);
        UNMARK(M_DIFF);
    }
    
    if ((local_mask = monitor_mask | (MARKED(M_RDIF) ? DBE_VAL_LOG : 0)))
    {
        db_post_events(pmr, &pmr->rdif, local_mask);
        UNMARK(M_RDIF);
    }
    
    if ((local_mask = monitor_mask | (MARKED(M_MSTA) ? DBE_VAL_LOG : 0)))
    {
        msta_field msta;
        
        msta.All = pmr->msta;
        db_post_events(pmr, &pmr->msta, local_mask);
        UNMARK(M_MSTA);
        if (msta.Bits.GAIN_SUPPORT)
        {
            unsigned short pos_maint = (msta.Bits.EA_POSITION) ? 1 : 0;
            if (pos_maint != pmr->cnen)
            {
                pmr->cnen = pos_maint;
                db_post_events(pmr, &pmr->cnen, local_mask);
            }
        }
    }

    if ((pmr->mmap == 0) && (pmr->nmap == 0))
        return;

    /* short circuit: less frequently posted PV's go below this line. */
    mmap_bits.All = pmr->mmap; /* Initialize for MARKED. */
    nmap_bits.All = pmr->nmap; /* Initialize for MARKED_AUX. */

    if ((local_mask = monitor_mask | (MARKED(M_VAL) ? DBE_VAL_LOG : 0)))
        db_post_events(pmr, &pmr->val, local_mask);
    if ((local_mask = monitor_mask | (MARKED(M_DVAL) ? DBE_VAL_LOG : 0)))
        db_post_events(pmr, &pmr->dval, local_mask);
    if ((local_mask = monitor_mask | (MARKED(M_RVAL) ? DBE_VAL_LOG : 0)))
        db_post_events(pmr, &pmr->rval, local_mask);
    if ((local_mask = monitor_mask | (MARKED(M_TDIR) ? DBE_VAL_LOG : 0)))
        db_post_events(pmr, &pmr->tdir, local_mask);
    if ((local_mask = monitor_mask | (MARKED(M_MIP) ? DBE_VAL_LOG : 0)))
        db_post_events(pmr, &pmr->mip, local_mask);
    if ((local_mask = monitor_mask | (MARKED(M_HLM) ? DBE_VAL_LOG : 0)))
        db_post_events(pmr, &pmr->hlm, local_mask);
    if ((local_mask = monitor_mask | (MARKED(M_LLM) ? DBE_VAL_LOG : 0)))
        db_post_events(pmr, &pmr->llm, local_mask);
    if ((local_mask = monitor_mask | (MARKED(M_SPMG) ? DBE_VAL_LOG : 0)))
        db_post_events(pmr, &pmr->spmg, local_mask);
    if ((local_mask = monitor_mask | (MARKED(M_RCNT) ? DBE_VAL_LOG : 0)))
        db_post_events(pmr, &pmr->rcnt, local_mask);
    if ((local_mask = monitor_mask | (MARKED(M_RLV) ? DBE_VAL_LOG : 0)))
        db_post_events(pmr, &pmr->rlv, local_mask);
    if ((local_mask = monitor_mask | (MARKED(M_OFF) ? DBE_VAL_LOG : 0)))
        db_post_events(pmr, &pmr->off, local_mask);
    if ((local_mask = monitor_mask | (MARKED(M_DHLM) ? DBE_VAL_LOG : 0)))
        db_post_events(pmr, &pmr->dhlm, local_mask);
    if ((local_mask = monitor_mask | (MARKED(M_DLLM) ? DBE_VAL_LOG : 0)))
        db_post_events(pmr, &pmr->dllm, local_mask);
    if ((local_mask = monitor_mask | (MARKED(M_HLS) ? DBE_VAL_LOG : 0)))
    {
        db_post_events(pmr, &pmr->hls, local_mask);
        if ((pmr->dir == motorDIR_Pos) == (pmr->mres >= 0))
            db_post_events(pmr, &pmr->rhls, local_mask);
        else
            db_post_events(pmr, &pmr->rlls, local_mask);
    }
    if ((local_mask = monitor_mask | (MARKED(M_LLS) ? DBE_VAL_LOG : 0)))
    {
        db_post_events(pmr, &pmr->lls, local_mask);
        if ((pmr->dir == motorDIR_Pos) == (pmr->mres >= 0))
            db_post_events(pmr, &pmr->rlls, local_mask);
        else
            db_post_events(pmr, &pmr->rhls, local_mask);
    }
    if ((local_mask = monitor_mask | (MARKED(M_ATHM) ? DBE_VAL_LOG : 0)))
        db_post_events(pmr, &pmr->athm, local_mask);
    if ((local_mask = monitor_mask | (MARKED(M_MRES) ? DBE_VAL_LOG : 0)))
        db_post_events(pmr, &pmr->mres, local_mask);
    if ((local_mask = monitor_mask | (MARKED(M_ERES) ? DBE_VAL_LOG : 0)))
        db_post_events(pmr, &pmr->eres, local_mask);
    if ((local_mask = monitor_mask | (MARKED(M_UEIP) ? DBE_VAL_LOG : 0)))
        db_post_events(pmr, &pmr->ueip, local_mask);
    if ((local_mask = monitor_mask | (MARKED(M_LVIO) ? DBE_VAL_LOG : 0)))
        db_post_events(pmr, &pmr->lvio, local_mask);
    if ((local_mask = monitor_mask | (MARKED(M_STOP) ? DBE_VAL_LOG : 0)))
        db_post_events(pmr, &pmr->stop, local_mask);

    if ((local_mask = monitor_mask | (MARKED_AUX(M_SBAS) ? DBE_VAL_LOG : 0)))
        db_post_events(pmr, &pmr->sbas, local_mask);
    if ((local_mask = monitor_mask | (MARKED_AUX(M_SREV) ? DBE_VAL_LOG : 0)))
        db_post_events(pmr, &pmr->srev, local_mask);
    if ((local_mask = monitor_mask | (MARKED_AUX(M_UREV) ? DBE_VAL_LOG : 0)))
        db_post_events(pmr, &pmr->urev, local_mask);
    if ((local_mask = monitor_mask | (MARKED_AUX(M_VELO) ? DBE_VAL_LOG : 0)))
        db_post_events(pmr, &pmr->velo, local_mask);
    if ((local_mask = monitor_mask | (MARKED_AUX(M_VBAS) ? DBE_VAL_LOG : 0)))
        db_post_events(pmr, &pmr->vbas, local_mask);
    if ((local_mask = monitor_mask | (MARKED_AUX(M_MISS) ? DBE_VAL_LOG : 0)))
        db_post_events(pmr, &pmr->miss, local_mask);
    if ((local_mask = monitor_mask | (MARKED(M_MOVN) ? DBE_VAL_LOG : 0)))
        db_post_events(pmr, &pmr->movn, local_mask);
    if ((local_mask = monitor_mask | (MARKED(M_DMOV) ? DBE_VAL_LOG : 0)))
        db_post_events(pmr, &pmr->dmov, local_mask);
    if ((local_mask = monitor_mask | (MARKED_AUX(M_STUP) ? DBE_VAL_LOG : 0)))
        db_post_events(pmr, &pmr->stup, local_mask);
    if ((local_mask = monitor_mask | (MARKED_AUX(M_JOGF) ? DBE_VAL_LOG : 0)))
        db_post_events(pmr, &pmr->jogf, local_mask);
    if ((local_mask = monitor_mask | (MARKED_AUX(M_JOGR) ? DBE_VAL_LOG : 0)))
        db_post_events(pmr, &pmr->jogr, local_mask);
    if ((local_mask = monitor_mask | (MARKED_AUX(M_HOMF) ? DBE_VAL_LOG : 0)))
        db_post_events(pmr, &pmr->homf, local_mask);
    if ((local_mask = monitor_mask | (MARKED_AUX(M_HOMR) ? DBE_VAL_LOG : 0)))
        db_post_events(pmr, &pmr->homr, local_mask);
    if ((local_mask = monitor_mask | (MARKED_AUX(M_CDIR) ? DBE_VAL_LOG : 0)))
        db_post_events(pmr, &pmr->cdir, local_mask);

    UNMARK_ALL;
}


/******************************************************************************
        process_motor_info()
*******************************************************************************/
static void process_motor_info(motorRecord * pmr, bool initcall)
{
    double old_drbv = pmr->drbv;
    double old_rbv = pmr->rbv;
    long old_rrbv = pmr->rrbv;
    short old_tdir = pmr->tdir;
    short old_movn = pmr->movn;
    short old_hls = pmr->hls;
    short old_lls = pmr->lls;
    short old_athm = pmr->athm;
    int dir = (pmr->dir == motorDIR_Pos) ? 1 : -1;
    bool ls_active;
    msta_field msta;

    /*** Process record fields. ***/

    /* Calculate raw and dial readback values. */
    msta.All = pmr->msta;
    if (pmr->ueip == motorUEIP_Yes)
    {
        /* An encoder is present and the user wants us to use it. */
        pmr->rrbv = pmr->rep;
        /* device support gave us a double, use it */
        if (pmr->priv->readBack.encoderPosition)
            pmr->drbv = pmr->priv->readBack.encoderPosition * pmr->eres;
        else
            pmr->drbv = pmr->rrbv * pmr->eres;
    }
    else if (pmr->urip == motorUEIP_Yes && initcall == false)
    {
        double rdblvalue;
        long rtnstat;

        rtnstat = dbGetLink(&(pmr->rdbl), DBR_DOUBLE, &rdblvalue, 0, 0 );
        if (!RTN_SUCCESS(rtnstat))
        {
            Debug(3, "%s:%d %s process_motor_info: error reading RDBL link\n",
                  __FILE__, __LINE__, pmr->name);

            if (pmr->mip != MIP_DONE)
            {
                /* Error reading RDBL - stop move. */
                clear_buttons(pmr);
                pmr->stop = 1;
                MARK(M_STOP);
            }
        }
        else
        {
            pmr->rrbv = NINT((rdblvalue * pmr->rres) / pmr->mres);
#if AXIS_CRIPPLE_RDBL_TO_32BIT_INT
            pmr->drbv = pmr->rrbv * pmr->mres;
#else
            pmr->drbv = rdblvalue * pmr->rres / pmr->mres;
#endif
        }
    }
    else /* UEIP = URIP = No */
    {
        pmr->rrbv = pmr->rmp;
        /* if device support gave us a double, use it */
        if (pmr->priv->readBack.position)
            pmr->drbv = pmr->priv->readBack.position * pmr->mres;
        else
            pmr->drbv = pmr->rrbv * pmr->mres;
    }

    if (pmr->rrbv != old_rrbv)
        MARK(M_RRBV);
    if (pmr->drbv != old_drbv)
        MARK(M_DRBV);

    /* Calculate user readback value. */
    pmr->rbv = dir * pmr->drbv + pmr->off;
    if (pmr->rbv != old_rbv)
        MARK(M_RBV);

    /* Set most recent raw direction. */
    pmr->tdir = (msta.Bits.RA_DIRECTION) ? 1 : 0;
    if (pmr->tdir != old_tdir)
        MARK(M_TDIR);

    /* Get states of high, low limit switches. State is independent of direction. */
    pmr->rhls = (msta.Bits.RA_PLUS_LS);
    pmr->rlls = (msta.Bits.RA_MINUS_LS);

    /* Treat limit switch active only when it is pressed and in direction of movement. */
    ls_active = ((pmr->rhls && pmr->cdir) || (pmr->rlls && !pmr->cdir)) ? true : false;

    if ((pmr->mip & MIP_HOMF) || (pmr->mip & MIP_HOMR))
    {
        if (pmr->mflg & MF_HOME_ON_LS)
            ls_active = false;    /*Suppress stop on LS if homing on LS is allowed */
    }
    
    pmr->hls = ((pmr->dir == motorDIR_Pos) == (pmr->mres >= 0)) ? pmr->rhls : pmr->rlls;
    pmr->lls = ((pmr->dir == motorDIR_Pos) == (pmr->mres >= 0)) ? pmr->rlls : pmr->rhls;
    if (pmr->hls != old_hls)
        MARK(M_HLS);
    if (pmr->lls != old_lls)
        MARK(M_LLS);

    /* If the motor has a problem, stop it if needed */
    if ((ls_active == true ||
         (msta.Bits.RA_PROBLEM && (!(pmr->mflg & MF_NOSTOP_PROB)))))
    {
        pmr->stop = 1;
        MARK(M_STOP);
        fprintf(stdout, "%s:%d %s STOP ls_active=%d PROBLEM=%d MF_NOSTOP_PROB=%d\n",
               __FILE__, __LINE__,
               pmr->name, ls_active ? 1 : 0,
               msta.Bits.RA_PROBLEM ? 1 : 0,
               pmr->mflg & MF_NOSTOP_PROB ? 1 : 0);
        clear_buttons(pmr);
    }

    /* Get motor-now-moving indicator. */
    if (msta.Bits.RA_DONE)
        pmr->movn = 0;
    else if (ls_active && (!(pmr->mflg & MF_LS_RAMPDOWN)))
        pmr->movn = 0;
    else
        pmr->movn = 1;

    if (pmr->movn != old_movn)
        MARK(M_MOVN);
    
    /* Get state of motor's or encoder's home switch. */
    if (pmr->ueip)
        pmr->athm = (msta.Bits.EA_HOME) ? 1 : 0;
    else
        pmr->athm = (msta.Bits.RA_HOME) ? 1 : 0;

    if (pmr->athm != old_athm)
        MARK(M_ATHM);

    pmr->diff = pmr->dval - pmr->drbv;
    MARK(M_DIFF);
    pmr->rdif = NINT(pmr->diff / pmr->mres);
    MARK(M_RDIF);
}

/* Calc and load new raw position into motor w/out moving it. */
static void load_pos(motorRecord * pmr)
{
    double newpos = pmr->dval / pmr->mres;

    pmr->priv->last.dval = pmr->dval;
    pmr->priv->last.val = pmr->val;
    if (pmr->rval != (epicsInt32) NINT(newpos))
        MARK(M_RVAL);
    pmr->priv->last.rval = pmr->rval = (epicsInt32) NINT(newpos);

    if (pmr->foff)
    {
        /* Translate dial value to user value. */
        if (pmr->dir == motorDIR_Pos)
            pmr->val = pmr->off + pmr->dval;
        else
            pmr->val = pmr->off - pmr->dval;
        MARK(M_VAL);
        pmr->priv->last.val = pmr->val;
    }
    else
    {
        /* Translate dial limits to user limits. */
        if (pmr->dir == motorDIR_Pos)
            pmr->off = pmr->val - pmr->dval;
        else
            pmr->off = pmr->val + pmr->dval;
        MARK(M_OFF);
        set_userlimits(pmr);    /* Translate dial limits to user limits. */
    }
    MIP_SET_VAL(MIP_LOAD_P);
    MARK(M_MIP);
    if (pmr->dmov == TRUE)
    {
        pmr->dmov = FALSE;
        MARK(M_DMOV);
    }

    /* Load pos. into motor controller.  Get new readback vals. */
    devSupLoadPos(pmr, newpos);
    devSupGetInfo(pmr);
}

/*
 * FUNCTION... static void check_resolution(motorRecord *)
 *
 * INPUT ARGUMENTS...
 *      1 - motor record pointer
 *
 * RETRUN ARGUMENTS... None.
 *
 */

static void check_resolution(motorRecord * pmr)
{
    /*
     * Reconcile two different ways of specifying speed, resolution, and make
     * sure things are sane.
     */

    /* SREV (steps/revolution) must be sane. */
    if (pmr->srev <= 0)
    {
        pmr->srev = 200;
        MARK_AUX(M_SREV);
    }

    /* UREV (EGU/revolution) <--> MRES (EGU/step) */
    if (pmr->urev != 0.0)
    {
        pmr->mres = pmr->urev / pmr->srev;
        MARK(M_MRES);
    }
    if (pmr->mres == 0.0)
    {
        pmr->mres = 1.0;
        MARK(M_MRES);
    }
    if (pmr->urev != pmr->mres * pmr->srev)
    {
        pmr->urev = pmr->mres * pmr->srev;
        MARK_AUX(M_UREV);
    }
}

/*
 * FUNCTION... static void check_resolution(motorRecord *)
 *
 * INPUT ARGUMENTS...
 *      1 - motor record pointer
 *
 *  NORMAL RETURN.
 */

static void check_speed(motorRecord * pmr)
{
    double fabs_urev = fabs(pmr->urev);

    /* SMAX (revolutions/sec) <--> VMAX (EGU/sec) */
    if (pmr->smax > 0.0)
        pmr->vmax = pmr->smax * fabs_urev;
    else if (pmr->vmax > 0.0)
        pmr->smax = pmr->vmax / fabs_urev;
    else
        pmr->smax = pmr->vmax = 0.0;

    if (pmr->priv->configRO.motorMaxVelocityDial > 0.0)
    {
        if (pmr->vmax == 0.0)
            pmr->vmax = pmr->priv->configRO.motorMaxVelocityDial;
        else
            range_check(pmr, &pmr->vmax, 0.0, pmr->priv->configRO.motorMaxVelocityDial);
        pmr->smax = pmr->vmax / fabs_urev;
    }
    db_post_events(pmr, &pmr->vmax, DBE_VAL_LOG);
    db_post_events(pmr, &pmr->smax, DBE_VAL_LOG);

    /* SBAS (revolutions/sec) <--> VBAS (EGU/sec) */
    if (pmr->sbas != 0.0)
    {
        range_check(pmr, &pmr->sbas, 0.0, pmr->smax);
        pmr->vbas = pmr->sbas * fabs_urev;
    }
    else
    {
        range_check(pmr, &pmr->vbas, 0.0, pmr->vmax);
        pmr->sbas = pmr->vbas / fabs_urev;
    }
    db_post_events(pmr, &pmr->vbas, DBE_VAL_LOG);
    db_post_events(pmr, &pmr->sbas, DBE_VAL_LOG);

    if (pmr->priv->configRO.motorDefVelocityDial > 0.0)
        pmr->velo = pmr->priv->configRO.motorDefVelocityDial;

    /* S (revolutions/sec) <--> VELO (EGU/sec) */
    if (pmr->s != 0.0)
    {
        range_check(pmr, &pmr->s, pmr->sbas, pmr->smax);
        pmr->velo = pmr->s * fabs_urev;
    }
    else
    {
        range_check(pmr, &pmr->velo, pmr->vbas, pmr->vmax);
        pmr->s = pmr->velo / fabs_urev;
    }
    db_post_events(pmr, &pmr->velo, DBE_VAL_LOG);
    db_post_events(pmr, &pmr->s, DBE_VAL_LOG);

    /* SBAK (revolutions/sec) <--> BVEL (EGU/sec) */
    if (pmr->sbak != 0.0)
    {
        range_check(pmr, &pmr->sbak, pmr->sbas, pmr->smax);
        pmr->bvel = pmr->sbak * fabs_urev;
    }
    else
    {
        range_check(pmr, &pmr->bvel, pmr->vbas, pmr->vmax);
        pmr->sbak = pmr->bvel / fabs_urev;
    }
    db_post_events(pmr, &pmr->sbak, DBE_VAL_LOG);
    db_post_events(pmr, &pmr->bvel, DBE_VAL_LOG);

    /* Sanity check on acceleration time. */
    if (pmr->accl == 0.0)
    {
        if (pmr->priv->configRO.motorDefJogAccDial > 0.0 && pmr->velo > 0.0)
            pmr->accl = pmr->velo / pmr->priv->configRO.motorDefJogAccDial;
        else
            pmr->accl = 0.1;
        db_post_events(pmr, &pmr->accl, DBE_VAL_LOG);
    }
    if (pmr->bacc == 0.0)
    {
        pmr->bacc = 0.1;
        db_post_events(pmr, &pmr->bacc, DBE_VAL_LOG);
    }
    /* Sanity check on jog velocity and acceleration rate. */
    if (pmr->jvel == 0.0)
    {
        if (pmr->priv->configRO.motorDefJogVeloDial > 0.0)
            pmr->jvel = pmr->priv->configRO.motorDefJogVeloDial;
        else
            pmr->jvel = pmr->velo;
    }
    else
        range_check(pmr, &pmr->jvel, pmr->vbas, pmr->vmax);

    if (pmr->jar == 0.0)
    {
        if (pmr->priv->configRO.motorDefJogAccDial > 0.0)
            pmr->jar = pmr->priv->configRO.motorDefJogAccDial;
        else
            pmr->jar = pmr->velo / pmr->accl;
    }

    /* Sanity check on home velocity. */
    if (pmr->hvel == 0.0)
        pmr->hvel = pmr->vbas;
    else
        range_check(pmr, &pmr->hvel, pmr->vbas, pmr->vmax);
}

/*
FUNCTION... void set_dial_highlimit(motorRecord *)
USAGE... Set dial-coordinate high limit.
NOTES... This function sends a command to the device to set the raw dial high
limit.  This is done so that a device level function may do an error check on
the validity of the limit.  This is to support those devices (e.g., MM4000)
that have their own, read-only, travel limits.
*/
static void set_dial_highlimit(motorRecord *pmr)
{
    int dir_positive = (pmr->dir == motorDIR_Pos);
    motor_cmnd command;

    if (pmr->mres < 0) {
        command = SET_LOW_LIMIT;
    } else {
        command = SET_HIGH_LIMIT;
    }
    if (pmr->priv->softLimitRO.motorDialLimitsValid)
    {
        double maxValue = pmr->priv->softLimitRO.motorDialHighLimitRO;
        fprintf(stdout,
                "%s:%d %s pmr->dhlm=%g maxValue=%g\n",
                __FILE__, __LINE__, pmr->name,
                pmr->dhlm, maxValue);
        fflush(stdout);
        if (pmr->dhlm > maxValue) pmr->dhlm = maxValue;
    }
    devSupUpdateLimitFromDial(pmr, command, pmr->dhlm);
    if (dir_positive)
    {
        pmr->hlm = pmr->dhlm + pmr->off;
        MARK(M_HLM);
    }
    else
    {
        pmr->llm = -(pmr->dhlm) + pmr->off;
        MARK(M_LLM);
    }
    MARK(M_DHLM);
    recalcLVIO(pmr);
}



static void set_user_highlimit(motorRecord *pmr)
{
    double offset = pmr->off;
    if (pmr->dir == motorDIR_Pos)
    {
        pmr->dhlm = pmr->hlm - offset;
        set_dial_highlimit(pmr);
    }
    else
    {
        pmr->dllm = -(pmr->hlm) + offset;
        set_dial_lowlimit(pmr);
    }
    MARK(M_HLM);
}


/*
FUNCTION... void set_dial_lowlimit(motorRecord *)
USAGE... Set dial-coordinate low limit.
NOTES... This function sends a command to the device to set the raw dial low
limit.  This is done so that a device level function may do an error check on
the validity of the limit.  This is to support those devices (e.g., MM4000)
that have their own, read-only, travel limits.
*/
static void set_dial_lowlimit(motorRecord *pmr)
{
    int dir_positive = (pmr->dir == motorDIR_Pos);
    motor_cmnd command;

    if (pmr->mres < 0) {
        command = SET_HIGH_LIMIT;
    } else {
        command = SET_LOW_LIMIT;
    }
    if (pmr->priv->softLimitRO.motorDialLimitsValid)
    {
        double minValue = pmr->priv->softLimitRO.motorDialLowLimitRO;
        fprintf(stdout,
                "%s:%d %s pmr->dllm=%g minValue=%g\n",
                __FILE__, __LINE__, pmr->name,
                pmr->dllm, minValue);
        fflush(stdout);
        if (pmr->dllm < minValue) pmr->dllm = minValue;
    }
    devSupUpdateLimitFromDial(pmr, command, pmr->dllm);

    if (dir_positive)
    {
        pmr->llm = pmr->dllm + pmr->off;
        MARK(M_LLM);
    }
    else
    {
        pmr->hlm = -(pmr->dllm) + pmr->off;
        MARK(M_HLM);
    }
    MARK(M_DLLM);
    recalcLVIO(pmr);
}

static void set_user_lowlimit(motorRecord *pmr)
{
    double offset = pmr->off;
    if (pmr->dir == motorDIR_Pos)
    {
        pmr->dllm = pmr->llm - offset;
        set_dial_lowlimit(pmr);
    }
    else
    {
        pmr->dhlm = -(pmr->llm) + offset;
        set_dial_highlimit(pmr);
    }
    MARK(M_LLM);
}
/*
FUNCTION... void set_userlimits(motorRecord *)
USAGE... Translate dial-coordinate limits to user-coordinate limits.
*/
static void set_userlimits(motorRecord *pmr)
{
    if (!softLimitsDefined(pmr))
    {
        pmr->hlm = pmr->llm = 0.0;
    }
    else if (pmr->dir == motorDIR_Pos)
    {
        pmr->hlm = pmr->dhlm + pmr->off;
        pmr->llm = pmr->dllm + pmr->off;
    }
    else
    {
        pmr->hlm = -(pmr->dllm) + pmr->off;
        pmr->llm = -(pmr->dhlm) + pmr->off;
    }
    MARK(M_HLM);
    MARK(M_LLM);
}

/*
FUNCTION... void range_check(motorRecord *, double *, double, double)
USAGE... Limit parameter to valid range; i.e., min. <= parameter <= max.

INPUT...        parm - pointer to parameter to be range check.
                min  - minimum value.
                max  - 0 = max. range check disabled; !0 = maximum value.
*/
static void range_check(motorRecord *pmr, double *parm_ptr, double min, double max)
{
    bool changed = false;
    double parm_val = *parm_ptr;

    if (parm_val < min)
    {
        parm_val = min;
        changed = true;
    }
    if (max != 0.0 && parm_val > max)
    {
        parm_val = max;
        changed = true;
    }

    if (changed == true)
    {
        *parm_ptr = parm_val;
        db_post_events(pmr, parm_ptr, DBE_VAL_LOG);
    }
}


/*
FUNCTION... void clear_buttons(motorRecord *)
USAGE... Clear all motion request buttons.
*/
static void clear_buttons(motorRecord *pmr)
{
    if (pmr->jogf)
    {
        pmr->jogf = 0;
        MARK_AUX(M_JOGF);
    }
    if (pmr->jogr)
    {
        pmr->jogr = 0;
        MARK_AUX(M_JOGR);
    }
    if (pmr->homf)
    {
        pmr->homf = 0;
        MARK_AUX(M_HOMF);
    }
    if (pmr->homr)
    {
        pmr->homr = 0;
        MARK_AUX(M_HOMR);
    }
}

/*
FUNCTION... void syncTargetPosition(motorRecord *)
USAGE... Synchronize target positions with readbacks.
*/
static void syncTargetPosition(motorRecord *pmr)
{
    int dir = (pmr->dir == motorDIR_Pos) ? 1 : -1;
    double rdblvalue;
    long rtnstat;


    if (pmr->ueip)
    {
        /* An encoder is present and the user wants us to use it. */
        pmr->rrbv = pmr->rep;
        /* device support gave us a double, use it */
        if (pmr->priv->readBack.encoderPosition)
            pmr->drbv = pmr->priv->readBack.encoderPosition * pmr->eres;
        else
            pmr->drbv = pmr->rrbv * pmr->eres;
    }
    else if (pmr->urip)
    {
        /* user wants us to use the readback link */
        rtnstat = dbGetLink(&(pmr->rdbl), DBR_DOUBLE, &rdblvalue, 0, 0 );
        if (!RTN_SUCCESS(rtnstat))
            fprintf(stdout, "%s:%d %s: syncTargetPosition: error reading RDBL link.\n",
                    __FILE__, __LINE__, pmr->name);
        else
        {
            pmr->rrbv = NINT((rdblvalue * pmr->rres) / pmr->mres);
#if AXIS_CRIPPLE_RDBL_TO_32BIT_INT
            pmr->drbv = pmr->rrbv * pmr->mres;
#else
            pmr->drbv = rdblvalue * pmr->rres / pmr->mres;
#endif
        }
    }
    else
    {
        pmr->rrbv = pmr->rmp;
        /* if device support gave us a double, use it */
        if (pmr->priv->readBack.position)
            pmr->drbv = pmr->priv->readBack.position * pmr->mres;
        else
            pmr->drbv = pmr->rrbv * pmr->mres;
    }
    MARK(M_RRBV);
    MARK(M_DRBV);
    pmr->rbv = pmr->drbv * dir + pmr->off;
    MARK(M_RBV);

    pmr->val  = pmr->priv->last.val = pmr->rbv ;
    MARK(M_VAL);
    pmr->dval = pmr->priv->last.dval = pmr->drbv;
    MARK(M_DVAL);
    pmr->rval = pmr->priv->last.rval = NINT(pmr->dval / pmr->mres);
    MARK(M_RVAL);
}

