
# Choose compiler and flags.
mcc	:= clang
mops := -g -D_FILE_OFFSET_BITS=64 -MMD

pcc := arm-none-eabi-gcc
pops := -g -D__RPI__ -Og -Wall -nostdlib -nostartfiles -ffreestanding -mcpu=arm1176jzf-s -mtune=arm1176jzf-s -std=gnu99 -ggdb -Wno-pointer-sign -Werror -Wno-unused-function -Wno-unused-variable -ffunction-sections -fdata-sections -MMD -mno-unaligned-access -mtp=soft 
poc := arm-none-eabi-objcopy

# Get source files, associated object files and dependency files.
macsrc := $(shell find . -wholename "./mac-src/*.c")
macobj := $(patsubst ./mac-src/%.c, ./build/mac/%.o, $(macsrc))
macdep := $(macobj:.o=.d)

pisrc := $(shell find . -wholename "./pi-src/*.c")
piobj := $(patsubst ./pi-src/%.c, ./build/pi/%.o, $(pisrc))
pidep := $(piobj:.o=.d)

pisupportingobjs := $(shell find . -wholename "./240-pi-binaries/*.o")

######################### MAC SIDE #########################

# Build mac-side and pi-side binaries, after making build/...
# First rule in the Makefile, automatically run.
all: init macbin pibin

# Build mac-side binaries.
macbin: $(macobj)
	$(mcc) $(mops) -o build/mac/deploy $(macobj)

# Build individual object files for the mac.
$(macobj): ./build/mac/%.o: ./mac-src/%.c ./build/mac/%.d
	$(mcc) $(mops) -c $< -o $@

# Needed for -MMD...haven't looked into why
-include $(macdep)

$(macdep): %.d: ;

########################## PI SIDE #########################

# Build pi-side binaries.
pibin: $(piobj)
	$(pcc) $(pops) -Wl,-e,start -T pi-src/linker.ld -o build/pi/pi-side.elf $(pisrc) $(pisupportingobjs)
	$(poc) build/pi/pi-side.elf -O binary build/pi/pi-side.bin

# Build individual ojects for the pi.
$(piobj): ./build/pi/%.o: ./pi-src/%.c ./build/pi/%.d
	$(pcc) $(pops) -c $< -o $@

-include $(pidep)

$(pidep): %.d: ;

############################################################

# Dir init + clean
init:
	mkdir -p ./build/mac && mkdir -p ./build/pi

clean:
	rm -rf build
	rm -f `find . -wholename "./src/*.d"`
