#
# monotone engine makefile
#
CC             = clang
AR             = ar
RM             = rm
CFLAGS_OPTS    = -Wall -Wextra -g -fPIC -O0
CFLAGS_INCLUDE = -I../runtime \
                 -I../runtime/runtime \
                 -I../runtime/lib \
                 -I../runtime/config \
                 -I. \
                 -Iio \
                 -Istorage \
                 -Iengine
CFLAGS         = $(CFLAGS_INCLUDE) $(CFLAGS_OPTS)
LIB            = libmonotone-engine.a
OBJECTS        = \
                 io/memtable.o \
                 io/part.o \
                 io/writer.o \
                 io/merger.o \
                 storage/storage_mgr.o \
                 storage/conveyor.o \
                 engine/engine.o \
                 engine/engine_recover.o \
                 engine/write.o \
                 engine/refresh.o \
                 engine/op.o \
                 engine/worker.o \
                 engine/worker_mgr.o
$(LIB): clean $(OBJECTS)
	@$(AR) cr $(LIB) $(OBJECTS)
	@echo $(AR) $(LIB)
.c.o:
	@$(CC) $(CFLAGS) -c $< -o $@
	@echo $(CC) -c $<
clean:
	@$(RM) -f $(OBJECTS) $(LIB)
