#!/bin/env python3

import fileinput
import subprocess
import re

in_backtrace = False
backtracing_binary = ""

def uncolor(line):
    return re.sub(r"\x1b\[[0-9;]*m", "", line)

class PostProcessor:
    def __init__(self):
        self.active = False
    
    def update(self, colorless_line):
        pass
    
    def is_active(self):
        return self.active 

    def process(self, line):
        pass

class Stacktracer(PostProcessor):
    def __init__(self):
        super().__init__()
        self.backtracing_binary = ""

    def update(self, line):
        line = uncolor(line)
        stacktrace = re.match(r"^={20} stacktrace \((pid \d+ (\w*)|(kernel))\) ={20}$", line)
        if stacktrace:
            self.active = True
            program = stacktrace.group(2)
            if program:
                self.backtracing_binary = "user/_" + program
            else:
                self.backtracing_binary = "kernel/kernel"

        if re.match(r"^={20} end stacktrace ={20}$", line):
            self.active = False
            self.backtracing_binary = ""
        
    def process(self, line):
        line = line.rstrip()
        addresses = re.search(r"(0x[0-9a-f]+)", uncolor(line))
        if addresses:
            loc = subprocess.check_output(["riscv64-unknown-elf-addr2line", "-e",
                                           self.backtracing_binary, addresses.group(0)]).decode("utf-8").strip()
            line += "\x1b[0m  @  \x1b[33m" + loc + "\x1b[0m"
        return line + "\n"

class Default(PostProcessor):
    def __init__(self):
        super().__init__()
        self.active = True

    def process(self, line):
        return line       

post_processors = [Stacktracer(), Default()]

def handle_line(line):
    for processor in post_processors:
        processor.update(line)
        if processor.is_active():
            line = processor.process(line)
            print(line, end="")
            break
if __name__ == "__main__":
    for line in fileinput.input(encoding="utf-8"):
        handle_line(line)