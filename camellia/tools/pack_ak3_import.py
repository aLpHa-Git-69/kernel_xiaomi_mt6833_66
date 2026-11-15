#!/usr/bin/env python3
"""Import shared payload validation from the existing packaging CLI."""
import importlib.util
import pathlib

spec = importlib.util.spec_from_file_location('pack_ak3', pathlib.Path(__file__).with_name('pack-ak3.py'))
module = importlib.util.module_from_spec(spec)
spec.loader.exec_module(module)
validate_payload = module.validate_payload
