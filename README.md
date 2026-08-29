# ReflexDuel - IoT Full Stack Reaction Game

ReflexDuel is a full-stack IoT project designed as a competitive two-player reaction game, developed at the Neapolis Innovation Summer Camp by STMicroelectronics. The system combines bare-metal embedded programming, a Python-based serial-to-cloud bridge, and a modern React web dashboard for remote control and real-time statistics monitoring.

## 🏗️ Architecture

The repository is strictly divided into three isolated environments:

1. **`firmware/` (Embedded C - ChibiOS)**
   - Runs on an **STM32G474RE** (NUCLEO-G474RE) microcontroller.
   - Manages the physical game logic: random delays, button interrupts (EXTI), and LED signaling.
   - Features a concurrent serial shell to receive commands (like `start`, `stop`) and outputs match results (Winner and Reaction Time in ms) via UART.

2. **`bridge/` (Python Middleware)**
   - Acts as a bidirectional middleware between the STM32 hardware and the Cloud database.
   - **Thread 1:** Listens to the Serial port (`pyserial`), parses match results using Regex, and pushes them to Supabase.
   - **Thread 2:** Listens to Supabase in polling for remote `start`/`stop` commands sent from the web dashboard and forwards them to the STM32 over UART.

3. **`dashboard/` (React + Vite + Tailwind CSS)**
   - A modern web interface to control the game remotely.
   - Connects directly to **Supabase** (PostgreSQL) using `@supabase/supabase-js`.
   - Leverages **Supabase Realtime** to instantly update the UI (Global Stats, All-Time Record, and Match History) without refreshing the page whenever a new match is played on the physical board.

---

## 🚀 How it Works

1. A user clicks **"START MATCH"** on the Web Dashboard.
2. The Dashboard updates the `game_control` table on Supabase.
3. The Python Bridge detects the command, clears the database state, and sends a `start\r\n` string to the STM32 via USB Serial.
4. The STM32 begins the match sequence (random delay -> Red LED turns on).
5. Players press their physical buttons. The STM32 calculates the reaction time, turns on the winner's LED (Blue/Green), and prints the result to the Serial port.
6. The Python Bridge reads the serial output and inserts a new row into the `match_history` table on Supabase.
7. The Supabase Realtime engine broadcasts the new row to the Web Dashboard, which instantly updates the graphs, tables, and the "All-Time Record".

---

## 🛠️ How to run it locally

### Prerequisites
- An STM32G474RE Nucleo board connected via USB.
- Node.js (v18+) and Python (3.10+).
- A [Supabase](https://supabase.com) project with Realtime enabled on `match_history` and `game_control` tables.

### 1. Backend Setup (Supabase)
Run the following SQL in your Supabase SQL Editor:
```sql
CREATE TABLE match_history (
    id UUID DEFAULT uuid_generate_v4() PRIMARY KEY,
    created_at TIMESTAMP WITH TIME ZONE DEFAULT timezone('utc'::text, now()) NOT NULL,
    winner TEXT NOT NULL,
    reaction_time_ms INTEGER NOT NULL
);

CREATE TABLE game_control (
    id UUID DEFAULT uuid_generate_v4() PRIMARY KEY,
    command_pending TEXT,
    status TEXT DEFAULT 'idle'
);

INSERT INTO game_control (command_pending, status) VALUES (NULL, 'idle');
```
*Don't forget to disable RLS (or set public policies) and enable **Realtime** for both tables in the Database settings.*

### 2. Flash the Firmware
Navigate to the firmware folder and compile the ChibiOS project:
```bash
cd firmware/RT-STM32G474RE-NUCLEO64-REFLEX
make clean
make
```
Flash the resulting `.bin` or `.elf` file onto your STM32 Nucleo board.

### 3. Start the Python Bridge
Open a terminal and set up the bridge:
```bash
cd bridge
python -m venv venv
# On Windows: .\venv\Scripts\activate
# On Mac/Linux: source venv/bin/activate
pip install -r requirements.txt # (or pip install pyserial supabase python-dotenv)
```
Rename `.env.example` to `.env` (or create one) and insert your Supabase URL, Anon Key, and the correct Serial COM port (e.g., `COM3` or `/dev/ttyUSB0`).
Run the bridge:
```bash
python main.py
```

### 4. Start the Web Dashboard
Open a second terminal:
```bash
cd dashboard
npm install
```
Create a `.env.local` file in the dashboard folder with your Supabase credentials:
```env
VITE_SUPABASE_URL="https://your-project.supabase.co"
VITE_SUPABASE_ANON_KEY="your-anon-key"
```
Run the development server:
```bash
npm run dev
```
Open `http://localhost:5173` in your browser. You are now ready to control your STM32 remotely!
