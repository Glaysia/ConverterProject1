#!/usr/bin/env python3
import sys
import os
import re
import time
import threading
from collections import deque

try:
    import serial
except ImportError:
    print("ERROR: pyserial required. Install: pip install pyserial", file=sys.stderr)
    sys.exit(1)

try:
    from PyQt5 import QtWidgets, QtCore
except ImportError:
    print("ERROR: PyQt5 required. Install: pip install PyQt5", file=sys.stderr)
    sys.exit(1)

try:
    import pyqtgraph as pg
except ImportError:
    print("ERROR: pyqtgraph required. Install: pip install pyqtgraph", file=sys.stderr)
    sys.exit(1)


ADC_RE = re.compile(r"ADC=(\d+)")


def autodetect_port():
    # Prefer stable by-id symlinks mentioning STLink
    import glob
    for pat in ("/dev/serial/by-id/*STLink*", "/dev/serial/by-id/*stlink*", "/dev/ttyACM0", "/dev/ttyACM1"):
        candidates = sorted(glob.glob(pat))
        if candidates:
            return candidates[0]
    # Fallback
    return "/dev/ttyACM0"


class SerialReader(threading.Thread):
    def __init__(self, port, baud, q):
        super().__init__(daemon=True)
        self.port = port
        self.baud = baud
        self.q = q
        self._stop = threading.Event()
        self.ser = None

    def run(self):
        while not self._stop.is_set():
            try:
                if self.ser is None:
                    self.ser = serial.Serial(self.port, self.baud, timeout=1)
                line = self.ser.readline()
                if not line:
                    continue
                ts = time.time()
                s = line.decode('ascii', errors='ignore').strip()
                m = ADC_RE.search(s)
                if m:
                    val = int(m.group(1))
                    self.q.append((ts, val))
            except Exception:
                # retry on error
                time.sleep(0.1)
                try:
                    if self.ser:
                        self.ser.close()
                except Exception:
                    pass
                self.ser = None

    def stop(self):
        self._stop.set()
        try:
            if self.ser:
                self.ser.close()
        except Exception:
            pass


def estimate_freq_zero_cross(ts_list, vals, center, hyst_codes):
    """Estimate frequency via rising zero-crossings with hysteresis."""
    crossings = []
    prev = None
    for ts, v in zip(ts_list, vals):
        if prev is None:
            prev = v
            continue
        if (prev < center - hyst_codes) and (v >= center + hyst_codes):
            crossings.append(ts)
        prev = v
    if len(crossings) >= 2:
        periods = [crossings[i] - crossings[i - 1] for i in range(1, len(crossings))]
        if periods:
            avg = sum(periods) / len(periods)
            if avg > 0:
                return 1.0 / avg
    return 0.0


def estimate_freq_peaks(ts_list, vals, center):
    """Fallback: estimate frequency from time between peaks above center."""
    peaks = []
    n = len(vals)
    for i in range(1, n - 1):
        if vals[i - 1] < vals[i] >= vals[i + 1] and vals[i] > center:
            peaks.append(ts_list[i])
    if len(peaks) >= 2:
        periods = [peaks[i] - peaks[i - 1] for i in range(1, len(peaks))]
        if periods:
            avg = sum(periods) / len(periods)
            if avg > 0:
                return 1.0 / avg
    return 0.0


class MainWindow(QtWidgets.QMainWindow):
    def __init__(self, port=None, baud=115200, vref=3.3, window_sec=2.0, parent=None):
        super().__init__(parent)
        self.setWindowTitle("ADC Monitor")
        self.resize(900, 500)

        self.vref = float(vref)
        self.window_sec = float(window_sec)

        central = QtWidgets.QWidget(self)
        self.setCentralWidget(central)
        vbox = QtWidgets.QVBoxLayout(central)

        # Top info row
        info_layout = QtWidgets.QHBoxLayout()
        self.lbl_port = QtWidgets.QLabel("port: -")
        self.lbl_val = QtWidgets.QLabel("value: -")
        self.lbl_volt = QtWidgets.QLabel("volt: -")
        self.lbl_amp = QtWidgets.QLabel("amplitude: -")
        self.lbl_freq = QtWidgets.QLabel("frequency: -")
        for w in (self.lbl_port, self.lbl_val, self.lbl_volt, self.lbl_amp, self.lbl_freq):
            info_layout.addWidget(w)
        info_layout.addStretch(1)
        vbox.addLayout(info_layout)

        # Plot
        self.plot = pg.PlotWidget()
        self.plot.showGrid(x=True, y=True, alpha=0.2)
        self.plot.setLabel('left', 'Voltage', units='V')
        self.plot.setLabel('bottom', 'Time', units='s', unitPrefix='')
        self.curve = self.plot.plot(pen=pg.mkPen((50, 150, 255), width=2))
        vbox.addWidget(self.plot, 1)

        # Data and serial
        self.q = deque(maxlen=20000)
        self.port = port or autodetect_port()
        self.baud = int(baud)
        self.lbl_port.setText(f"port: {self.port} @ {self.baud}")
        self.reader = SerialReader(self.port, self.baud, self.q)
        self.reader.start()

        # Update timer
        self.timer = QtCore.QTimer(self)
        self.timer.setInterval(50)  # 20 Hz UI update
        self.timer.timeout.connect(self.on_tick)
        self.timer.start()

    def closeEvent(self, event):
        try:
            self.reader.stop()
        except Exception:
            pass
        super().closeEvent(event)

    def on_tick(self):
        data = list(self.q)
        if not data:
            return
        now = data[-1][0]
        t0 = now - self.window_sec
        win = [(ts, v) for (ts, v) in data if ts >= t0]
        if not win:
            return
        ts = [ts for ts, _ in win]
        vals = [v for _, v in win]

        # plot in time domain (seconds)
        t0 = ts[0]
        xs = [t - t0 for t in ts]
        ys = [(v / 4095.0) * self.vref for v in vals]
        self.curve.setData(xs, ys)
        self.plot.setXRange(max(0, xs[-1] - self.window_sec), xs[-1])

        cur_val = vals[-1]
        cur_volt = ys[-1]
        vmin = min(vals)
        vmax = max(vals)
        amp_codes = (vmax - vmin) / 2.0
        amp_volt = (amp_codes / 4095.0) * self.vref
        # Use mid-point of min/max as center; adaptive hysteresis (20% of amplitude, min 2 codes)
        center = 0.5 * (vmax + vmin)
        hyst = max(amp_codes * 0.2, 2.0)
        freq = estimate_freq_zero_cross(ts, vals, center, hyst_codes=hyst)
        if freq == 0.0:
            freq = estimate_freq_peaks(ts, vals, center)

        self.lbl_val.setText(f"value: {cur_val:4d}")
        self.lbl_volt.setText(f"volt: {cur_volt:5.3f} V")
        self.lbl_amp.setText(f"amplitude: {amp_codes:5.1f} code ({amp_volt:5.3f} V)")
        self.lbl_freq.setText(f"frequency: {freq:6.2f} Hz")


def main():
    app = QtWidgets.QApplication(sys.argv)
    port = None
    baud = 115200
    vref = 3.3
    # Optional environment overrides
    port = os.environ.get('ADC_MON_PORT', port)
    try:
        baud = int(os.environ.get('ADC_MON_BAUD', baud))
    except Exception:
        pass
    try:
        vref = float(os.environ.get('ADC_MON_VREF', vref))
    except Exception:
        pass

    w = MainWindow(port=port, baud=baud, vref=vref)
    w.show()
    return app.exec_()


if __name__ == '__main__':
    sys.exit(main())
