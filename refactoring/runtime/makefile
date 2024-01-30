#
# monotone runtime makefile
#
CC             = clang
AR             = ar
RM             = rm
CFLAGS_OPTS    = -Wall -Wextra -g -fPIC -O0
CFLAGS_INCLUDE = -I. \
                 -Iruntime \
                 -Ilib
CFLAGS         = $(CFLAGS_INCLUDE) $(CFLAGS_OPTS)
LIB            = libmonotone-runtime.a
OBJECTS        = \
#                runtime/runtime.o \
                 runtime/vfs.o \
                 runtime/logger.o \
#                lib/json.o \
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
                 catalog/storage_mgr.o \
                 catalog/conveyor.o \
                 engine/engine.o \
                 engine/engine_recover.o \
                 engine/write.o \
                 engine/refresh.o \
                 engine/op.o \
                 engine/op_ddl.o \
                 engine/worker.o \
                 engine/worker_mgr.o \
                 main/main.o \
                 main/command.o \
                 api/monotone.o
$(LIB): clean $(OBJECTS)
	@$(AR) cr $(LIB) $(OBJECTS)
	@echo $(AR) $(LIB)
.c.o:
	@$(CC) $(CFLAGS) -c $< -o $@
	@echo $(CC) -c $<
clean:
	@$(RM) -f $(OBJECTS) $(LIB)
