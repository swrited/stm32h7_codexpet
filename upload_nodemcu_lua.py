import argparse
import sys
import time
from pathlib import Path

import serial


def read_available(ser, delay=0.08):
    time.sleep(delay)
    data = b""
    while ser.in_waiting:
        data += ser.read(ser.in_waiting)
        time.sleep(0.02)
    if data:
        text = data.decode("utf-8", errors="replace")
        sys.stdout.buffer.write(text.encode("utf-8", errors="replace"))
        sys.stdout.buffer.flush()
    return data


def send_line(ser, line, wait=0.08):
    ser.write((line + "\r\n").encode("utf-8"))
    ser.flush()
    return read_available(ser, wait)


def lua_quote(chunk):
    return "'" + chunk.replace("\\", "\\\\").replace("'", "\\'").replace("\r", "\\r").replace("\n", "\\n") + "'"


def main():
    parser = argparse.ArgumentParser(description="Upload Lua file to NodeMCU via serial")
    parser.add_argument("--port", required=True)
    parser.add_argument("--baud", type=int, default=9600)
    parser.add_argument("--src", required=True)
    parser.add_argument("--dest", default="init.lua")
    args = parser.parse_args()

    content = Path(args.src).read_text(encoding="utf-8")

    with serial.Serial(args.port, args.baud, timeout=0.2, write_timeout=2) as ser:
        time.sleep(0.5)
        read_available(ser, 0.2)

        print("\nStopping old server/timers if any...")
        send_line(ser, "if srv then srv:close() srv=nil end", 0.15)
        send_line(ser, "for i=0,6 do pcall(function() tmr.stop(i) end) end", 0.15)

        print(f"Removing {args.dest}...")
        send_line(ser, f"file.remove('{args.dest}')", 0.15)

        print(f"Opening {args.dest}...")
        send_line(ser, f"file.open('{args.dest}','w+')", 0.15)

        print("Writing file...")
        chunk_size = 64
        total = len(content)
        for pos in range(0, total, chunk_size):
            chunk = content[pos:pos + chunk_size]
            send_line(ser, "file.write(" + lua_quote(chunk) + ")", 0.03)
            if pos % (chunk_size * 16) == 0:
                print(f"\n{min(pos + chunk_size, total)}/{total}")

        print("\nClosing file...")
        send_line(ser, "file.close()", 0.15)

        print("Verifying file list...")
        send_line(ser, "for k,v in pairs(file.list()) do print(k,v) end", 0.3)

        print("Restarting ESP8266...")
        send_line(ser, "node.restart()", 0.2)
        print("Done.")


if __name__ == "__main__":
    main()
