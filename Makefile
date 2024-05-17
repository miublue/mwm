OUT = mwm
INC = -I ../mutils/
LIB = -lX11

all:
	tcc -o $(OUT) src/*.c $(INC) $(LIB)

install: all
	mv $(OUT) /usr/bin

uninstall:
	rm /usr/bin/$(OUT)
