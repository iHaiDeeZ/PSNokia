/*
 *
 *
 * Copyright  1990-2007 Sun Microsystems, Inc. All Rights Reserved.
 * DO NOT ALTER OR REMOVE COPYRIGHT NOTICES OR THIS FILE HEADER
 * 
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License version
 * 2 only, as published by the Free Software Foundation.
 * 
 * This program is distributed in the hope that it will be useful, but
 * WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the GNU
 * General Public License version 2 for more details (a copy is
 * included at /legal/license.txt).
 * 
 * You should have received a copy of the GNU General Public License
 * version 2 along with this work; if not, write to the Free Software
 * Foundation, Inc., 51 Franklin St, Fifth Floor, Boston, MA
 * 02110-1301 USA
 * 
 * Please contact Sun Microsystems, Inc., 4150 Network Circle, Santa
 * Clara, CA 95054 or visit www.sun.com if you need additional
 * information or have any questions.
 */

#include <stdio.h>
#include <string.h>
#include <ctype.h>
#include <midp_logging.h>
#include <midpAMS.h>
#include <midpMalloc.h>
#include <jvm.h>
#include <findMidlet.h>
#include <midpUtilKni.h>
#include <suitestore_task_manager.h>
#include <commandLineUtil.h>
#include <commandLineUtil_md.h>

#ifdef PS4
#include <stdio.h>
#include <renderlog.h>
/* No console on the PS4: record where runMidlet gives up */
#define PS4_RUNMIDLET_MARK(what, value) do { \
    char ps4_buf_[96]; \
    int ps4_len_ = snprintf(ps4_buf_, sizeof(ps4_buf_), "runMidlet: %s %d (line %d)\n", \
                            (what), (int)(value), __LINE__); \
    RENDERLOG_WRITE(ps4_buf_, ps4_len_); \
  } while (0)
#else
#define PS4_RUNMIDLET_MARK(what, value) do { } while (0)
#endif

#if ENABLE_MULTIPLE_ISOLATES
#define MIDP_HEAP_REQUIREMENT (MAX_ISOLATES * 1024 * 1024)
#elif __has_include(<psp2/kernel/threadmgr.h>)
/* 1280KB (the shared default below) is far too small for real MIDlets
 * with dozens of image assets on this platform - confirmed via a live
 * heap dump (Runtime.freeMemory()/totalMemory()) showing the heap
 * essentially completely exhausted (156 bytes free out of 1310336) at
 * the exact point a game's asset-loading sequence started failing.
 * The Vita has 512MB RAM; this is still a tiny, conservative slice of
 * it. Guarded to this platform only - other ports keep the original
 * conservative default. 4MB was CONFIRMED SAFE on Vita3K (multiple
 * games tested) but NOT on real hardware - session 6, 2026-09-04: a
 * real-hardware renderlog.txt showed a game reaching FIRST_FRAME and
 * real extended gameplay (lots of touch input) before dying with no
 * further log output at all - consistent with the JVM running out of
 * heap so completely that even the error-reporting path couldn't
 * allocate what it needed to log anything. That test also happened
 * AFTER fixing the separate real-hardware-only SDL_MULTIGESTURE
 * logging flood (see midp_msgQueue_md.c) and the param.sfo system
 * memory budget (256MB -> 365MB via ATTRIBUTE2=12) - so this is
 * apparently the NEXT real ceiling once those two are out of the way.
 * 8MB was tried once before (session 4, to fix Sonic 2 Dash's missing
 * background visuals, on VITA3K only) and caused a real boot hang for
 * that game after running for a while, so bumping straight to 8MB
 * again is not assumed safe.
 * UPDATE (2026-09-05): 6MB was re-tested on REAL HARDWARE with Metal
 * Slug (the exact VPK with all 3 stacked fixes - this heap bump, the
 * MULTIGESTURE logging fix, and the ATTRIBUTE2 SFO budget fix). The
 * MULTIGESTURE fix is confirmed working (renderlog.txt's two latest
 * launches show zero raw=2050 spam, vs. heavy flooding in older,
 * uncleared log entries from before that fix). But the OOM crash
 * itself is NOT fixed at 6MB: renderlog.txt reaches FIRST_FRAME then
 * goes completely silent after only a handful of SAMPLE lines - same
 * "too OOM to even log an error" signature as before, now happening
 * almost immediately after first frame rather than after extended
 * play. Moved to 7MB as the next cautious step (not jumping to 8MB,
 * which is a known-bad value for a different game/scenario). Re-test
 * on REAL HARDWARE specifically (Vita3K alone is not sufficient - see
 * project memory), with renderlog.txt/vmmarker.txt deleted from the
 * SD card first for a clean read. See project memory for full story. */
