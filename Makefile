INSTALL_DIR ?= /boot/home/config/non-packaged/bin
CXX ?= c++

.PHONY: all install test clean

all:
	$(MAKE) -C src

install: all
	$(MAKE) -C src install INSTALL_DIR="$(INSTALL_DIR)"

test:
	$(CXX) -std=c++17 -Wall -Wextra -Isrc \
		tests/SyntaxHighlighterTest.cpp src/SyntaxHighlighter.cpp \
		-o tests/syntax_highlighter_test
	./tests/syntax_highlighter_test

clean:
	$(MAKE) -C src clean
	rm -f tests/syntax_highlighter_test
