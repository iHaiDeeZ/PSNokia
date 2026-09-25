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

#include <runMidlet.h>
#include <stdlib.h>
#include <string.h>
#include <midpJar.h>
#include <midpMalloc.h>
#include <midpUtilKni.h>
#ifdef PS4
#include <stdio.h>
#include <sys/stat.h>
#include <renderlog.h>
#endif

/**
 * Reads META-INF/MANIFEST.MF straight out of the given jar and returns
 * the class name from its "MIDlet-1" entry (the last comma-separated
 * field), or NULL if it can't be determined.
 *
 * find_midlet_class()/runMidlet()'s own auto-detect reads the MIDlet-1
 * property from the *installed suite's* stored properties, which is
 * empty for our "path to a jar file" developer/test launch mode (the
 * jar is never actually installed as a suite) -- so it always fails
 * instantly. Reading the manifest directly out of the jar itself works
 * regardless of which jar is dropped in as game.jar.
 */
#ifdef PS4
/* 1 if the manifest declares a landscape display (width > height), 0 if
 * portrait, -1 if it says nothing */
static int ps4_manifest_landscape = -1;

static void ps4_check_display_size(const char* manifest) {
    static const char* const keys[] = {
        "Nokia-MIDlet-Original-Display-Size:",
        "Nokia-MIDlet-Target-Display-Size:",
        "MIDlet-Display-Size:"
    };
    int i, w, h;
    for (i = 0; i < (int)(sizeof(keys) / sizeof(keys[0])); i++) {
        const char* p = strstr(manifest, keys[i]);
        if (p != NULL && sscanf(p + strlen(keys[i]), " %d , %d", &w, &h) == 2) {
            ps4_manifest_landscape = w > h;
            return;
        }
    }
}
#endif

static char* vita_find_midlet_class(const char* jarPath) {
    pcsl_string jarName = PCSL_STRING_NULL;
    pcsl_string manifestName = PCSL_STRING_NULL;
    unsigned char* data = NULL;
    long len;
    void* jar;
    int error = 0;
    char* result = NULL;

    if (pcsl_string_from_chars(jarPath, &jarName) != PCSL_STRING_OK) {
        return NULL;
    }
    if (pcsl_string_from_chars("META-INF/MANIFEST.MF", &manifestName) != PCSL_STRING_OK) {
        pcsl_string_free(&jarName);
        return NULL;
    }

    jar = midpOpenJar(&error, &jarName);
    pcsl_string_free(&jarName);
#ifdef PS4
    {
        char buf[96];
        int n = snprintf(buf, sizeof(buf), "midpOpenJar: error %d, jar %p\n", error, jar);
        RENDERLOG_WRITE(buf, n);
    }
#endif
    if (error || jar == NULL) {
        pcsl_string_free(&manifestName);
        return NULL;
    }

    len = midpGetJarEntry(jar, &manifestName, &data);
#ifdef PS4
    {
        char buf[96];
        int n = snprintf(buf, sizeof(buf), "midpGetJarEntry(MANIFEST.MF): %ld bytes\n", len);
        RENDERLOG_WRITE(buf, n);
    }
#endif
    pcsl_string_free(&manifestName);
    midpCloseJar(jar);

    if (len <= 0 || data == NULL) {
        return NULL;
    }

    {
        char* buf = (char*)malloc(len + 1);
        char* line;
        char* classStart;
        char* p;

        if (buf == NULL) {
            midpFree(data);
            return NULL;
        }
        memcpy(buf, data, len);
        buf[len] = '\0';
        midpFree(data);

        /* Manifest lines longer than 72 bytes continue on the next line,
         * which starts with a single space: join them first */
        {
            char* in = buf;
            char* out = buf;
            while (*in) {
                if (in[0] == '\r' && in[1] == '\n' && in[2] == ' ') {
                    in += 3;
                } else if ((in[0] == '\n' || in[0] == '\r') && in[1] == ' ') {
                    in += 2;
                } else {
                    *out++ = *in++;
                }
            }
            *out = '\0';
        }

#ifdef PS4
        ps4_check_display_size(buf);
#endif
        line = strstr(buf, "MIDlet-1:");
        if (line == NULL) {
            free(buf);
            return NULL;
        }
        line += strlen("MIDlet-1:");

        /* The class is the last comma-separated field on the line. */
        p = line;
        while (*p && *p != '\r' && *p != '\n') p++;
        classStart = NULL;
        {
            char* q;
            for (q = line; q < p; q++) {
                if (*q == ',') classStart = q + 1;
            }
        }
        if (classStart == NULL) {
            free(buf);
            return NULL;
        }
        while (classStart < p && (*classStart == ' ' || *classStart == '\t')) classStart++;
        while (p > classStart && (p[-1] == ' ' || p[-1] == '\t')) p--;

        if (p > classStart) {
            result = (char*)malloc(p - classStart + 1);
            if (result != NULL) {
                memcpy(result, classStart, p - classStart);
                result[p - classStart] = '\0';
            }
        }
        free(buf);
    }
    return result;
}

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
 *
 * Vita port note: there is no real command line when launched from
 * Vita3K/the Vita homebrew loader (argc/argv are not meaningful), so we
 * hardcode a fixed launch target instead: run the bundled game.jar's
 * first MIDlet class directly (the "path to a jar file" developer/test
 * mode that runMidlet() already supports), and point MIDP_HOME at app0:
 * so storageInitialize() finds appdb/ and lib/ that we bundle alongside
 * it. The class name is read directly from the jar's own manifest (see
 * vita_find_midlet_class above) so this works for any jar dropped in as
 * game.jar, not just ones with a class named RMIDlet.
 */
