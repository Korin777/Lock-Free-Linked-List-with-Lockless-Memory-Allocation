CFLAGS = -Wall -O2 -std=c11 #-g -fsanitize=address
CFLAGS += -I include
CFLAGS += -D_GNU_SOURCE
LDFLAGS += -lpthread #-g -fsanitize=address

OUT = out
EXEC = $(OUT)/linklist $(OUT)/linklist_MY_MALLOC

all: $(EXEC)

src/main.o : src/main.c
	$(CC) $(CFLAGS) -o $@ -MMD -MF $@.d -c $<
src/MY_MALLOC/main.o : src/main.c
	$(CC) $(CFLAGS) -DMY_MALLOC -o $@ -MMD -MF $@.d -c $<

deps=
LOCKFREE_OBJS =
LOCKFREE_OBJS += src/nblist/nblist.o
LOCKFREE_OBJS += src/nblist/mymemmalloc.o
LOCKFREE_OBJS += src/nblist/hp.o
LOCKFREE_OBJS += src/main.o
deps += $(LOCKFREE_OBJS:%.o=%.o.d)

$(OUT)/linklist: $(LOCKFREE_OBJS)
	@mkdir -p $(OUT)
	$(CC) -o $@ $^ $(LDFLAGS)
src/nblist/%.o: src/nblist/%.c
	$(CC) $(CFLAGS) -o $@ -MMD -MF $@.d -c $<

MY_MALLOC_OBJ =
MY_MALLOC_OBJ += src/MY_MALLOC/nblist.o
MY_MALLOC_OBJ += src/MY_MALLOC/mymemmalloc.o
MY_MALLOC_OBJ += src/MY_MALLOC/hp.o
MY_MALLOC_OBJ += src/MY_MALLOC/main.o
deps += $(MY_MALLOC_OBJ:%.o=%.o.d)

$(OUT)/linklist_MY_MALLOC: $(MY_MALLOC_OBJ)
	@mkdir -p $(OUT)
	$(CC) -o $@ $^ $(LDFLAGS) -DMY_MALLOC
src/MY_MALLOC/%.o: src/nblist/%.c
	@mkdir -p src/MY_MALLOC
	$(CC) $(CFLAGS) -DMY_MALLOC -o $@ -MMD -MF $@.d -c $<

deps += null.d

clean:
	$(RM) $(EXEC)
	$(RM) $(MY_MALLOC_OBJ) $(LOCKFREE_OBJS) $(deps)

.PHONY: all check clean distclean