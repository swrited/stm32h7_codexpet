import argparse
import sys
import time
from pathlib import Path

import serial


def show(data):
    if data:
        sys.stdout.buffer.write(data.decode("utf-8", errors="replace").encode("utf-8", errors="replace"))
        sys.stdout.buffer.flush()


def wait_prompt(ser, timeout=5.0):
    end = time.time() + timeout
    buf = b""
    while time.time() < end:
        if ser.in_waiting:
            data = ser.read(ser.in_waiting)
            buf += data
            show(data)
            if b">" in buf[-16:]:
                return True
        time.sleep(0.03)
    return False


def send_wait(ser, cmd, timeout=5.0):
    ser.write((cmd + "\r\n").encode("utf-8"))
    ser.flush()
    ok = wait_prompt(ser, timeout)
    if not ok:
        print("\n[WARN] prompt timeout after:", cmd[:80])
    return ok


def lua_dq(s):
    return '"' + s.replace("\\", "\\\\").replace('"', '\\"').replace("\r", "\\r").replace("\n", "\\n") + '"'


def main():
    p = argparse.ArgumentParser()
    p.add_argument("--port", required=True)
    p.add_argument("--baud", type=int, default=9600)
    p.add_argument("--src", required=True)
    p.add_argument("--dest", default="init.lua")
    args = p.parse_args()

    lines = Path(args.src).read_text(encoding="utf-8").splitlines()

    with serial.Serial(args.port, args.baud, timeout=0.1, write_timeout=2) as ser:
        time.sleep(0.5)
        ser.reset_input_buffer()
        ser.write(b"\r\n")
        ser.flush()
        wait_prompt(ser, 2.0)

        print("\nStopping old code...")
        send_wait(ser, "if srv then srv:close() srv=nil end", 2.0)
        send_wait(ser, "for i=0,6 do pcall(function() tmr.stop(i) end) end", 2.0)

        print(f"Removing {args.dest}...")
        send_wait(ser, f"file.remove('{args.dest}')", 2.0)

        print(f"Opening {args.dest}...")
        send_wait(ser, f"file.open('{args.dest}','w+')", 2.0)

        print("Writing lines...")
        for i, line in enumerate(lines, 1):
            cmd = "file.write(" + lua_dq(line + "\n") + ")"
            if not send_wait(ser, cmd, 3.0):
                raise RuntimeError(f"upload failed at line {i}")
            if i % 10 == 0 or i == len(lines):
                print(f"\n{i}/{len(lines)}")

        print("Closing file...")
        send_wait(ser, "file.close()", 2.0)

        print("Verifying file list...")
        send_wait(ser, "for k,v in pairs(file.list()) do print(k,v) end", 3.0)

        print("Restarting...")
        ser.write(b"node.restart()\r\n")
        ser.flush()
        time.sleep(0.5)
        show(ser.read(ser.in_waiting or 1))
        print("Done")


if __name__ == "__main__":
    main()