#define MIDP_HEAP_REQUIREMENT (7 * 1024 * 1024)
#elif defined(PS4)
/* The PS4 has plenty of memory; the Java heap comes from the low heap
 * arena (256MB, see ps4/common/lowheap_malloc.c). */
#define MIDP_HEAP_REQUIREMENT (16 * 1024 * 1024)
#else
#define MIDP_HEAP_REQUIREMENT (1280 * 1024)
#endif

/** Maximum number of command line arguments. */
#define RUNMIDLET_MAX_ARGS 32

/** Usage text for the run MIDlet executable. */
static const char* const runUsageText =
"\n"
"Usage: runMidlet [<VM args>] [-debug] [-loop] [-classpathext <path>]\n"
"           (-ordinal <suite number> | <suite ID>)\n"
"           [<classname of MIDlet to run> [<arg0> [<arg1> [<arg2>]]]]\n"
"         Run a MIDlet of an installed suite. If the classname\n"
"         of the MIDlet is not provided and the suite has multiple MIDlets,\n"
"         the first MIDlet from the suite will be run.\n"
"          -debug: start the VM suspended in debug mode\n"
"          -loop: run the MIDlet in a loop until system shuts down\n"
"          -classpathext <path>: append <path> to classpath passed to VM\n"
"             (can access classes from <path> as if they were romized)\n"
"\n"
"  where <suite number> is the number of a suite as displayed by the\n"
"  listMidlets command, and <suite ID> is the unique ID a suite is \n"
"  referenced by\n\n";

/**
 * Runs a MIDlet from an installed MIDlet suite. This is an example of
 * how to use the public MIDP API.
 *
 * @param argc The total number of arguments
 * @param argv An array of 'C' strings containing the arguments
 *
 * @return <tt>0</tt> for success, otherwise <tt>-1</tt>
 *
 * IMPL_NOTE:determine if it is desirable for user targeted output
 *       messages to be sent via the log/trace service, or if
 *       they should remain as printf calls
 */
