all: ex

ex:
	$(MAKE) -C examples/01---hello-world
	$(MAKE) -C examples/02---list-dirs
	$(MAKE) -C examples/03---error-handling
	$(MAKE) -C examples/04---advanced-strings
	$(MAKE) -C examples/08---udp

prod:
	$(MAKE) -C examples/08---udp debug_enable=0

clean:
	$(MAKE) -C examples/01---hello-world clean
	$(MAKE) -C examples/02---list-dirs clean
	$(MAKE) -C examples/03---error-handling clean
	$(MAKE) -C examples/04---advanced-strings clean
	$(MAKE) -C examples/08---udp clean
