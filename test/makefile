#
# monotone test suite
# 
CC              = clang
RM              = rm
CFLAGS          = -Wall -pthread -O0 -g -fPIE
CFLAGS_INCLUDE  = -I. \
                  -I../monotone/runtime \
                  -I../monotone/runtime/runtime \
                  -I../monotone/runtime/lib \
                  -I../monotone/runtime/config \
                  -I../monotone/engine \
                  -I../monotone/engine/io \
                  -I../monotone/engine/storage \
                  -I../monotone/engine/engine \
                  -I../monotone/main \
                  -I../monotone/main/command \
                  -I../monotone/main/main \
                  -I../monotone/main/api \
                  -I. \
                  -Isuite
LFLAGS          = -rdynamic -pthread -Wl,-whole-archive \
                  ../monotone/runtime/libmonotone-runtime.a \
                  ../monotone/engine/libmonotone-engine.a \
                  ../monotone/main/libmonotone-main.a \
                  -Wl,-no-whole-archive -lm -lzstd
TARGET          = test
OBJECTS_RUNTIME =
OBJECTS         = suite/suite.o \
                  suite/suite_cmd.o \
                  test.o
$(TARGET): clean $(OBJECTS_RUNTIME) $(OBJECTS)
	@$(CC) $(OBJECTS_RUNTIME) $(OBJECTS) $(CFLAGS_INCLUDE) $(CFLAGS) $(LFLAGS) -o $(TARGET)
	@echo $(CC) $(CFLAGS) -o $(TARGET)
.c.o:
	@$(CC) $(CFLAGS_INCLUDE) $(CFLAGS) -c $< -o $@
	@echo $(CC) -c $<
clean:
	@$(RM) -f $(OBJECTS_RUNTIME) $(OBJECTS) $(TARGET)