#ifdef PS4
/* The package's files are mounted read-only at /app0; MIDP_HOME must be
 * writable (appdb/ holds the record stores), so it lives under /data and
 * its appdb/ is seeded from the package. midpStorage.c reads lib/ from
 * /app0 directly. */
#define PORT_GAME_JAR  "/app0/game.jar"
#include <renderlog.h>
#include <unistd.h>
#include <stdio.h>
#include <sys/stat.h>

/* /data/psnokia/<TITLE_ID>: every game package gets its own record stores */
static char ps4_midp_home[64] = "/data/psnokia/midp";
#define PORT_MIDP_HOME ps4_midp_home

/* Reads TITLE_ID from the package's param.sfo into id. Returns 0 on
 * success. */
static int ps4_read_title_id(char* id, int size) {
    unsigned char sfo[4096];
    unsigned keyTable, dataTable, count, i;
    FILE* f;
    size_t len;
    /* Written by the package Makefile */
    f = fopen("/app0/titleid.txt", "rb");
    if (f != NULL) {
        len = fread(id, 1, size - 1, f);
        fclose(f);
        id[len] = 0;
        for (i = 0; i < len; i++) {
            char c = id[i];
            if (!((c >= 'A' && c <= 'Z') || (c >= '0' && c <= '9'))) {
                id[i] = 0;
                break;
            }
        }
        if (id[0] != 0) {
            return 0;
        }
    }
    f = fopen("/app0/sce_sys/param.sfo", "rb");
    if (f == NULL) {
        return -1;
    }
    len = fread(sfo, 1, sizeof(sfo), f);
    fclose(f);
    if (len < 20 || sfo[1] != 'P' || sfo[2] != 'S' || sfo[3] != 'F') {
        return -1;
    }
#define SFO_U16(o) ((unsigned)sfo[o] | ((unsigned)sfo[(o) + 1] << 8))
#define SFO_U32(o) (SFO_U16(o) | (SFO_U16((o) + 2) << 16))
    keyTable = SFO_U32(8);
    dataTable = SFO_U32(12);
    count = SFO_U32(16);
    for (i = 0; i < count && 20 + i * 16 + 16 <= len; i++) {
        unsigned e = 20 + i * 16;
        unsigned key = keyTable + SFO_U16(e);
        unsigned data = dataTable + SFO_U32(e + 12);
        unsigned n = SFO_U32(e + 4);
        if (key + 9 <= len && memcmp(sfo + key, "TITLE_ID", 9) == 0 &&
            n > 1 && n < (unsigned)size && data + n <= len) {
            memcpy(id, sfo + data, n);
            id[n - 1] = 0;
            return 0;
        }
    }
    return -1;
}

/* The files MIDP ships in appdb/ (midp_ps4/appdb) */
static const char* const ps4_appdb_files[] = {
    "_ack8.png", "_ch_disabled.png", "_ch_fg_requested.png",
    "_ch_hilight_bg.png", "_ch_single.png", "_ch_suite.png",
    "_dukeok8.png", "_main.ks", "_single8.png", "_suite8.png",
    "splash_screen_176x210.png", "splash_screen_210x176.png",
    "trustedmidlet_icon.png"
};

