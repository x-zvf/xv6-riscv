# How to debug userspace

1. copy .gdbinit.tmpl-riscv to .gdbinit
2. change port (in my case 26000)
3. run `make qemu-gdb` (best with CPUS=1)
4. run `riscv64-unknown-elf-gdb`
5. In GDB: file user/my_user_prog.o
6. Happy debugging: `break user/my_user_prog.o:main`
