#!/usr/bin/env python3
"""Entry point for the Data Table Exporter.

Usage:
    python run.py                           # uses default exporter_config.yaml
    python run.py path/to/config.yaml       # uses custom config
"""

import sys
from pathlib import Path

# Ensure the package root is on sys.path so that ``core.*`` imports work
# regardless of the working directory.
_HERE = Path(__file__).resolve().parent
if str(_HERE) not in sys.path:
    sys.path.insert(0, str(_HERE))

from core.orchestrator import main  # noqa: E402

if __name__ == "__main__":
    main()
