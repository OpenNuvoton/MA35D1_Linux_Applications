Target = amp.bin

CPP=aarch64-linux-gcc

LIBS = -pthread -lz -lc -lgcc

all: $(Target)

$(Target):amp.c
	$(CPP) amp.c -o $(Target) $(LIBS) 
	@echo "generate $(Target) success!!!"

.PHONY:clean cleanall

clean: 
	@rm -f $(Target)
cleanall:clean
	@echo -e '\e[1;33m -rm -f $(INSTATLL_PATH)/$(Target) \e[0m'

distclean:cleanall 
	
install:
	@echo -e '\e[1;33m  install -m 0755 $(Target) $(INSTATLL_PATH)/$(Target) \e[0m'
