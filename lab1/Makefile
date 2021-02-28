obj-m += lab1.o
makecmd = make -C /lib/modules/`uname -r`/build M=$(PWD)
modules:
	$(makecmd) modules
clean:
	$(makecmd) clean
