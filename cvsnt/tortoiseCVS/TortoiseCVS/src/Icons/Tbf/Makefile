STATES=Changed Conflict InCVS InCVSReadOnly NotInCVS Ignored
TARGETS=$(STATES:%=Tortoise%.ico)

all: $(TARGETS)

clean:
	rm *.ico

Tortoise%.ico: Tortoise%-16.png Tortoise%-32.png
	icotool --create $^ > $@