/* Copies from into to unless to already exists. Returns 0 on success. */
static int ps4_copy_if_missing(const char* from, const char* to) {
    struct stat st;
    FILE* in;
    FILE* out;
    char buf[8192];
    size_t n;
    int rc = 0;

    if (stat(to, &st) == 0) {
        return 0;
    }
    in = fopen(from, "rb");
    if (in == NULL) {
        return -1;
    }
    out = fopen(to, "wb");
    if (out == NULL) {
        fclose(in);
        return -1;
    }
    while ((n = fread(buf, 1, sizeof(buf), in)) > 0) {
        if (fwrite(buf, 1, n, out) != n) {
            rc = -1;
            break;
        }
    }
    fclose(in);
    fclose(out);
    return rc;
}

static void ps4_prepare_midp_home(void) {
    char from[160];
    char to[160];
    char buf[200];
    int i, n, failed = 0;

    {
        char id[16];
        if (ps4_read_title_id(id, sizeof(id)) == 0) {
            snprintf(ps4_midp_home, sizeof(ps4_midp_home), "/data/psnokia/%s", id);
        }
    }
    mkdir("/data/psnokia", 0777);
    mkdir(PORT_MIDP_HOME, 0777);
    snprintf(to, sizeof(to), "%s/appdb", PORT_MIDP_HOME);
    mkdir(to, 0777);
    for (i = 0; i < (int)(sizeof(ps4_appdb_files) / sizeof(ps4_appdb_files[0])); i++) {
        snprintf(from, sizeof(from), "/app0/appdb/%s", ps4_appdb_files[i]);
        snprintf(to, sizeof(to), "%s/appdb/%s", PORT_MIDP_HOME, ps4_appdb_files[i]);
        if (ps4_copy_if_missing(from, to) != 0) {
            n = snprintf(buf, sizeof(buf), "appdb: could not copy %s\n", from);
            RENDERLOG_WRITE(buf, n);
            failed++;
        }
    }
    n = snprintf(buf, sizeof(buf), "MIDP_HOME %s ready (%d copy failures)\n", PORT_MIDP_HOME, failed);
    RENDERLOG_WRITE(buf, n);
}
/* In ps4/common/psn_marker.c */
void psn_install_crash_handler(void);
#define PS4_MARK(text) RENDERLOG_WRITE(text "\n", (int)sizeof(text))
#else
#define PORT_GAME_JAR  "app0:game.jar"
#define PORT_MIDP_HOME "app0:"
#endif

