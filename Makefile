
# Choose compiler and flags.
mac_cc		:= clang
mac_link_ops := -g -D_FILE_OFFSET_BITS=64
mac_c_ops 	:= $(mac_link_ops) -MMD

pi_cc 		:= arm-none-eabi-gcc
pi_c_ops 	:= -g -D__RPI__ -Og -Wall -nostdlib -nostartfiles \
					-ffreestanding -mcpu=arm1176jzf-s -mtune=arm1176jzf-s \
					-std=gnu99 -ggdb -Wno-pointer-sign -Werror \
					-Wno-unused-function -Wno-unused-variable \
					-ffunction-sections -fdata-sections -MMD \
					-mno-unaligned-access -mtp=soft -Wl,--no-warn-rwx-segments

pi_as 		:= arm-none-eabi-gcc
pi_as_ops := -g -D__RPI__ -0g -Wall -mcpu=cortex-a53

pi_link_ops := -ffreestanding -nostdlib -lgcc

pi_oc			:= arm-none-eabi-objcopy

# Get source files, associated object files and dependency files.
mac_src		:= $(shell find . -wholename "./mac-src/*.c")
mac_obj		:= $(patsubst ./mac-src/%.c, ./build/mac/%.o, $(mac_src))
mac_dep		:= $(mac_obj:.o=.d)

pi_c 			:= $(shell find . -wholename "./pi-src/*.c")
pi_c_obj	:= $(patsubst ./pi-src/%.c, ./build/pi/%.o, $(pi_c))
pi_c_dep	:= $(pi_c_obj:.o=.d)

pi_S 			:= $(shell find . -wholename "./pi-src/*.S")
pi_S_obj	:= $(patsubst ./pi-src/%.S, ./build/pi/%.o, $(pi_S))
pi_S_dep	:= $(pi_S_obj:.o=.d)

pi_support_obj := $(shell find . -wholename "./ext-bin/*.o")

# Build mac-side and pi-side binaries, after making build/...
# First rule in the Makefile, automatically run.
all: init pibin macbin
	./build/mac/deploy ./build/pi/pi-side.bin

######################### MAC SIDE #########################

macbin: $(mac_obj)
	$(mac_cc) $(mac_link_ops) -o build/mac/deploy $(mac_obj)

$(mac_obj): ./build/mac/%.o: ./mac-src/%.c ./build/mac/%.d
	$(mac_cc) $(mac_c_ops) -c $< -o $@

$(mac_dep): %.d: ;
-include $(mac_dep)

########################## PI SIDE #########################

pibin: $(pi_c_obj) $(pi_S_obj)
	$(pi_cc) $(pi_link_ops) -Wl,-e,_start \
		-T pi-src/linker.ld -o build/pi/pi-side.elf \
		$(pi_c_obj) $(pi_S_obj) $(pi_support_obj)

	$(pi_oc) build/pi/pi-side.elf -O binary build/pi/pi-side.bin

$(pi_c_obj): ./build/pi/%.o: ./pi-src/%.c ./build/pi/%.d
	$(pi_cc) $(pi_c_ops) -c $< -o $@

-include $(pi_c_dep)
$(pi_c_dep): %.d: ;

$(pi_S_obj): ./build/pi/%.o: ./pi-src/%.S ./build/pi/%.d
	$(pi_as) $(pi_S_ops) -c $< -o $@

-include $(pi_S_dep)
$(pi_S_dep): %.d: ;

############################################################

# Dir init + clean
init:
	mkdir -p ./build/mac && mkdir -p ./build/pi

clean:
	rm -rf build
	rm -f `find . -wholename "./src/*.d"`
