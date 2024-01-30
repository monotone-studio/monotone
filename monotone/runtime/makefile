#
# monotone runtime makefile
#
CC             = clang
AR             = ar
RM             = rm
CFLAGS_OPTS    = -Wall -Wextra -g -fPIC -O0
CFLAGS_INCLUDE = -I. \
                 -Iruntime \
                 -Ilib \
                 -Iconfig
CFLAGS         = $(CFLAGS_INCLUDE) $(CFLAGS_OPTS)
LIB            = libmonotone-runtime.a
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
                 config/config.o
$(LIB): clean $(OBJECTS)
	@$(AR) cr $(LIB) $(OBJECTS)
	@echo $(AR) $(LIB)
.c.o:
	@$(CC) $(CFLAGS) -c $< -o $@
	@echo $(CC) -c $<
clean:
	@$(RM) -f $(OBJECTS) $(LIB)
