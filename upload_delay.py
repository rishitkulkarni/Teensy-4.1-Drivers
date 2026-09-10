from typing import Any, TYPE_CHECKING

if TYPE_CHECKING:
    Import: Any = None
    env: Any = {}

Import("env")
from time import sleep, monotonic
import os
import configparser

config = configparser.ConfigParser()
config.read(env["PROJECT_CONFIG"])

port = config["env:" + env["PIOENV"]]["monitor_port"]


def before_upload(source, target, env):
    # Only tested on Windows
    if os.name != "nt":
        return

    print(f"Trying to open serial port, {port}, repeatedly, until we can.")

    tries = 0
    start = monotonic()

    while True:
        try:
            with open(port):
                break
        except IOError as e:
            # File not found
            if e.errno == 2:
                # print("Port does not exist.")
                break

            # Permission denied
            if e.errno == 13:
                # This is the error we're waiting for
                tries += 1
                # Be nice
                sleep(0.01)
                # Repeat until we can open it
                continue

            # Other exceptions we don't know about
            raise e

    if tries != 0:
        duration = monotonic() - start
        status = f"It took {tries} tries, in {duration:.2f}s, for {port} to become available."
        print(status)

    print("Good to go!")


env.AddPreAction("upload", before_upload)
