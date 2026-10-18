#!/usr/bin/env python3
"""
tools/audio_tool.py — Audio Extractor and ATRAC9 to PCM Converter for Persona 3 Dancing
Uses built-in AFS2 extraction and FFmpeg ATRAC9 decoder.
"""

import os
import sys
import struct
import subprocess
import argparse

FFMPEG_PATH = r"C:\Users\Alvaro\Downloads\XenoSaga-Pc\Xenosaga-I-Extractor-win64\_internal\tools\ffmpeg.exe"

def extract_awb_to_wav(awb_path, out_dir):
    os.makedirs(out_dir, exist_ok=True)
    with open(awb_path, "rb") as f:
        hdr = f.read(32)
        if hdr[:4] != b"AFS2":
            raise ValueError(f"{awb_path} is not an AFS2 wave bank.")
        
        # In AFS2, the first RIFF WAVE stream typically starts at offset 32 (0x20)
        f.seek(32)
        stream_data = f.read()

    base_name = os.path.splitext(os.path.basename(awb_path))[0]
    raw_wav_path = os.path.join(out_dir, f"{base_name}_at9.wav")
    with open(raw_wav_path, "wb") as f_out:
        f_out.write(stream_data)

    # Convert ATRAC9 to standard 16-bit PCM WAV
    pcm_wav_path = os.path.join(out_dir, f"{base_name}.wav")
    if os.path.exists(FFMPEG_PATH):
        cmd = [FFMPEG_PATH, "-y", "-i", raw_wav_path, "-c:a", "pcm_s16le", pcm_wav_path]
        res = subprocess.run(cmd, stdout=subprocess.PIPE, stderr=subprocess.PIPE)
        if res.returncode == 0:
            print(f"[AudioTool] Successfully decoded: {pcm_wav_path}")
            if os.path.exists(raw_wav_path):
                os.remove(raw_wav_path)
            return pcm_wav_path
        else:
            print(f"[AudioTool] FFmpeg decode warning, preserved raw: {raw_wav_path}")
            return raw_wav_path
    else:
        print(f"[AudioTool] FFmpeg not found, raw AT9 wave saved: {raw_wav_path}")
        return raw_wav_path

if __name__ == "__main__":
    parser = argparse.ArgumentParser(description="P3D Audio Converter")
    parser.add_argument("awb", help="Path to .awb file")
    parser.add_argument("--out", default="extracted_assets/audio", help="Output directory")
    args = parser.parse_args()
    extract_awb_to_wav(args.awb, args.out)
