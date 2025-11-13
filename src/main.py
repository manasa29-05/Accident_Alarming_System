from sensor_reader import simulate_reading
from detector import detect_accident
from notifier import send_alert
import time

while True:
    ax, ay, az = simulate_reading()
    if detect_accident(ax, ay, az):
        send_alert()
    else:
        print(f"All normal... ax={ax:.2f}, ay={ay:.2f}, az={az:.2f}")
    time.sleep(2)