int
main(int argc, char** commandlineArgs) {
    static const char* jarPath = PORT_GAME_JAR;
    static char* vitaArgv[5];
    int argCount;
    char* classname;

    (void)argc;
    (void)commandlineArgs;
#ifdef PS4
    {
        static const char start[] = "runMidlet (PS4) starting\n";
        RENDERLOG_WRITE(start, (int)sizeof(start) - 1);
    }
    psn_install_crash_handler();
    /* No console: keep what MIDP prints to stdout/stderr (usage and error
     * messages) */
    freopen("/data/psnokia/stdout.txt", "w", stdout);
    freopen("/data/psnokia/stdout.txt", "a", stderr);
    setvbuf(stdout, NULL, _IONBF, 0);
    setvbuf(stderr, NULL, _IONBF, 0);
#endif
#ifdef PS4
    ps4_prepare_midp_home();
#endif
    setenv("MIDP_HOME", PORT_MIDP_HOME, 1);

#ifdef PS4
    {
        static const char* paths[] = { "/app0", "/app0/game.jar", "/app0/lib", "/app0/appdb", "/app0/lib/skin.bin", "/app0/titleid.txt" };
        char buf[160];
        int n, i;
        const char* home = getenv("MIDP_HOME");
        n = snprintf(buf, sizeof(buf), "MIDP_HOME=%s\n", home ? home : "(unset)");
        RENDERLOG_WRITE(buf, n);
        for (i = 0; i < (int)(sizeof(paths) / sizeof(paths[0])); i++) {
            struct stat st;
            int r = stat(paths[i], &st);
            n = snprintf(buf, sizeof(buf), "stat %s: %d, mode %o, size %lld\n",
                         paths[i], r, r == 0 ? (unsigned)st.st_mode : 0,
                         r == 0 ? (long long)st.st_size : -1LL);
            RENDERLOG_WRITE(buf, n);
        }
        {
            FILE* f = fopen(jarPath, "rb");
            unsigned char head[4] = { 0, 0, 0, 0 };
            size_t got = f ? fread(head, 1, 4, f) : 0;
            if (f) fclose(f);
            n = snprintf(buf, sizeof(buf), "fopen game.jar: %s, first bytes %02x %02x %02x %02x (%d)\n",
                         f ? "ok" : "FAILED", head[0], head[1], head[2], head[3], (int)got);
            RENDERLOG_WRITE(buf, n);
        }
    }
#endif
    classname = vita_find_midlet_class(jarPath);
#ifdef PS4
    {
        char buf[160];
        int len = snprintf(buf, sizeof(buf), "MIDlet class from manifest: %s\n",
                           classname != NULL ? classname : "(none)");
        RENDERLOG_WRITE(buf, len);
    }
#endif

    /* "-int" forces pure bytecode interpretation, disabling the JIT
     * compiler (which IS otherwise built into this binary - confirmed
     * via nm on libcldc_vm_g.a showing real JVMCodeGenerator/
     * BytecodeCompileClosure symbols, and ENABLE_COMPILER is true at
     * compile time too). Tried removing it (session 4, 2026-09) hoping
     * to fix a CPU-heavy game's low ~10fps: confirmed the removal
     * compiled correctly (objdump'd main() - only 2-3 vitaArgv stores,
     * no "-int" load) and UseCompiler defaults to true, so the compiler
     * really was active - but FPS did not change AT ALL (still exactly
     * ~10fps), a clean negative result. Reverted back to "-int" as the
     * known-stable baseline (a leftover runMidlet_g_nocompiler.velf
     * build artifact suggests a past session already explored and
     * abandoned this same path). If revisited: the open question is WHY
     * the compiler makes no measurable difference - never confirmed
     * whether it's actually compiling this game's hot methods at all
     * (would need real Compiler-level instrumentation, not attempted),
     * so don't assume "JIT doesn't help this VM" as a settled fact. */
    vitaArgv[0] = "runMidlet";
    vitaArgv[1] = "-int";
#ifdef PS4
    argCount = 2;
#ifdef _DEBUG
    /* Debug build: log every exception the VM raises, with the Java stack
     * (the release VM has no trace flags) */
    vitaArgv[argCount++] = "+TraceExceptions";
#endif
    vitaArgv[argCount++] = (char*)jarPath;
    if (classname != NULL) {
        vitaArgv[argCount++] = classname;
    }
#else
    vitaArgv[2] = (char*)jarPath;
    if (classname != NULL) {
        vitaArgv[3] = classname;
        argCount = 4;
    } else {
        argCount = 3;
    }
#endif

#ifdef PS4
    /* Most MIDP games are made for portrait 240x320 phones; the display is
     * 320x240 unless J2ME_GP2X_REVERSE is set (lfjport_ui_init). An empty
     * /data/psnokia/landscape or /data/psnokia/portrait file overrides
     * the choice. */
    {
        struct stat st;
        int landscape;
        char buf[96];
        int n;
        if (stat("/data/psnokia/landscape", &st) == 0) {
            landscape = 1;
        } else if (stat("/data/psnokia/portrait", &st) == 0) {
            landscape = 0;
        } else {
            landscape = ps4_manifest_landscape == 1;
        }
        if (!landscape) {
            setenv("J2ME_GP2X_REVERSE", "1", 1);
        }
        /* A fixed screen size ("WxH"): <MIDP_HOME>/screen.txt overrides
         * the package's /app0/screen.txt */
        {
            char path[96], size[32];
            FILE* f;
            snprintf(path, sizeof(path), "%s/screen.txt", PORT_MIDP_HOME);
            f = fopen(path, "rb");
            if (f == NULL) {
                f = fopen("/app0/screen.txt", "rb");
            }
            if (f != NULL) {
                size_t len = fread(size, 1, sizeof(size) - 1, f);
                fclose(f);
                size[len] = 0;
                while (len > 0 && (size[len - 1] == '\n' || size[len - 1] == '\r' || size[len - 1] == ' ')) {
                    size[--len] = 0;
                }
                if (len > 0) {
                    setenv("J2ME_SCREEN_SIZE", size, 1);
                    n = snprintf(buf, sizeof(buf), "screen size %s\n", size);
                    RENDERLOG_WRITE(buf, n);
                }
            }
        }
        n = snprintf(buf, sizeof(buf), "display: %s (manifest says %d)\n",
                     landscape ? "landscape 320x240" : "portrait 240x320",
                     ps4_manifest_landscape);
        RENDERLOG_WRITE(buf, n);
    }
    PS4_MARK("calling runMidlet");
    {
        char buf[64];
        int result = runMidlet(argCount, vitaArgv);
        int len = snprintf(buf, sizeof(buf), "runMidlet returned %d\n", result);
        RENDERLOG_WRITE(buf, len);
        /* Returning from main ends the app like a crash would; stay alive
         * so the logs can be read, and let the user close it. */
        for (;;) {
            sleep(1);
        }
    }
#else
    return runMidlet(argCount, vitaArgv);
#endif
}
