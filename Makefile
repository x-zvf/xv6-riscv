K=kernel
U=user

OBJS = \
  $K/entry.o \
  $K/start.o \
  $K/console.o \
  $K/printf.o \
  $K/uart.o \
  $K/kalloc.o \
  $K/spinlock.o \
  $K/string.o \
  $K/main.o \
  $K/vm.o \
  $K/proc.o \
  $K/swtch.o \
  $K/trampoline.o \
  $K/trap.o \
  $K/syscall.o \
  $K/sysproc.o \
  $K/bio.o \
  $K/fs.o \
  $K/log.o \
  $K/sleeplock.o \
  $K/file.o \
  $K/pipe.o \
  $K/exec.o \
  $K/sysfile.o \
  $K/kernelvec.o \
  $K/plic.o \
  $K/virtio_disk.o  \
  $K/cxxtest.o \
  $K/terminate.o \

# riscv64-unknown-elf- or riscv64-linux-gnu-
# perhaps in /opt/riscv/bin
#TOOLPREFIX = 

# Try to infer the correct TOOLPREFIX if not set
ifndef TOOLPREFIX
TOOLPREFIX := $(shell if riscv64-unknown-elf-objdump -i 2>&1 | grep 'elf64-big' >/dev/null 2>&1; \
	then echo 'riscv64-unknown-elf-'; \
	elif riscv64-linux-gnu-objdump -i 2>&1 | grep 'elf64-big' >/dev/null 2>&1; \
	then echo 'riscv64-linux-gnu-'; \
	elif riscv64-unknown-linux-gnu-objdump -i 2>&1 | grep 'elf64-big' >/dev/null 2>&1; \
	then echo 'riscv64-unknown-linux-gnu-'; \
	else echo "***" 1>&2; \
	echo "*** Error: Couldn't find a riscv64 version of GCC/binutils." 1>&2; \
	echo "*** To turn off this error, run 'gmake TOOLPREFIX= ...'." 1>&2; \
	echo "***" 1>&2; exit 1; fi)
endif

QEMU = qemu-system-riscv64

CC = $(TOOLPREFIX)gcc
CXX = $(TOOLPREFIX)g++
AS = $(TOOLPREFIX)gas
LD = $(TOOLPREFIX)ld
OBJCOPY = $(TOOLPREFIX)objcopy
OBJDUMP = $(TOOLPREFIX)objdump

ASFLAGS += -I.

CFLAGS = -Wall -Werror -O -fno-omit-frame-pointer -ggdb -gdwarf-2
CFLAGS += -MD
CFLAGS += -mcmodel=medany
CFLAGS += -ffreestanding -fno-common -nostdlib -mno-relax
CFLAGS += -I.
CFLAGS += $(shell $(CC) -fno-stack-protector -E -x c /dev/null >/dev/null 2>&1 && echo -fno-stack-protector)

CXXFLAGS = -Wall -Werror -O -fno-omit-frame-pointer -ggdb -gdwarf-2
CXXFLAGS += -MD
CXXFLAGS += -mcmodel=medany
CXXFLAGS += -ffreestanding -fno-common -nostdlib -mno-relax
CXXFLAGS += -I.
CXXFLAGS += $(shell $(CXX) -fno-stack-protector -E -x c /dev/null >/dev/null 2>&1 && echo -fno-stack-protector)

# Disable PIE when possible (for Ubuntu 16.10 toolchain)
ifneq ($(shell $(CC) -dumpspecs 2>/dev/null | grep -e '[^f]no-pie'),)
CFLAGS += -fno-pie -no-pie
endif
ifneq ($(shell $(CC) -dumpspecs 2>/dev/null | grep -e '[^f]nopie'),)
CFLAGS += -fno-pie -nopie
endif

ifneq ($(shell $(CXX) -dumpspecs 2>/dev/null | grep -e '[^f]no-pie'),)
CXXFLAGS += -fno-pie -no-pie
endif
ifneq ($(shell $(CXX) -dumpspecs 2>/dev/null | grep -e '[^f]nopie'),)
CXXFLAGS += -fno-pie -nopie
endif

LDFLAGS = -z max-page-size=4096

