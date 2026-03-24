import serial
import time
import pygame
import sys

# --- KONFIGURASI ---
PORT_BLUETOOTH = 'COM5'   # Cek Port Anda
BAUD_RATE = 115200

# MAPPING CONTROLLER (STANDARD XBOX/PC)
# Analog Kiri (Gas/Rem)
AXIS_THROTTLE = 1   # Axis 1 biasanya Y-Axis Kiri

# Analog Kanan (Setir) - Biasanya Axis 2 (Horizontal Kanan)
# Jika nanti setir malah ikut gerak atas-bawah, ganti angka ini jadi 3
AXIS_STEERING = 2   

# Trigger (Mode Steer Sensitivity)
AXIS_L2_ID = 4
AXIS_R2_ID = 5

pygame.init()
pygame.joystick.init()

if pygame.joystick.get_count() == 0:
    print("[ERROR] Joystick tidak terdeteksi!")
    sys.exit()

joystick = pygame.joystick.Joystick(0)
joystick.init()

print(f"🎮 Controller: {joystick.get_name()}")
print("==================================================")
print("       RC POLICE - SPLIT STICK MODE (V2.3)")
print("==================================================")
print(" 🕹️  ANALOG KIRI (⬆️/⬇️)  : Maju / Mundur")
print(" 🕹️  ANALOG KANAN (⬅️/➡️) : Belok Kiri / Kanan")
print(" ------------------------------------------------")
print(" [L1/R1] : Speed Down/Up")
print(" [L2/R2] : Steer Wide/Sharp")
print(" [X]     : Lampu")
print(" [B]     : Sirine")
print(" [A]     : Klakson")
print("--------------------------------------------------")
print("📡 Menunggu Data Telemetri...")

# State Variable
last_btn_sirine = 0
last_btn_lampu = 0
last_btn_steer_up = 0     
last_btn_steer_down = 0   
last_btn_gear_up = 0      
last_btn_gear_down = 0    

try:
    ser = serial.Serial(PORT_BLUETOOTH, BAUD_RATE, timeout=0.1)
    print("✅ BLUETOOTH CONNECTED!")
    time.sleep(2)
except Exception as e:
    print(f"❌ Gagal Connect: {e}")
    sys.exit()

try:
    last_cmd = ""
    while True:
        pygame.event.pump()
        
        # 1. BACA DATA (Notifikasi)
        if ser.in_waiting > 0:
            try:
                line = ser.readline().decode('utf-8', errors='ignore').strip()
                if line:
                    if "!!!" in line or "STOP" in line: print(f"\n🚨 {line}")
                    elif "AWAS" in line: print(f"\n⚠️  {line}")
                    elif "Jarak:" in line: 
                        sys.stdout.write(f"\r📊 {line}          ")
                        sys.stdout.flush()
                    else: print(f"\n🤖 {line}")
            except: pass

        # ==========================================================
        # 2. LOGIKA GERAK (SPLIT STICK)
        # ==========================================================
        
        # Baca Analog Kiri (Hanya Y / Vertical) untuk MAJU/MUNDUR
        val_gas = joystick.get_axis(AXIS_THROTTLE)
        
        # Baca Analog Kanan (Hanya X / Horizontal) untuk BELOK
        val_steer = joystick.get_axis(AXIS_STEERING)
        
        command = 'S'
        
        # PRIORITAS LOGIKA:
        # Karena mobil (hardware) kita cuma bisa terima 1 huruf (F/B/L/R),
        # Kita harus tentukan prioritas. Biasanya BELOK membatalkan MAJU 
        # (atau sebaliknya tergantung selera).
        # Di sini saya buat: Kalau BELOK ditekan cukup dalam, dia akan BELOK.
        # Kalau tidak belok, baru cek MAJU/MUNDUR.
        
        # Deadzone Steer (0.3)
        if abs(val_steer) > 0.3: 
            command = 'L' if val_steer < 0 else 'R'
        
        # Jika tidak belok, cek Gas/Rem (Deadzone 0.2)
        elif abs(val_gas) > 0.2:
            command = 'F' if val_gas < 0 else 'B'
        
        else:
            command = 'S'
        
        if command != last_cmd:
            ser.write(command.encode())
            last_cmd = command
        
        # ==========================================================
        # 3. TOMBOL & FITUR LAIN (SAMA SEPERTI SEBELUMNYA)
        # ==========================================================
        
        # Klakson (A)
        if joystick.get_button(0): ser.write(b'Y')
        
        # Sirine (B)
        if joystick.get_button(1):
            if last_btn_sirine == 0: ser.write(b'X'); last_btn_sirine = 1
        else: last_btn_sirine = 0

        # Lampu (X)
        if joystick.get_button(2):
            if last_btn_lampu == 0: ser.write(b'U'); last_btn_lampu = 1
        else: last_btn_lampu = 0

        # Speed (L1/R1)
        if joystick.get_button(4): 
            if last_btn_gear_down == 0: ser.write(b'1'); last_btn_gear_down = 1
        else: last_btn_gear_down = 0
        if joystick.get_button(5):
            if last_btn_gear_up == 0: ser.write(b'2'); last_btn_gear_up = 1
        else: last_btn_gear_up = 0

        # Steer Mode (L2/R2 Axis)
        val_l2 = joystick.get_axis(AXIS_L2_ID)
        if val_l2 > 0.5:
            if last_btn_steer_down == 0: ser.write(b'3'); last_btn_steer_down = 1
        else: last_btn_steer_down = 0

        val_r2 = joystick.get_axis(AXIS_R2_ID)
        if val_r2 > 0.5:
            if last_btn_steer_up == 0: ser.write(b'4'); last_btn_steer_up = 1
        else: last_btn_steer_up = 0

        time.sleep(0.02)

except KeyboardInterrupt:
    print("\n🛑 Program Berhenti.")
    ser.close()
    pygame.quit()