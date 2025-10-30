# %%
#!/usr/bin/env python3
import argparse
import sys
import threading
import time
from collections import deque
from datetime import datetime

import matplotlib.pyplot as plt
import matplotlib.animation as animation
try:
    import serial
    from serial.tools import list_ports
except ImportError:
    print("Missing dependency. Install with: pip install pyserial matplotlib")
    sys.exit(1)


def auto_detect_port(prefer_id=True):
    ports = list(list_ports.comports())
    # Prefer STLink VCP if present
    for p in ports:
        desc = f"{p.description} {p.manufacturer} {p.product}".lower()
        if "stlink" in desc or "stmicro" in desc:
            return p.device
    # Fallback: first ACM/USB serial
    for p in ports:
        if "ttyACM" in p.device or "ttyUSB" in p.device or "usbmodem" in p.device:
            return p.device
    # Windows fallback: first COM
    for p in ports:
        if p.device.upper().startswith("COM"):
            return p.device
    return None


def list_all_ports():
    return [p.device for p in list_ports.comports()]


def reader_worker(port, baud, dq_vals, dq_times, log_path=None, stop_evt=None):
    ser = None
    logf = None
    try:
        ser = serial.Serial(port, baudrate=baud, timeout=1)
        # Give device a moment, then flush old data
        time.sleep(0.2)
        ser.reset_input_buffer()
        if log_path:
            logf = open(log_path, "a", buffering=1)
            if logf.tell() == 0:
                logf.write("timestamp,value\n")
        while not (stop_evt and stop_evt.is_set()):
            try:
                line = ser.readline()
                if not line:
                    continue
                s = line.decode(errors="ignore").strip()
                if not s:
                    continue
                # Take first token, accept comma or dot
                token = s.split()[0].replace(",", ".")
                val = float(token)
                t = time.time()
                dq_vals.append(val)
                dq_times.append(t)
                if logf:
                    ts = datetime.fromtimestamp(t).isoformat()
                    logf.write(f"{ts},{val}\n")
            except ValueError:
                # Ignore non-numeric lines
                continue
            except serial.SerialException:
                break
    finally:
        if ser:
            try:
                ser.close()
            except Exception:
                pass
        if logf:
            try:
                logf.close()
            except Exception:
                pass


def main():
    parser = argparse.ArgumentParser(description="Live plot voltage from serial")
    parser.add_argument("--port", help="Serial port (e.g. /dev/ttyACM0, COM5). Omit to auto-detect.")
    parser.add_argument("--baud", type=int, default=115200, help="Baud rate")
    parser.add_argument("--window", type=float, default=10.0, help="Time window (seconds)")
    parser.add_argument("--points", type=int, default=2000, help="Max points buffered")
    parser.add_argument("--log", help="Optional CSV log file path")
    parser.add_argument("--ylim", type=float, nargs=2, metavar=("YMIN","YMAX"), help="Y-axis limits")
    args = parser.parse_args()

    port = args.port or auto_detect_port()
    if not port:
        print("No serial port detected. Available:", list_all_ports())
        sys.exit(1)
    print(f"Using port: {port} @ {args.baud} baud")

    dq_vals = deque(maxlen=args.points)
    dq_times = deque(maxlen=args.points)
    stop_evt = threading.Event()
    th = threading.Thread(target=reader_worker, args=(port, args.baud, dq_vals, dq_times, args.log, stop_evt), daemon=True)
    th.start()

    plt.style.use("ggplot")
    fig, ax = plt.subplots()
    line, = ax.plot([], [], lw=1.2)
    ax.set_xlabel("Time (s)")
    ax.set_ylabel("Voltage (V)")
    ax.set_title("Live Voltage")
    if args.ylim:
        ax.set_ylim(args.ylim[0], args.ylim[1])

    def update(_):
        if not dq_times:
            return line,
        t_now = dq_times[-1]
        t0 = t_now - args.window
        # Collect points within time window
        xs = []
        ys = []
        for tt, vv in zip(dq_times, dq_vals):
            if tt >= t0:
                xs.append(tt - t_now)  # relative time (negative to 0)
                ys.append(vv)
        if not xs:
            return line,
        line.set_data(xs, ys)
        ax.set_xlim(-args.window, 0.0)
        if not args.ylim:
            # Auto-scale with small margin
            ymin = min(ys)
            ymax = max(ys)
            if ymin == ymax:
                ymin -= 0.1
                ymax += 0.1
            margin = (ymax - ymin) * 0.1
            ax.set_ylim(ymin - margin, ymax + margin)
        return line,

    ani = animation.FuncAnimation(fig, update, interval=50, blit=True)
    try:
        plt.show()
    except KeyboardInterrupt:
        pass
    finally:
        stop_evt.set()
        th.join(timeout=1.0)


if __name__ == "__main__":
    main()



