// PSNokia PS4 logger (header-only).
//
// Every line is written, flushed and fsync'd to each available sink, so a
// crash never loses what came before it. Sinks:
//   /mnt/usb0/psnokia/<name>, /mnt/usb1/psnokia/<name>  - plug the stick into
//                                                          the PC and upload
//   /data/psnokia/<name>                                 - FTP fallback
//   UDP port PSN_LOG_PORT, sent to PSN_LOG_PC_IP and broadcast on the LAN -
//     received on the PC by ps4/tools/psn_logrecv.py (no USB/FTP needed)
// plus stdout (visible with a klog viewer).
//
// Usage: psn_log_open("probe.txt"); psn_log("x=%d", x); psn_log_sinks() lists
// the files that were actually opened.

#ifndef PSN_LOG_H
#define PSN_LOG_H

#include <stdio.h>
#include <stdarg.h>
#include <string.h>
#include <unistd.h>
#include <sys/stat.h>
#include <orbis/libkernel.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>

#ifndef PSN_LOG_PC_IP
#define PSN_LOG_PC_IP "192.168.0.28"
#endif
#define PSN_LOG_PORT 18194

#define PSN_LOG_MAX_SINKS 3

static FILE *psn_log_files[PSN_LOG_MAX_SINKS];
static char psn_log_paths[PSN_LOG_MAX_SINKS][96];
static int psn_log_count;
static unsigned long psn_log_start_us;
static int psn_log_sock = -1;
static struct sockaddr_in psn_log_dest[2];

static void psn_log_net_open(void) {
    psn_log_sock = socket(AF_INET, SOCK_DGRAM, 0);
    if (psn_log_sock < 0) {
        return;
    }
    int on = 1;
    setsockopt(psn_log_sock, SOL_SOCKET, SO_BROADCAST, &on, sizeof(on));
    for (int i = 0; i < 2; i++) {
        memset(&psn_log_dest[i], 0, sizeof(psn_log_dest[i]));
        psn_log_dest[i].sin_family = AF_INET;
        psn_log_dest[i].sin_port = htons(PSN_LOG_PORT);
    }
    psn_log_dest[0].sin_addr.s_addr = inet_addr(PSN_LOG_PC_IP);
    psn_log_dest[1].sin_addr.s_addr = htonl(INADDR_BROADCAST);
}

static void psn_log_net_send(const char *text) {
    if (psn_log_sock < 0) {
        return;
    }
    size_t len = strlen(text);
    for (int i = 0; i < 2; i++) {
        sendto(psn_log_sock, text, len, 0, (struct sockaddr *)&psn_log_dest[i],
               sizeof(psn_log_dest[i]));
    }
}

static void psn_log_try_sink(const char *root, const char *name) {
    char dir[64];
    snprintf(dir, sizeof(dir), "%s/psnokia", root);
    mkdir(dir, 0777);
    char *path = psn_log_paths[psn_log_count];
    snprintf(path, sizeof(psn_log_paths[0]), "%s/%s", dir, name);
    FILE *f = fopen(path, "w");
    if (f) {
        psn_log_files[psn_log_count++] = f;
    }
}

static void psn_log_open(const char *name) {
    psn_log_start_us = sceKernelGetProcessTime();
    psn_log_net_open();
    psn_log_try_sink("/mnt/usb0", name);
    psn_log_try_sink("/mnt/usb1", name);
    if (psn_log_count < PSN_LOG_MAX_SINKS) {
        psn_log_try_sink("/data", name);
    }
}

// Returns the opened log paths, one per line (for showing on screen).
static const char *psn_log_sinks(void) {
    static char buf[PSN_LOG_MAX_SINKS * 100 + 64];
    buf[0] = 0;
    for (int i = 0; i < psn_log_count; i++) {
        strcat(buf, psn_log_paths[i]);
        strcat(buf, "\n");
    }
    if (psn_log_count == 0) {
        strcat(buf, "no writable file location\n");
    }
    strcat(buf, psn_log_sock >= 0 ? "network: UDP to " PSN_LOG_PC_IP " + broadcast\n"
                                 : "network: socket failed\n");
    return buf;
}

static void psn_log_v(const char *fmt, va_list ap) {
    char buf[512];
    unsigned long ms = (sceKernelGetProcessTime() - psn_log_start_us) / 1000;
    int n = snprintf(buf, sizeof(buf), "[%6lu.%03lu] ", ms / 1000, ms % 1000);
    vsnprintf(buf + n, sizeof(buf) - n, fmt, ap);
    printf("[psnokia] %s\n", buf);
    psn_log_net_send(buf);
    for (int i = 0; i < psn_log_count; i++) {
        fprintf(psn_log_files[i], "%s\n", buf);
        fflush(psn_log_files[i]);
        fsync(fileno(psn_log_files[i]));
    }
}

static void psn_log(const char *fmt, ...) {
    va_list ap;
    va_start(ap, fmt);
    psn_log_v(fmt, ap);
    va_end(ap);
}

#endif // PSN_LOG_H
