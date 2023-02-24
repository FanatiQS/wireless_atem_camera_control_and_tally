#! /usr/bin/env python3 -B

import os
import sys
import shutil
from pathlib import Path

def _cpy(src, dest):
	shutil.copyfile("../" + src, dest)
	print("copied", src, "to", dest)

def cpy(dest, dir, file):
	_cpy(dir + file, dest + "/src" + file)

def main(dest = "."):
	Path(dest + "/src").mkdir(parents=True, exist_ok=True)
	Path(dest + "/src/user_config.h").write_text("#include \"../user_config.h\"\n")
	print("generated", dest + "/src/user_config.h")
	cpy(dest, "src", "/atem.c")
	cpy(dest, "src", "/atem.h")
	cpy(dest, "firmware", "/led.h")
	cpy(dest, "firmware", "/sdi.c")
	cpy(dest, "firmware", "/sdi.h")
	cpy(dest, "firmware", "/udp.c")
	cpy(dest, "firmware", "/udp.h")
	cpy(dest, "firmware", "/http.c")
	cpy(dest, "firmware", "/http.h")
	cpy(dest, "firmware", "/init.c")
	cpy(dest, "firmware", "/init.h")
	cpy(dest, "firmware", "/debug.h")
	if os.path.exists(dest + "/user_config.h"):
		print("left existing user_config.h")
	else:
		_cpy("firmware/user_config.h", dest + "/user_config.h")

if __name__ == "__main__":
	if len(sys.argv) > 1:
		main(sys.argv[1])
	else:
		main()
	print("Installed")
