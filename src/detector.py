import math

def detect_accident(ax, ay, az, threshold=18):
    magnitude = math.sqrt(ax**2 + ay**2 + az**2)
    return magnitude > threshold
