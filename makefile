all: build/CMakeCache.txt
	@(cd build && make --no-print-directory)
build/CMakeCache.txt:
	@(cd build && cmake .)
	@(echo)
clean:
	@(cd build && make clean --no-print-directory)
