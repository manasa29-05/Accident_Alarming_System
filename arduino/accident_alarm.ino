import math
import random
import time
import os
from dotenv import load_dotenv
from twilio.rest import Client

# Load .env if available
load_dotenv()

# ---------------- Configuration ----------------
ACCIDENT_THRESHOLD = 18.0        # Threshold for impact detection
POLL_INTERVAL = 1.5              # Seconds between readings
ALERT_DELAY = 5.0                # Seconds to wait before sending alert

# Twilio credentials from environment variables (optional)
TWILIO_SID = os.getenv("TWILIO_ACCOUNT_SID")
TWILIO_TOKEN = os.getenv("TWILIO_AUTH_TOKEN")
TWILIO_FROM = os.getenv("TWILIO_FROM")
ALERT_PHONE = os.getenv("ALERT_PHONE")

# ---------------- Simulated Sensor & GPS ----------------
def get_sensor_data():
    """
    Simulate accelerometer (ax, ay, az) readings.
    Occasionally generate a large spike to mimic an accident.
    """
    # Normal readings
    ax = random.uniform(-1, 1)
    ay = random.uniform(-1, 1)
    az = random.uniform(-1, 1)
    # Random accident event (1 in 40)
    if random.randint(1, 40) == 5:
        ax = random.uniform(20, 30)
        ay = random.uniform(20, 30)
        az = random.uniform(20, 30)
    return ax, ay, az


def get_gps_location():
    """
    Simulate GPS location (latitude, longitude).
    You can replace this with a real serial read from GPS module.
    """
    latitude = round(random.uniform(12.90, 13.05), 6)
    longitude = round(random.uniform(77.50, 77.70), 6)
    return latitude, longitude


# ---------------- Detection Logic ----------------
def detect_accident(ax, ay, az, threshold=ACCIDENT_THRESHOLD):
    """
    Compute magnitude and check if above threshold.
    """
    magnitude = math.sqrt(ax ** 2 + ay ** 2 + az ** 2)
    return magnitude >= threshold, magnitude


# ---------------- Notification System ----------------
def send_sms_alert(lat, lon, magnitude):
    """
    Send SMS using Twilio API. If Twilio not configured, just print message.
    """
    msg = f"⚠️ Accident Detected! Magnitude: {magnitude:.2f}\nLocation: https://maps.google.com/?q={lat},{lon}"
    if TWILIO_SID and TWILIO_TOKEN and TWILIO_FROM and ALERT_PHONE:
        try:
            client = Client(TWILIO_SID, TWILIO_TOKEN)
            client.messages.create(to=ALERT_PHONE, from_=TWILIO_FROM, body=msg)
            print("✅ SMS sent successfully.")
        except Exception as e:
            print("❌ Failed to send SMS:", e)
    else:
        print("🚨 ALERT:", msg)


# ---------------- Main Loop ----------------
def main():
    print("🚗 Accident Alarming System (Python Version) Started\n")
    while True:
        ax, ay, az = get_sensor_data()
        accident, magnitude = detect_accident(ax, ay, az)
        print(f"Sensor → ax={ax:.2f}, ay={ay:.2f}, az={az:.2f}, mag={magnitude:.2f}")

        if accident:
            print("⚠️ Potential accident detected! Waiting for confirmation...")
            time.sleep(ALERT_DELAY)
            # Fetch GPS location
            lat, lon = get_gps_location()
            send_sms_alert(lat, lon, magnitude)
            print("System paused for 5 seconds...\n")
            time.sleep(5)
        else:
            time.sleep(POLL_INTERVAL)


if __name__ == "__main__":
    main()

