#
# monotone makefile
#
CC             = clang
AR             = ar
RM             = rm
CFLAGS_OPTS    = -Wall -Wextra -g -fPIC -O0
CFLAGS_INCLUDE = -I. \
                 -Iruntime \
                 -Ilib \
                 -Iio \
                 -Istorage \
                 -Imain \
                 -Iapi
CFLAGS         = $(CFLAGS_INCLUDE) $(CFLAGS_OPTS)
LIB            = libmonotone.a
OBJECTS        = \
                 runtime/runtime.o \
                 runtime/vfs.o \
                 runtime/logger.o \
                 lib/json.o \
                 lib/rbtree.o \
                 lib/crc.o \
                 lib/uuid.o \
                 lib/compression.o \
                 lib/lex.o \
                 lib/config.o \
                 io/memtable.o \
                 io/part.o \
                 io/writer.o \
                 io/merger.o \
                 storage/storage_mgr.o \
                 storage/conveyor.o \
                 main/main.o \
                 main/command.o \
                 api/monotone.o
#                engine/engine.o \
                 engine/engine_recover.o \
                 engine/write.o \
                 engine/compaction.o \
                 engine/compaction_mgr.o \
                 engine/stats.o \
                 instance/instance.o \
                 instance/config.o \
                 api/monotone.o
$(LIB): clean $(OBJECTS)
	@$(AR) cr $(LIB) $(OBJECTS)
	@echo $(AR) $(LIB)
.c.o:
	@$(CC) $(CFLAGS) -c $< -o $@
	@echo $(CC) -c $<
clean:
	@$(RM) -f $(OBJECTS) $(LIB)