$K/kernel: $(OBJS) $K/kernel.ld $U/initcode
	$(LD) $(LDFLAGS) -T $K/kernel.ld -o $K/kernel $(OBJS) 
	$(OBJDUMP) -S $K/kernel > $K/kernel.asm
	$(OBJDUMP) -t $K/kernel | sed '1,/SYMBOL TABLE/d; s/ .* / /; /^$$/d' > $K/kernel.sym

$U/initcode: $U/initcode.S
	$(CC) $(CFLAGS) -march=rv64g -nostdinc -I. -Ikernel -c $U/initcode.S -o $U/initcode.o
	$(LD) $(LDFLAGS) -N -e start -Ttext 0 -o $U/initcode.out $U/initcode.o
	$(OBJCOPY) -S -O binary $U/initcode.out $U/initcode
	$(OBJDUMP) -S $U/initcode.o > $U/initcode.asm

tags: $(OBJS) _init
	etags *.S *.c

ULIB = $U/ulib.o $U/usys.o $U/printf.o $U/umalloc.o $U/bmalloc.o $U/mmap-mock.o

_%: %.o $(ULIB)
	$(LD) $(LDFLAGS) -T $U/user.ld -o $@ $^
	$(OBJDUMP) -S $@ > $*.asm
	$(OBJDUMP) -t $@ | sed '1,/SYMBOL TABLE/d; s/ .* / /; /^$$/d' > $*.sym

$U/usys.S : $U/usys.pl
	perl $U/usys.pl > $U/usys.S

$U/usys.o : $U/usys.S
	$(CC) $(CFLAGS) -c -o $U/usys.o $U/usys.S

$U/_forktest: $U/forktest.o $(ULIB)
	# forktest has less library code linked in - needs to be small
	# in order to be able to max out the proc table.
	$(LD) $(LDFLAGS) -N -e main -Ttext 0 -o $U/_forktest $U/forktest.o $U/ulib.o $U/usys.o
	$(OBJDUMP) -S $U/_forktest > $U/forktest.asm

mkfs/mkfs: mkfs/mkfs.c $K/fs.h $K/param.h
	gcc -Werror -Wall -I. -o mkfs/mkfs mkfs/mkfs.c

# Prevent deletion of intermediate files, e.g. cat.o, after first build, so
# that disk image changes after first build are persistent until clean.  More
# details:
# http://www.gnu.org/software/make/manual/html_node/Chained-Rules.html
.PRECIOUS: %.o

UPROGS=\
	$U/_cat\
	$U/_echo\
	$U/_forktest\
	$U/_grep\
	$U/_init\
	$U/_kill\
	$U/_ln\
	$U/_ls\
	$U/_mkdir\
	$U/_rm\
	$U/_sh\
	$U/_stressfs\
	$U/_usertests\
	$U/_grind\
	$U/_wc\
	$U/_zombie\
	$U/_cxxtest\
	$U/_malloc_test\
	$U/_malloc_test_cxx\
	$U/_terminate\

fs.img: mkfs/mkfs README $(UPROGS)
	mkfs/mkfs fs.img README $(UPROGS)

