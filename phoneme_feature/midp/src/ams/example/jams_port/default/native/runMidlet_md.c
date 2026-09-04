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
    if (error || jar == NULL) {
        pcsl_string_free(&manifestName);
        return NULL;
    }

    len = midpGetJarEntry(jar, &manifestName, &data);
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
int
main(int argc, char** commandlineArgs) {
    static const char* jarPath = "app0:game.jar";
    static char* vitaArgv[4];
    int argCount;
    char* classname;

    (void)argc;
    (void)commandlineArgs;
    setenv("MIDP_HOME", "app0:", 1);

    classname = vita_find_midlet_class(jarPath);

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
    vitaArgv[2] = (char*)jarPath;
    if (classname != NULL) {
        vitaArgv[3] = classname;
        argCount = 4;
    } else {
        argCount = 3;
    }

    return runMidlet(argCount, vitaArgv);
}
