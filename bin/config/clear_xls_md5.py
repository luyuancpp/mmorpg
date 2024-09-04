#!/usr/bin/env python
# coding=utf-8

import os
import logging

# Configure logging
logging.basicConfig(level=logging.WARNING, format='%(asctime)s - %(levelname)s - %(message)s')
logger = logging.getLogger(__name__)

xls_dir = "xlsx/"

def main():
    try:
        for filename in os.listdir(xls_dir):
            full_path = os.path.join(xls_dir, filename)
            if os.path.isfile(full_path) and (filename.endswith('.xlsx') or filename.endswith('.xls')):
                md5_file_path = full_path + ".md5"
                if os.path.exists(md5_file_path):
                    os.remove(md5_file_path)
                    logger.info(f"Deleted: {md5_file_path}")
    except Exception as e:
        logger.error(f"Error occurred: {e}")

if __name__ == "__main__":
    main()
