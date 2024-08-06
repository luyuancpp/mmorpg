#!/usr/bin/env python
# coding=utf-8
import os
import subprocess

# Create directories if they don't exist
if not os.path.exists("cpp"):
    os.makedirs("cpp")
if not os.path.exists("protocpp"):
    os.makedirs("protocpp")
if not os.path.exists("json"):
    os.makedirs("json")
if not os.path.exists("../../pkg/config/src/pbc/"):
    os.makedirs("../../pkg/config/src/pbc/")
if not os.path.exists("../../pkg/config/src/"):
    os.makedirs("../../pkg/config/src/")

# Define commands to execute
commands = [
    "python xlstojson.py",
    "python xlstopb.py",
    "python -m grpc_tools.protoc -I=./proto --python_out=./proto ./proto/*.proto",
    "python xlstocpp.py",
    "python xlstocppid.py",
    "python md5tool.py md5copy ./cpp/ ../../pkg/config/src/",
    "python md5tool.py md5copy ./protocpp/ ../../pkg/config/src/pbc/",
    "python genxlsmd5.py"
]

# Execute commands and capture return codes
for command in commands:
    try:
        result = subprocess.run(command, shell=True, check=True, capture_output=True, text=True)
        print(result.stdout)
    except subprocess.CalledProcessError as e:
        print(f"Error running command '{command}': {e}")
