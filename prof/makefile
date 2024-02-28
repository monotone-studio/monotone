#
# monoton test
# 
CC             = clang
RM             = rm
CFLAGS         = -Wall -pthread -O2 -g -fPIE
CFLAGS_INCLUDE = -I. \
                 -I../monotone/runtime \
                 -I../monotone/runtime/runtime \
                 -I../monotone/runtime/lib \
                 -I../monotone/runtime/config \
                 -I../monotone/engine \
                 -I../monotone/engine/cloud \
                 -I../monotone/engine/io \
                 -I../monotone/engine/storage \
                 -I../monotone/engine/wal \
                 -I../monotone/engine/engine \
                 -I../monotone/main \
                 -I../monotone/main/command \
                 -I../monotone/main/main \
                 -I../monotone/main/api
LFLAGS         = -rdynamic -pthread -Wl,-whole-archive \
                  ../monotone/runtime/libmonotone-runtime.a \
                  ../monotone/engine/libmonotone-engine.a \
                  ../monotone/main/libmonotone-main.a \
                  -Wl,-no-whole-archive -lm -lzstd -lcurl -lcrypto
SRC            = test.c
TARGET         = test
$(TARGET): clean $(OBJECTS)
	@$(CC) $(SRC) $(CFLAGS_INCLUDE) $(CFLAGS) $(LFLAGS) -o $(TARGET)
	@echo $(CC) $(SRC) $(CFLAGS) -o $(TARGET)
clean:
	@$(RM) -f $(TARGET)
