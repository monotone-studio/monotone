#
# noire makefile
#
CC             = clang
AR             = ar
RM             = rm
CFLAGS_OPTS    = -Wall -Wextra -g -fPIC -O2
CFLAGS_INCLUDE = -I. \
                 -Iruntime \
                 -Ilib \
                 -Iio \
                 -Iengine \
                 -Istorage
CFLAGS         = $(CFLAGS_INCLUDE) $(CFLAGS_OPTS)
LIB            = libnoire.a
OBJECTS        = \
                 runtime/runtime.o \
                 runtime/vfs.o \
                 runtime/logger.o \
                 lib/rbtree.o \
                 lib/crc.o \
                 lib/compression.o \
                 io/part.o \
                 io/part_writer.o \
                 io/merger.o \
                 engine/engine.o \
                 engine/engine_recover.o \
                 engine/compaction.o \
                 engine/compaction_mgr.o \
                 storage/storage.o
$(LIB): clean $(OBJECTS)
	@$(AR) cr $(LIB) $(OBJECTS)
	@echo $(AR) $(LIB)
.c.o:
	@$(CC) $(CFLAGS) -c $< -o $@
	@echo $(CC) -c $<
clean:
	@$(RM) -f $(OBJECTS) $(LIB)
