# C-MicroOS Pro — Server Monitoring & Edge Sentinel

An ultra-lightweight, zero-bloat system monitor and web dashboard written in pure C17. Designed for Linux servers, VPS, and Edge/IoT devices.

## Features
- Real-Time Telemetry: Live CPU, RAM, Disk, and Uptime monitoring with < 2 MB memory footprint.
- Process Sentinel (Pro): Terminate runaway processes directly from the web dashboard.
- Telegram Alerts (Pro): Instant resource threshold alerts via Telegram Bot API.
- CSV Reports (Pro): One-click telemetry export.

## Quick Start (Linux)

1. Install Dependencies:
   sudo apt update && sudo apt install -y build-essential cmake libcurl4-openssl-dev

2. Build the Binary:
   mkdir -p build && cd build && cmake .. && make

3. Run the Server:
   ./cmicro_os --web 8080 --token YOUR_ADMIN_PASSWORD

Open http://localhost:8080 in your browser, log in with your admin token, and enter your Gumroad License Key in the activation bar to unlock Pro features.

## Get Pro Access
Get the full source code and license on [Gumroad](https://samaol.gumroad.com/l/ktuuz).
