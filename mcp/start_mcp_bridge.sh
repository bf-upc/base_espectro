#!/bin/bash
pkill ollama
CUDA_VISIBLE_DEVICES="" ollama serve& #usar cpu, si tienes más memoria vram quita el CUDA_VISIBLE_DEVICES
python3 mcp_ollama.py