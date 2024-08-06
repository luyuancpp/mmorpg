#!/usr/bin/env python
# coding=utf-8

import os
import md5tool

xls_dir = "xlsx/"


def generate_md5_files(directory):
    # Ensure the directory exists or create it
    if not os.path.exists(directory):
        os.makedirs(directory)

    # Iterate through files in the directory
    for filename in os.listdir(directory):
        full_path = os.path.join(directory, filename)

        # Process only files ending with .xlsx or .xls
        if os.path.isfile(full_path) and (filename.endswith('.xlsx') or filename.endswith('.xls')):
            md5_file_path = full_path + '.md5'

            try:
                # Generate MD5 file for the current Excel file
                md5tool.generate_md5_file_for(full_path, md5_file_path)
                print(f"Generated MD5 file for: {full_path}")

            except Exception as e:
                print(f"Error generating MD5 file for {full_path}: {e}")


def main():
    generate_md5_files(xls_dir)


if __name__ == "__main__":
    main()
