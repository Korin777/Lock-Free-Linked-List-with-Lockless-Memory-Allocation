CFLAGS = -Wall -O2 -std=c11
CFLAGS += -I include
CFLAGS += -D_GNU_SOURCE
LDFLAGS += -lpthread

OUT = out
EXEC = $(OUT)/linklist

all: $(EXEC)

deps=
LOCKFREE_OBJS =
LOCKFREE_OBJS += src/nblist/nblist.o
LOCKFREE_OBJS += src/main.o
deps += $(LOCKFREE_OBJS:%.o=%.o.d)

$(OUT)/linklist: $(LOCKFREE_OBJS)
	@mkdir -p $(OUT)
	$(CC) -o $@ $^ $(LDFLAGS)
src/nblist/%.o: src/nblist/%.c
	$(CC) $(CFLAGS) -o $@ -MMD -MF $@.d -c $<

clean:
	$(RM) -f $(EXEC)
	$(RM) -f $(LOCK_OBJS) $(LOCKFREE_OBJS) $(deps)

.PHONY: all check clean distclean