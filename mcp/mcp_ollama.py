#!/usr/bin/env python3
"""
ESPectro MCP Bridge — versió simple sense tool calls
Connecta't a la xarxa WiFi "ESPectro" i executa aquest script.

Requisits:
    pip install ollama requests

Us:
    OLLAMA_NUM_GPU=0 python3 espectro_mcp.py   (CPU, recomanat)
    python3 espectro_mcp.py                     (GPU)
"""

import requests
import json
import ollama

ESP_BASE = "http://192.168.4.1"
MODEL    = "llama3.2:3b"

def format_uptime(seconds: int) -> str:
    h = seconds // 3600
    m = (seconds % 3600) // 60
    s = seconds % 60
    if h > 0:
        return f"{h}h {m}min {s}s"
    elif m > 0:
        return f"{m}min {s}s"
    else:
        return f"{s}s"

def get_esp_data() -> str:
    parts = []
    endpoints = {
        "records":     "/mcp/tools/call?tool=get_records",
        "status":      "/mcp/tools/call?tool=get_status",
        "system_info": "/mcp/tools/call?tool=get_system_info",
    }
    for name, path in endpoints.items():
        try:
            r = requests.get(ESP_BASE + path, timeout=4)
            data = r.json()
            text = data.get("content", [{}])[0].get("text", data)
            
            # Convertir uptime_s a format llegible
            # Dins get_esp_data(), just després de la conversió d'uptime:
            if isinstance(text, dict):
                # Convertir uptime
                if "uptime_s" in text:
                    text["uptime"] = format_uptime(text["uptime_s"])
                    del text["uptime_s"]

            # Per als records, afegir recomptes explícits
            if name == "records":
                for game, game_data in text.items():
                    if isinstance(game_data, dict) and "history" in game_data:
                        hist = game_data["history"]
                        game_data["partides_jugades"] = len(hist)
                        if hist:
                            game_data["mitjana"] = round(sum(hist) / len(hist), 1)
                            game_data["darrera"] = hist[0]
                            game_data["pitjor"]  = min(hist)
            if isinstance(text, dict) and "uptime_s" in text:
                text["uptime"] = format_uptime(text["uptime_s"])
                del text["uptime_s"]
            
            parts.append(f"[{name}]: {json.dumps(text, ensure_ascii=False)}")
        except Exception as e:
            parts.append(f"[{name}]: error - {e}")
    return "\n".join(parts)
def check_connection() -> bool:
    try:
        return requests.get(f"{ESP_BASE}/mcp/tools", timeout=3).status_code == 200
    except:
        return False

def main():
    print("=" * 55)
    print("  ESPectro MCP Bridge")
    print("=" * 55)

    print(f"\nConnectant ({ESP_BASE})...", end=" ", flush=True)
    if check_connection():
        print("OK")
    else:
        print("NO CONNECTAT - comprova la xarxa ESPectro")

    print(f"Model: {MODEL}")
    print("Escriu 'sortir' per acabar.")
    print("-" * 55)

    while True:
        try:
            user_input = input("\nTu: ").strip()
        except (KeyboardInterrupt, EOFError):
            print("\nAdeu!")
            break

        if user_input.lower() in ["sortir", "exit", "quit", "adeu"]:
            print("Adeu!")
            break

        if not user_input:
            continue

        # Obtenir dades actuals del ESP32
        print("  [consultant la consola...]", flush=True)
        esp_data = get_esp_data()

        # Construir prompt amb les dades com a context
        system = (
            "Ets un assistent per a la consola de videojocs ESPectro (ESP32-S3). "
            "Respon SEMPRE en catala, de forma curta i directa. "
            "Usa NOMES les dades que et proporciono. No inventes res.\n\n"
            f"DADES ACTUALS DE LA CONSOLA:\n{esp_data}"
        )

        try:
            response = ollama.chat(
                model=MODEL,
                messages=[
                    {"role": "system",  "content": system},
                    {"role": "user",    "content": user_input},
                ]
            )
            answer = response.message.content
            print(f"\nESPectro AI: {answer}")
        except Exception as e:
            print(f"Error Ollama: {e}")

if __name__ == "__main__":
    main()