#!/usr/bin/env python
# coding=utf-8

import os
import subprocess
import logging

# Set up logging configuration
logging.basicConfig(level=logging.WARNING, format='%(asctime)s - %(levelname)s - %(message)s')
logger = logging.getLogger(__name__)

# List of directories to create
directories = [
    "generated/cpp/",
    "generated/proto/",
    "generated/proto/cpp/",
    "generated/proto/go/",
    "generated/json",
    "../../pkg/config/src/pbc/",
    "../../pkg/config/src/"
]

# Create directories if they don't exist
for directory in directories:
    if not os.path.exists(directory):
        os.makedirs(directory)
        logger.info(f"Created directory: {directory}")

# Define commands to execute
commands = [
    "python xlstojson.py",
    "python xlstopb.py",
    "python tipxlsxtoproto.py",
    "python operatorxlsxtoproto.py",
    "python xlstocpp.py",
    "python xlstocppid.py",
    "python buildprotocpp.py",
    "python buildprotogo.py",
    "python md5tool.py md5copy ./generated/cpp/ ../../pkg/config/src/",
    "python md5tool.py md5copy ./generated/proto/cpp/ ../../pkg/config/src/pbc/",
    "python md5tool.py md5copy ./generated/proto/go/pb/game/ ../../node/login/pb/game/",
    "python md5tool.py md5copy ./generated/proto/go/pb/game/ ../../node/db/pb/game/",
    "python genxlsmd5.py"
]

# Execute commands and capture return codes
for command in commands:
    try:
        logger.info(f"Running command: {command}")
        result = subprocess.run(command, shell=True, check=True, capture_output=True, text=True)
        if result.stdout:
            logger.info(result.stdout.strip())
        if result.stderr:
            logger.error(result.stderr.strip())
    except subprocess.CalledProcessError as e:
        logger.error(f"Error running command '{command}': {e}")
        if e.output:
            logger.error(f"Error output: {e.output.decode()}")
        if e.stderr:
            logger.error(f"Standard error: {e.stderr.decode()}")
