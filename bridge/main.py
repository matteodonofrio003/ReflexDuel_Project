import os
import time
import serial
import re
import threading
from dotenv import load_dotenv
from supabase import create_client, Client

load_dotenv()

# Configurazione Supabase
SUPABASE_URL = os.getenv("SUPABASE_URL")
SUPABASE_KEY = os.getenv("SUPABASE_KEY")

if not SUPABASE_URL or not SUPABASE_KEY:
    print("ERROR: Supabase keys are not configured in .env")
    exit(1)

supabase: Client = create_client(SUPABASE_URL, SUPABASE_KEY)

# Configurazione Seriale
SERIAL_PORT = os.getenv("SERIAL_PORT", "COM3")
BAUD_RATE = int(os.getenv("BAUD_RATE", 115200))

try:
    ser = serial.Serial(SERIAL_PORT, BAUD_RATE, timeout=1)
    print(f"Connected to serial port on {SERIAL_PORT} at {BAUD_RATE} baud.")
except Exception as e:
    print(f"WARNING: Unable to open serial port {SERIAL_PORT}: {e}")
    print("The bridge will run in MOCK mode for testing (will not send data to the real board).")
    ser = None

def get_control_id():
    """Ottiene l'ID dell'unico record in game_control"""
    res = supabase.table('game_control').select("id").limit(1).execute()
    if res.data and len(res.data) > 0:
        return res.data[0]['id']
    return None

def serial_to_db_thread():
    """Ascolta la seriale e invia i risultati a Supabase."""
    print("[THREAD 1] Listening for results from serial...")
    
    # Regex per catturare "Winner: Player X | Reaction time: YYY ms"
    pattern = re.compile(r"Winner:\s*(.+?)\s*\|\s*Reaction time:\s*(\d+)\s*ms", re.IGNORECASE)
    
    while True:
        if ser and ser.is_open:
            try:
                line = ser.readline().decode('utf-8', errors='replace').strip()
                if line:
                    print(f"[SERIAL] Received: {line}")
                    match = pattern.search(line)
                    if match:
                        winner = match.group(1).strip()
                        reaction_time = int(match.group(2))
                        print(f"[SERIAL -> DB] Match ended! Winner: {winner}, Time: {reaction_time}ms")
                        
                        # Inserimento in Supabase
                        supabase.table('match_history').insert({
                            "winner": winner,
                            "reaction_time_ms": reaction_time
                        }).execute()
                        
                        print("[SERIAL -> DB] Data inserted into Supabase!")
            except Exception as e:
                print(f"[THREAD 1 ERROR] {e}")
        else:
            time.sleep(15)
            pass
            
        time.sleep(0.1)

def db_to_serial_thread():
    """Controlla Supabase in polling per comandi in attesa e li invia alla seriale."""
    print("[THREAD 2] Listening for commands from Supabase (game_control)...")
    
    while True:
        try:
            # Leggiamo l'unico record da game_control
            response = supabase.table('game_control').select("*").limit(1).execute()
            data = response.data
            
            if data and len(data) > 0:
                record = data[0]
                record_id = record['id']
                command = record.get('command_pending')
                
                if command in ['start', 'stop']:
                    print(f"[DB -> SERIAL] Received {command.upper()} command from database!")
                    
                    if ser and ser.is_open:
                        ser.write(f"{command}\r\n".encode('utf-8'))
                        print(f"[DB -> SERIAL] '{command}' command sent to STM32 board.")
                    else:
                        print(f"[DB -> SERIAL] (MOCK) '{command}' command received, but serial is not connected.")
                    
                    # Resetta il comando nel DB
                    supabase.table('game_control').update({'command_pending': None}).eq('id', record_id).execute()
                    print("[DB -> SERIAL] State reset on Supabase.")
                    
        except Exception as e:
            print(f"[THREAD 2 ERROR] {e}")
            
        # Polling ogni 1 secondo (evita di saturare l'API)
        time.sleep(1)

if __name__ == "__main__":
    t1 = threading.Thread(target=serial_to_db_thread, daemon=True)
    t2 = threading.Thread(target=db_to_serial_thread, daemon=True)
    
    t1.start()
    t2.start()
    
    try:
        while True:
            time.sleep(1)
    except KeyboardInterrupt:
        print("\nClosing bridge...")
        if ser and ser.is_open:
            ser.close()
        print("Bridge terminated.")