-include kernel/*.d user/*.d ct-test/*.d rt-test/*.d

clean: 
	rm -f *.tex *.dvi *.idx *.aux *.log *.ind *.ilg \
	*/*.o */*.d */*.asm */*.sym \
	$U/initcode $U/initcode.out $K/kernel fs.img rt-test.img rt-bench.img \
	mkfs/mkfs .gdbinit \
	$U/usys.S \
	$(RUNTIMETESTFOLDER)/_* $(BENCHMARKFOLDER)/_* \
	$(UPROGS)

# try to generate a unique GDB port
GDBPORT = $(shell expr `id -u` % 5000 + 25000)
# QEMU's gdb stub command line changed in 0.11
QEMUGDB = $(shell if $(QEMU) -help | grep -q '^-gdb'; \
	then echo "-gdb tcp::$(GDBPORT)"; \
	else echo "-s -p $(GDBPORT)"; fi)
ifndef CPUS
CPUS := 3
endif

QEMUOPTS = -machine virt -bios none -kernel $K/kernel -m 128M -smp $(CPUS) -nographic
QEMUOPTS += -global virtio-mmio.force-legacy=false
QEMUOPTS += -device virtio-blk-device,drive=x0,bus=virtio-mmio-bus.0
QEMUOPTS.drive = -drive file=fs.img,if=none,format=raw,id=x0

qemu: $K/kernel fs.img
	$(QEMU) $(QEMUOPTS) $(QEMUOPTS.drive)

.gdbinit: .gdbinit.tmpl-riscv
	sed "s/:1234/:$(GDBPORT)/" < $^ > $@

qemu-gdb: $K/kernel .gdbinit fs.img
	@echo "*** Now run 'gdb' in another window." 1>&2
	$(QEMU) $(QEMUOPTS) $(QEMUOPTS.drive) -S $(QEMUGDB)


LANGUAGE_EXTENSION = c cpp cxx c++ cc
COMPILETESTFOLDER = ct-test
COMPILETEST = $(foreach ext,$(LANGUAGE_EXTENSION),$(wildcard $(COMPILETESTFOLDER)/*.$(ext)))
COMPILEOUT = $(foreach ext,$(LANGUAGE_EXTENSION),$(patsubst %.$(ext),%.o, $(filter %.$(ext),$(COMPILETEST))))
#$(info $$(COMPILETEST) is $(COMPILETEST))
#$(info $$(COMPILEOUT) is $(COMPILEOUT))

ct-test: $(COMPILEOUT)

RUNTIMETESTFOLDER = rt-test
RUNTIMETEST = $(foreach ext,$(LANGUAGE_EXTENSION),$(wildcard $(RUNTIMETESTFOLDER)/*.$(ext)))
RUNTIMEOUT = $(foreach ext,$(LANGUAGE_EXTENSION),$(patsubst %.$(ext),%.o, $(filter %.$(ext),$(RUNTIMETEST))))
RUNTIMEBIN = $(foreach object,$(filter %.o,$(RUNTIMEOUT)),$(dir $(object))_$(basename $(notdir $(object))))

rt-test.img: mkfs/mkfs $(RUNTIMEBIN)
	mkfs/mkfs rt-test.img $(RUNTIMEBIN)

rt-test: $K/kernel rt-test.img
	$(QEMU) $(QEMUOPTS) $(subst fs.img,rt-test.img,$(QEMUOPTS.drive))

BENCHMARK_QEMU_FOLDER = ~/Develop/KIT/osdev/qemu/build
BENCHMARK_QEMU_ADDITIONAL_OPTIONS = -plugin ~/Develop/KIT/osdev/qemu/build/tests/plugin/libinsn.so,inline=on -d plugin
BENCHMARKFOLDER = rt-bench
BENCHMARK = $(foreach ext,$(LANGUAGE_EXTENSION),$(wildcard $(BENCHMARKFOLDER)/*.$(ext)))
BENCHMARKOUT = $(foreach ext,$(LANGUAGE_EXTENSION),$(patsubst %.$(ext),%.o, $(filter %.$(ext),$(BENCHMARK))))
BENCHMARKBIN = $(foreach object,$(filter %.o,$(BENCHMARKOUT)),$(dir $(object))_$(basename $(notdir $(object))))
BENCHMARKEXEC = $(foreach benchmark,$(filter %bench,$(BENCHMARKBIN)),\
	mkfs/mkfs rt-bench-individual.img rt-bench/_init $(benchmark); \
	PATH=$(BENCHMARK_QEMU_FOLDER):$$PATH \
		$(QEMU) $(subst -smp 3,-smp 1,$(QEMUOPTS)) $(subst fs.img,rt-bench-individual.img,$(QEMUOPTS.drive)) $(BENCHMARK_QEMU_ADDITIONAL_OPTIONS) \
		$(NEWLINE))

rt-bench.img: mkfs/mkfs $(BENCHMARKBIN)
	mkfs/mkfs rt-bench.img $(BENCHMARKBIN)

rt-bench: $K/kernel rt-bench.img
	$(QEMU) $(QEMUOPTS) $(subst fs.img,rt-bench.img,$(QEMUOPTS.drive))

define NEWLINE


endef

rt-bench-individual: $K/kernel mkfs/mkfs $(BENCHMARKBIN)
	$(BENCHMARKEXEC)

test : ct-test rt-test

bench: rt-bench

eval: test bench

all: fs.img eval
