cc 	:= clang
ops := -g -D_FILE_OFFSET_BITS=64 -MMD

src := $(shell find . -wholename "./src/*.c")
obj := $(patsubst ./src/%.c, ./build/%.o, $(src))
dep := $(obj:.o=.d)



all: init $(obj)
	$(cc) -o build/rpi-gfs $(obj)

$(obj): ./build/%.o: ./src/%.c ./build/%.d
	$(cc) $(ops) -c $< -o $@

-include $(dep)

$(dep): %.d: ;

init:
	mkdir -p ./build

clean:
	rm -rf build
	rm -f `find . -wholename "./src/*.d"`
