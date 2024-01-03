#
# monoton test
# 
CC             = clang
RM             = rm
CFLAGS         = -Wall -pthread -O2 -g -fPIE
CFLAGS_INCLUDE = -I. \
                 -I../monotone \
                 -I../monotone/runtime \
                 -I../monotone/lib \
                 -I../monotone/io \
                 -I../monotone/engine \
                 -I../monotone/instance
LFLAGS         = -rdynamic -pthread -Wl,-whole-archive \
                  ../monotone/libmonotone.a \
                  -Wl,-no-whole-archive -lm -lzstd
SRC            = test.c
TARGET         = test
$(TARGET): clean $(OBJECTS)
	@$(CC) $(SRC) $(CFLAGS_INCLUDE) $(CFLAGS) $(LFLAGS) -o $(TARGET)
	@echo $(CC) $(SRC) $(CFLAGS) -o $(TARGET)
clean:
	@$(RM) -f $(TARGET)
