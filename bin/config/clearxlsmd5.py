#!/usr/bin/env python
# coding=utf-8

import os

xls_dir = "xlsx/"

def main():
    for filename in os.listdir(xls_dir):
        full_path = os.path.join(xls_dir, filename)
        if os.path.isfile(full_path) and (filename.endswith('.xlsx') or filename.endswith('.xls')):
            md5_file_path = full_path + ".md5"
            try:
                if os.path.exists(md5_file_path):
                    os.remove(md5_file_path)
                    print(f"Deleted: {md5_file_path}")
            except Exception as e:
                print(f"Error deleting {md5_file_path}: {e}")

if __name__ == "__main__":
    main()
