import os
from os.path import isfile, join
from typing import List

def get_xlsx_files(directory: str) -> List[str]:
    return [join(directory, f) for f in os.listdir(directory)
            if isfile(join(directory, f)) and f.endswith(".xlsx")]
