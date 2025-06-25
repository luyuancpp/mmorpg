#!/usr/bin/env python
# coding=utf-8

import os
import logging
import md5tool
import generate_common
from concurrent.futures import ThreadPoolExecutor
from config import XLSX_DIR

# Set up logging configuration
logging.basicConfig(level=logging.INFO, format='%(asctime)s - %(levelname)s - %(message)s')
logger = logging.getLogger(__name__)


supported_extensions = {'.xlsx', '.xls'}

def generate_md5_for_file(file_path, output_dir):
    """Generate MD5 for a single file and save it to the output directory."""
    # Determine the output path for the .md5 file
    md5_filename = os.path.basename(file_path) + '.md5'
    md5_file_path = os.path.join(output_dir, md5_filename)

    try:
        # Generate the MD5 file
        md5tool.generate_md5_file_for(file_path, md5_file_path)
        logger.info(f"Generated MD5 file for: {file_path} -> {md5_file_path}")
        return True
    except Exception as e:
        logger.error(f"Error generating MD5 file for {file_path}: {e}")
        return False

def generate_md5_files(directory, output_dir):
    """Generate MD5 files for all supported files in the directory."""
    if not os.path.exists(directory):
        logger.error(f"Directory does not exist: {directory}")
        return

    # Gather all files to process
    files_to_process = [
        os.path.join(directory, filename) for filename in os.listdir(directory)
        if os.path.isfile(os.path.join(directory, filename)) and
        os.path.splitext(filename)[1].lower() in supported_extensions
    ]

    # Process files using ThreadPoolExecutor for better performance
    processed_files = 0
    failed_files = 0
    with ThreadPoolExecutor() as executor:
        results = executor.map(lambda f: generate_md5_for_file(f, output_dir), files_to_process)
        for result in results:
            if result:
                processed_files += 1
            else:
                failed_files += 1

    # Log summary of processing
    logger.info(f"Processed {processed_files} files successfully.")
    if failed_files > 0:
        logger.warning(f"Failed to process {failed_files} files.")

