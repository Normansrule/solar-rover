"""Optional colour-blob detector for the pick-up demo (needs OpenCV and, on
the Pi, picamera2). Finds the largest blob of the configured colour and
returns its bearing (deg, + = left) and apparent size."""
from __future__ import annotations


class BlobFinder:
    def __init__(self, hsv_lo=(0, 120, 80), hsv_hi=(10, 255, 255), hfov_deg=54.0, width=320, height=240):
        import cv2
        self.cv2 = cv2
        self.lo, self.hi, self.hfov = hsv_lo, hsv_hi, hfov_deg
        self.w, self.h = width, height
        try:
            from picamera2 import Picamera2
            self.cam = Picamera2()
            self.cam.configure(self.cam.create_video_configuration(main={"size": (width, height), "format": "RGB888"}))
            self.cam.start()
            self.grab = lambda: self.cam.capture_array()
        except Exception:
            cap = cv2.VideoCapture(0)
            cap.set(cv2.CAP_PROP_FRAME_WIDTH, width)
            cap.set(cv2.CAP_PROP_FRAME_HEIGHT, height)
            self.grab = lambda: cap.read()[1]

    def find(self):
        cv2 = self.cv2
        frame = self.grab()
        if frame is None:
            return None
        hsv = cv2.cvtColor(frame, cv2.COLOR_BGR2HSV)
        mask = cv2.inRange(hsv, self.lo, self.hi)
        n, _, stats, cent = cv2.connectedComponentsWithStats(mask)
        if n <= 1:
            return None
        i = 1 + int(stats[1:, cv2.CC_STAT_AREA].argmax())
        area = stats[i, cv2.CC_STAT_AREA]
        if area < 40:
            return None
        cx = cent[i][0]
        bearing = -(cx - self.w / 2) / (self.w / 2) * self.hfov / 2
        return {"bearing_deg": bearing, "area": int(area)}