int
runMidlet(int argc, char** commandlineArgs) {
    int status = -1;
    SuiteIdType suiteId   = UNUSED_SUITE_ID;
    pcsl_string classname = PCSL_STRING_NULL;
    pcsl_string arg0 = PCSL_STRING_NULL;
    pcsl_string arg1 = PCSL_STRING_NULL;
    pcsl_string arg2 = PCSL_STRING_NULL;
    int repeatMidlet = 0;
    char* argv[RUNMIDLET_MAX_ARGS];
    int i, used;
    int debugOption = MIDP_NO_DEBUG;
    char *progName = commandlineArgs[0];
    char* midpHome = NULL;
    char* additionalPath;
    SuiteIdType* pSuites = NULL;
    int numberOfSuites = 0;
    int ordinalSuiteNumber = -1;
    char* chSuiteNum = NULL;

    JVM_Initialize(); /* It's OK to call this more than once */

    /*
     * Set Java heap capacity now so it can been overridden from command line.
     */
    JVM_SetConfig(JVM_CONFIG_HEAP_CAPACITY, MIDP_HEAP_REQUIREMENT);

    /*
     * Parse options for the VM. This is desirable on a 'development' platform
     * such as linux_qte. For actual device ports, copy this block of code only
     * if your device can handle command-line arguments.
     */

    /*
     * JVM_ParseOneArg expects commandlineArgs[0] to contain the first actual
     * parameter
     */
    argc --;
    commandlineArgs ++;

    while ((used = JVM_ParseOneArg(argc, commandlineArgs)) > 0) {
        argc -= used;
        commandlineArgs += used;
    }

    /* Restore commandlineArgs[0] to contain the program name. */
    argc ++;
    commandlineArgs --;
    commandlineArgs[0] = progName;

    /*
     * Not all platforms allow rewriting the command line arg array,
     * make a copy
     */
    if (argc > RUNMIDLET_MAX_ARGS) {
        REPORT_ERROR(LC_AMS, "Number of arguments exceeds supported limit");
        fprintf(stderr, "Number of arguments exceeds supported limit\n");
        PS4_RUNMIDLET_MARK("early return", -1);
        return -1;
    }

    for (i = 0; i < argc; i++) {
        argv[i] = commandlineArgs[i];
    }

    if (midpRemoveOptionFlag("-debug", argv, &argc) != NULL) {
        debugOption = MIDP_DEBUG_SUSPEND;
    }

    if (midpRemoveOptionFlag("-loop", argv, &argc) != NULL) {
        repeatMidlet = 1;
    }

    /* run the midlet suite by its ordinal number */
    if ((chSuiteNum = midpRemoveCommandOption("-ordinal",
                                              argv, &argc)) != NULL) {
        /* the format of the string is "number:" */
        if (sscanf(chSuiteNum, "%d", &ordinalSuiteNumber) != 1) {
            REPORT_ERROR(LC_AMS, "Invalid suite number format");
            fprintf(stderr, "Invalid suite number format: %s\n", chSuiteNum);
            PS4_RUNMIDLET_MARK("early return", -1);
        return -1;
        }
    }

    /* additionalPath gets appended to the classpath */
    additionalPath = midpRemoveCommandOption("-classpathext", argv, &argc);

    if (argc == 1 && ordinalSuiteNumber == -1) {
        REPORT_ERROR(LC_AMS, "Too few arguments given.");
        fprintf(stderr, runUsageText);
        PS4_RUNMIDLET_MARK("early return", -1);
        return -1;
    }

    if (argc > 6) {
        REPORT_ERROR(LC_AMS, "Too many arguments given\n");
        fprintf(stderr, "Too many arguments given\n%s", runUsageText);
        PS4_RUNMIDLET_MARK("early return", -1);
        return -1;
    }

    /* get midp home directory, set it */
    midpHome = midpFixMidpHome(argv[0]);
    if (midpHome == NULL) {
        PS4_RUNMIDLET_MARK("early return", -1);
        return -1;
    }
    
    /* set up midpHome before calling initialize */
    midpSetHomeDir(midpHome);

    if (midpInitialize() != 0) {
        REPORT_ERROR(LC_AMS, "Not enough memory");
        fprintf(stderr, "Not enough memory\n");
        PS4_RUNMIDLET_MARK("early return", -1);
        return -1;
    }

    do {
        int onlyDigits;
        int len;
        int i;

        if (argc > 5) {
            if (PCSL_STRING_OK != pcsl_string_from_chars(argv[5], &arg2)) {
                REPORT_ERROR(LC_AMS, "Out of Memory");
                fprintf(stderr, "Out Of Memory\n");
                break;
            }
        }

        if (argc > 4) {
            if (PCSL_STRING_OK != pcsl_string_from_chars(argv[4], &arg1)) {
                REPORT_ERROR(LC_AMS, "Out of Memory");
                fprintf(stderr, "Out Of Memory\n");
                break;
            }
        }

        if (argc > 3) {
            if (PCSL_STRING_OK != pcsl_string_from_chars(argv[3], &arg0)) {
                REPORT_ERROR(LC_AMS, "Out of Memory");
                fprintf(stderr, "Out Of Memory\n");
                break;
            }
        }

        if (argc > 2) {
            if (PCSL_STRING_OK != pcsl_string_from_chars(argv[2], &classname)) {
                REPORT_ERROR(LC_AMS, "Out of Memory");
                fprintf(stderr, "Out Of Memory\n");
                break;
            }

        }

        /* if the storage name only digits, convert it */
        onlyDigits = 1;
        len = strlen(argv[1]);
        for (i = 0; i < len; i++) {
            if (!isdigit((argv[1])[i])) {
                onlyDigits = 0;
                break;
            }
        }

        if (ordinalSuiteNumber != -1 || onlyDigits) {
            /* load IDs of the installed suites */
            MIDPError err = midp_get_suite_ids(&pSuites, &numberOfSuites);
            if (err != ALL_OK) {
                REPORT_ERROR1(LC_AMS, "Error in midp_get_suite_ids(), code %d",
                              err);
                fprintf(stderr, "Error in midp_get_suite_ids(), code %d.\n",
                        err);
                break;
            }
        }

        if (ordinalSuiteNumber != -1) {
            /* run the midlet suite by its ordinal number */
            if (ordinalSuiteNumber > numberOfSuites || ordinalSuiteNumber < 1) {
                REPORT_ERROR(LC_AMS, "Suite number out of range");
                fprintf(stderr, "Suite number out of range\n");
                midp_free_suite_ids(pSuites, numberOfSuites);
                break;
            }

            suiteId = pSuites[ordinalSuiteNumber - 1];
        } else if (onlyDigits) {
            /* run the midlet suite by its ID */
            int i;

            /* the format of the string is "number:" */
            if (sscanf(argv[1], "%d", &suiteId) != 1) {
                REPORT_ERROR(LC_AMS, "Invalid suite ID format");
                fprintf(stderr, "Invalid suite ID format\n");
                break;
            }

            for (i = 0; i < numberOfSuites; i++) {
                if (suiteId == pSuites[i]) {
                    break;
                }
            }

            if (i == numberOfSuites) {
                REPORT_ERROR(LC_AMS, "Suite with the given ID was not found");
                fprintf(stderr, "Suite with the given ID was not found\n");
                break;
            }
        } else {
            /* Run by ID */
            suiteId = INTERNAL_SUITE_ID;

            if (strcmp(argv[1], "internal") &&
                strcmp(argv[1], "-1") && additionalPath == NULL) {
                /*
                 * If the argument is not a suite ID, it might be a full
                 * path to the midlet suite's jar file.
                 * In this case this path is added to the classpath and
                 * the suite is run without installation (it is useful
                 * for internal test and development purposes).
                 */
                additionalPath = argv[1];
            }
        }

        if (pcsl_string_is_null(&classname)) {
            int res = find_midlet_class(suiteId, 1, &classname);
            if (OUT_OF_MEM_LEN == res) {
                REPORT_ERROR(LC_AMS, "Out of Memory");
                fprintf(stderr, "Out Of Memory\n");
                break;
            }

            if (NULL_LEN == res) {
                REPORT_ERROR(LC_AMS, "Could not find the first MIDlet");
                fprintf(stderr, "Could not find the first MIDlet\n");
                break;
            }
        }

        PS4_RUNMIDLET_MARK("starting the MIDlet, suite", suiteId);
        do {
            status = midp_run_midlet_with_args_cp(suiteId, &classname,
                                                  &arg0, &arg1, &arg2,
                                                  debugOption, additionalPath);
        } while (repeatMidlet && status != MIDP_SHUTDOWN_STATUS);

        if (pSuites != NULL) {
            midp_free_suite_ids(pSuites, numberOfSuites);
            suiteId = UNUSED_SUITE_ID;
        }
    } while (0);

    pcsl_string_free(&arg0);
    pcsl_string_free(&arg1);
    pcsl_string_free(&arg2);
    pcsl_string_free(&classname);

    switch (status) {
    case MIDP_SHUTDOWN_STATUS:
        break;

    case MIDP_ERROR_STATUS:
        REPORT_ERROR(LC_AMS, "The MIDlet suite could not be run.");
        fprintf(stderr, "The MIDlet suite could not be run.\n");
        break;

    case SUITE_NOT_FOUND_STATUS:
        REPORT_ERROR(LC_AMS, "The MIDlet suite was not found.");
        fprintf(stderr, "The MIDlet suite was not found.\n");
        break;

    default:
        break;
    }

    PS4_RUNMIDLET_MARK("finished with status", status);
    midpFinalize();

    return status;
}
