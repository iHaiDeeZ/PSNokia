"""Receives PSNokia PS4 log lines (UDP, see common/psn_log.h) and saves them.

Each run of the PS4 app starts with a line at time 0.000, which starts a new
file: ps4/logs/<YYYYmmdd-HHMMSS>.txt. ps4/logs/latest.txt always mirrors the
current session.
"""
import datetime
import os
import socket

PORT = 18194
LOG_DIR = os.path.join(os.path.dirname(os.path.abspath(__file__)), "..", "logs")


def main():
    os.makedirs(LOG_DIR, exist_ok=True)
    sock = socket.socket(socket.AF_INET, socket.SOCK_DGRAM)
    sock.setsockopt(socket.SOL_SOCKET, socket.SO_REUSEADDR, 1)
    sock.bind(("0.0.0.0", PORT))
    print(f"listening on UDP {PORT}, saving to {os.path.normpath(LOG_DIR)}", flush=True)

    session = None
    latest = None
    last = None
    while True:
        data, (ip, _) = sock.recvfrom(4096)
        text = data.decode("utf-8", "replace")
        # Each line arrives twice (unicast + broadcast); keep one copy.
        if text == last:
            continue
        last = text
        if session is None or text.startswith("[     0.0"):
            stamp = datetime.datetime.now().strftime("%Y%m%d-%H%M%S")
            for f in (session, latest):
                if f:
                    f.close()
            session = open(os.path.join(LOG_DIR, f"{stamp}.txt"), "w", encoding="utf-8")
            latest = open(os.path.join(LOG_DIR, "latest.txt"), "w", encoding="utf-8")
            print(f"--- new session from {ip}: {stamp}.txt", flush=True)
        for f in (session, latest):
            f.write(text + "\n")
            f.flush()
        print(text, flush=True)


if __name__ == "__main__":
    main()
