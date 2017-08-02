#ifeq ($(KERNELRELEASE),)
#export MMZ_SRC_BASE=$(PWD)
#endif

ifeq ($(PARAM_FILE), )
		PARAM_FILE:=../../Makefile.param
		include $(PARAM_FILE)
endif
obj-m := nvp6114_ex.o
nvp6114_ex-objs := nvp6114_drv.o coax_protocol.o video.o motion.o audio.o


EXTRA_CFLAGS += -I$(PWD)/../gpio_i2c_8b

default:
	@make -C $(LINUX_ROOT) M=$(PWD) modules 
	cp *.ko /nfs/zw/
	cd /nfs/zw; ./modules_3521.sh
	#cd /nfs/zw; ./modules_3520A.sh
clean: 
	@make -C $(LINUX_ROOT) M=$(PWD) clean 


