
import argparse
import re
import sys
import threading
import time
from collections import deque
import os 

try:
    import serial  # pyserial
except ImportError:
    print("ERROR: pyserial is required. Install with: pip install pyserial", file=sys.stderr)
    sys.exit(1)


def parse_args():
    p = argparse.ArgumentParser(description="Read ADC=xxxx from a serial VCP and display value, amplitude, frequency")
    p.add_argument("--port", help="Serial port path. If omitted, auto-detects STLink VCP under /dev/serial/by-id or falls back to /dev/ttyACM0")
    p.add_argument("--baud", type=int, default=115200, help="Baud rate (default: 115200)")
    p.add_argument("--vref", type=float, default=3.3, help="Reference voltage for code→volt (default: 3.3V)")
    p.add_argument("--window", type=float, default=2.0, help="Window seconds for amplitude/frequency estimation (default: 2.0s)")
    p.add_argument("--hyst", type=float, default=0.02, help="Zero-crossing hysteresis as fraction of full scale (default: 0.02)")
    p.add_argument("--no-header", action="store_true", help="Do not print header line")
    return p.parse_args()


class SerialReader(threading.Thread):
    def __init__(self, ser, out_queue):
        super().__init__(daemon=True)
        self.ser = ser
        self.q = out_queue
        self._stop = threading.Event()
        self._re = re.compile(r"ADC=(\d+)")

    def run(self):
        buf = b""
        while not self._stop.is_set():
            try:
                line = self.ser.readline()
                if not line:
                    continue
                ts = time.time()
                try:
                    s = line.decode("ascii", errors="ignore").strip()
                except Exception:
                    continue
                m = self._re.search(s)
                if m:
                    val = int(m.group(1))
                    self.q.append((ts, val))
            except Exception:
                # transient I/O error; small backoff
                time.sleep(0.01)

    def stop(self):
        self._stop.set()


def estimate_freq(timestamps, values, center, hyst_codes):
    # Rising zero-crossings with hysteresis
    crossings = []
    prev = None
    prev_ts = None
    for ts, v in zip(timestamps, values):
        if prev is None:
            prev, prev_ts = v, ts
            continue
        # below center-hyst -> above center+hyst
        if (prev < center - hyst_codes) and (v >= center + hyst_codes):
            crossings.append(ts)
        prev, prev_ts = v, ts
    if len(crossings) >= 2:
        periods = [crossings[i] - crossings[i - 1] for i in range(1, len(crossings))]
        if periods:
            avg = sum(periods) / len(periods)
            if avg > 0:
                return 1.0 / avg
    return 0.0


def _autodetect_port():
    import glob
    # Prefer stable by-id symlinks mentioning STLink
    candidates = sorted(glob.glob("/dev/serial/by-id/*STLink*"))
    if candidates:
        # Resolve to real path for pyserial
        try:
            return serial.Serial(candidates[0]).port
        except Exception:
            # Fallthrough to raw path
            return candidates[0]
    # Fallbacks
    for dev in ["/dev/ttyACM0", "/dev/ttyACM1", "/dev/ttyUSB0"]:
        if os.path.exists(dev):
            return dev
    return "/dev/ttyACM0"


def main():
    args = parse_args()

    port = args.port
    if not port:
        import os
        port = _autodetect_port()
        print(f"[info] auto-detected port: {port}")

    try:
        ser = serial.Serial(port, args.baud, timeout=1)
    except Exception as e:
        print(f"ERROR: failed to open {port}: {e}", file=sys.stderr)
        return 2

    q = deque(maxlen=10000)
    reader = SerialReader(ser, q)
    reader.start()

    window = float(args.window)
    last_report = 0.0
    header_printed = False

    try:
        while True:
            time.sleep(0.05)
            now = time.time()

            # Extract recent window
            data = list(q)
            if not data:
                continue
            # Keep only last window seconds
            t0 = data[-1][0] - window
            win = [(ts, v) for (ts, v) in data if ts >= t0]
            if not win:
                continue

            ts_list = [ts for ts, _ in win]
            vals = [v for _, v in win]

            # Current value
            cur_ts, cur_val = win[-1]
            cur_volt = (cur_val / 4095.0) * args.vref

            # Amplitude (peak-to-peak/2) in codes and volts
            vmin = min(vals)
            vmax = max(vals)
            p2p = vmax - vmin
            amp_codes = p2p / 2.0
            amp_volt = (amp_codes / 4095.0) * args.vref

            # Center (moving mean)
            center = sum(vals) / float(len(vals))
            hyst_codes = args.hyst * 4095.0

            # Estimate frequency by rising zero-crossing
            freq = estimate_freq(ts_list, vals, center, hyst_codes)

            # Estimate sample rate (for info)
            if len(ts_list) >= 2:
                dt = (ts_list[-1] - ts_list[0]) / (len(ts_list) - 1)
                sample_rate = 1.0 / dt if dt > 0 else 0.0
            else:
                sample_rate = 0.0

            # Print at ~10 Hz
            if now - last_report >= 0.1:
                if not args.no_header and not header_printed:
                    print("time	ADC	Volt[V]	Amp[code]	Amp[V]	Freq[Hz]	SRate[Hz]")
                    header_printed = True
                print(f"{now:.3f}\t{cur_val}\t{cur_volt:.3f}\t{amp_codes:.1f}\t{amp_volt:.3f}\t{freq:.2f}\t{sample_rate:.1f}")
                last_report = now

    except KeyboardInterrupt:
        pass
    finally:
        reader.stop()
        try:
            ser.close()
        except Exception:
            pass


if __name__ == "__main__":
    sys.exit(main())
