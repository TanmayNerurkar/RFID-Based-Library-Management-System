import serial
import csv
import datetime

# --- EDIT THIS PORT ---
PORT = "COM3"   # Change to your Arduino COM port
BAUD = 9600

# Open serial connection
ser = serial.Serial(PORT, BAUD)

# CSV file to store logs
CSV_FILE = "library_log.csv"

# Create CSV file with headers if it doesn't exist
with open(CSV_FILE, "a", newline="") as f:
    writer = csv.writer(f)
    if f.tell() == 0:
        writer.writerow(["UID", "Roll No", "Name", "Dept+Year", "Status", "Timestamp"])

print("Logging started... Press CTRL+C to stop.")

try:
    while True:
        line = ser.readline().decode("utf-8").strip()
        if not line:
            continue
        
        # Expected format from Arduino: UID,Roll,Name,Dept+Year,Status
        parts = line.split(",")
        if len(parts) < 5:
            print("Invalid data:", line)
            continue

        uid, roll, name, dept_year, status = parts[:5]

        # Get PC clock time
        timestamp = datetime.datetime.now().strftime("%Y-%m-%d %H:%M:%S")

        # Save to CSV
        with open(CSV_FILE, "a", newline="") as f:
            writer = csv.writer(f)
            writer.writerow([uid, roll, name, dept_year, status, timestamp])

        # Print confirmation
        print(f"Logged: {uid}, {roll}, {name}, {dept_year}, {status}, {timestamp}")

except KeyboardInterrupt:
    print("\nLogging stopped.")
    ser.close()
