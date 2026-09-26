all: test

test:
	$(MAKE) -C tests run

clean:
	$(MAKE) -C tests clean

.PHONY: all test clean
