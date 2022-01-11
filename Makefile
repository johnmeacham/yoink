CC=gcc
LD=gcc
CFLAGS= -Wall -O -Isrc -Iresizable_buf
LDLIBS=-lm

all: src/yoink

%.s : %.c
	$(CC) $(CFLAGS) -S $< -o $@
nms: t
	nm --size-sort -S $<
%.c : %.re2c
	re2c -W $< -o $@

%: obj/t/%.o
	$(LD) $(CFLAGS) $(LDFLAGS) $^ -o $@ $(LDLIBS)

src/yoink: obj/src/ptrhashtable2.o obj/resizable_buf/resizable_buf.o obj/src/inthash.o


obj/%.o : %.c
	@mkdir -p $(dir $@)
	$(CC) $(CFLAGS) -MMD -MP -c $< -o $@
obj/t/%.o : %.c
	@mkdir -p $(dir $@)
	$(CC) $(CFLAGS) -MMD -MP -c $< -o $@ -DTESTING
%.s: %.o
	objdump -S $< > $@


include $(shell find obj/ -name \*.d)
clean:
	rm $(shell find obj/ -name \*.o)
