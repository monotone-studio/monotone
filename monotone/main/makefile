#
# monotone main makefile
#
CC             = clang
AR             = ar
RM             = rm
CFLAGS_OPTS    = -Wall -Wextra -g -fPIC -O0
CFLAGS_INCLUDE = -I../runtime \
                 -I../runtime/runtime \
                 -I../runtime/lib \
                 -I../runtime/config \
                 -I../engine \
                 -I../engine/io \
                 -I../engine/storage \
                 -I../engine/engine \
                 -I. \
                 -Icommand \
                 -Imain \
                 -Iapi
CFLAGS         = $(CFLAGS_INCLUDE) $(CFLAGS_OPTS)
LIB            = libmonotone-main.a
OBJECTS        = \
                 command/parse_config.o \
                 command/parse_storage.o \
                 command/parse_conveyor.o \
                 command/parse_partition.o \
                 command/parse.o \
#                main/main.o \
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
