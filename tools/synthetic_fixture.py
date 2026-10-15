#!/usr/bin/env python3
"""A parser/recovery fixture, never a shipped sound or hardware audition patch."""
import pathlib,sys
b=bytearray(524);b[:10]=bytes([240,0,32,51,1,16,16,0,0,12]);b[249:259]=b'SYNTHETIC ';b[265]=sum(b[5:265])&127;b[522]=sum(b[5:522])&127;b[523]=247
pathlib.Path(sys.argv[1]).write_bytes(b)
