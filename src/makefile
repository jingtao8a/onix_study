BOCHS:= /home/yuxintao/onix_study/bochs
BUILD:= /home/yuxintao/onix_study/build
SRC:= /home/yuxintao/onix_study/src
TEST:=/home/yuxintao/onix_study/tests
MULTIBOOT2:=0x10000
ENTRYPOINT:=$(shell python -c "print(f'0x{$(MULTIBOOT2) + 64:x}')")


CFLAGS:=-m32
CFLAGS+=-fno-builtin#不需要gcc的内置函数
CFLAGS+=-nostdinc#不需要标准头文件
CFLAGS+=-fno-pic#不需要位置无关的代码
CFLAGS+=-fno-pie#不需要位置无关的可执行程序
CFLAGS+=-nostdlib#不需要标准库
CFLAGS+=-fno-stack-protector#不需要栈保护
CFLAGS+=-DONIX #定义ONIX
CFLAGS:=$(strip ${CFLAGS})


DEBUG:=-g

LDFLAGS:= -m elf_i386 \
		-static \
		-Ttext $(ENTRYPOINT)\
		--section-start=.multiboot2=$(MULTIBOOT2)

LDFLAGS:=$(strip ${LDFLAGS})

$(BUILD)/boot/%.bin : $(SRC)/boot/%.asm
	$(shell mkdir -p $(dir $@))
	nasm -f bin $< -o $@


$(BUILD)/%.o : $(SRC)/%.asm
	$(shell mkdir -p $(dir $@))
	nasm -f elf32 $(DEBUG) $< -o $@

$(BUILD)/%.o : $(SRC)/%.c
	$(shell mkdir -p $(dir $@))
	gcc $(CFLAGS) $(DEBUG) -c $< -o $@ 

$(BUILD)/%.out : $(BUILD)/%.o\
	$(BUILD)/lib/libc.o\

	ld -m elf_i386 -static $^ -o $@ -Ttext 0x1001000

$(BUILD)/lib/libc.o : $(BUILD)/lib/crt.o \
	$(BUILD)/lib/crt1.o \
	$(BUILD)/lib/string.o \
	$(BUILD)/lib/vsprintf.o \
	$(BUILD)/lib/stdlib.o \
	$(BUILD)/lib/syscall.o \
	$(BUILD)/lib/printf.o \
	$(BUILD)/lib/assert.o \
	$(BUILD)/lib/time.o\

	ld -m elf_i386 -r $^ -o $@

BUILTIN_APPS := \
	$(BUILD)/builtin/init.out\
	$(BUILD)/builtin/env.out\
	$(BUILD)/builtin/echo.out\
	$(BUILD)/builtin/cat.out\
	$(BUILD)/builtin/ls.out\
	$(BUILD)/builtin/dup.out\
	$(BUILD)/builtin/err.out\
	$(BUILD)/builtin/osh.out\


$(BUILD)/kernel.bin : $(BUILD)/kernel/start.o \
					$(BUILD)/kernel/main.o \
					$(BUILD)/kernel/io.o \
					$(BUILD)/lib/string.o \
					$(BUILD)/kernel/console.o \
					$(BUILD)/lib/vsprintf.o\
					$(BUILD)/kernel/printk.o \
					$(BUILD)/kernel/assert.o \
					$(BUILD)/kernel/debug.o \
					$(BUILD)/kernel/global.o \
					$(BUILD)/kernel/tasks.o \
					$(BUILD)/kernel/schedule.o \
					$(BUILD)/kernel/handler.o \
					$(BUILD)/kernel/interrupt.o \
					$(BUILD)/lib/stdlib.o \
					$(BUILD)/kernel/clock.o \
					$(BUILD)/kernel/time.o \
					$(BUILD)/kernel/rtc.o \
					$(BUILD)/kernel/memory.o \
					$(BUILD)/lib/bitmap.o \
					$(BUILD)/kernel/gate.o \
					$(BUILD)/lib/syscall.o \
					$(BUILD)/lib/list.o \
					$(BUILD)/kernel/thread.o \
					$(BUILD)/kernel/mutex.o \
					$(BUILD)/kernel/keyboard.o \
					$(BUILD)/lib/fifo.o \
					$(BUILD)/lib/printf.o \
					$(BUILD)/kernel/arena.o \
					$(BUILD)/kernel/ide.o \
					$(BUILD)/kernel/device.o \
					$(BUILD)/kernel/buffer.o \
					$(BUILD)/fs/super.o \
					$(BUILD)/fs/bmap.o \
					$(BUILD)/fs/inode.o \
					$(BUILD)/kernel/system.o \
					$(BUILD)/fs/namei.o\
					$(BUILD)/fs/file.o\
					$(BUILD)/fs/stat.o \
					$(BUILD)/fs/dev.o \
					$(BUILD)/kernel/ramdisk.o\
					$(BUILD)/kernel/execve.o\
					$(BUILD)/kernel/serial.o\
					$(BUILD)/fs/pipe.o\

	$(shell mkdir -p $(dir $@))
	ld  ${LDFLAGS} $^ -o $@ 

# 从elf文件中提取二进制文件
$(BUILD)/system.bin : $(BUILD)/kernel.bin
	$(shell mkdir -p $(dir $@))
	objcopy -O binary $< $@

#system.map文件是kernel.bin中的符号映射文件
$(BUILD)/system.map: $(BUILD)/kernel.bin
	$(shell mkdir -p $(dir $@))
	nm $< | sort > $@

############################################################
#master.img 硬盘启动
############################################################
$(BUILD)/master.img : $(BUILD)/boot/boot.bin \
				$(BUILD)/boot/loader.bin \
				$(BUILD)/system.bin \
				$(BUILD)/system.map \
				$(SRC)/utils/master.sfdisk\
				$(BUILTIN_APPS)\
				
	$(shell mkdir -p $(dir $@))
	yes | bximage -mode=create -hd=16 -imgmode=flat -sectsize=512 -q $@
	dd if=$(BUILD)/boot/boot.bin of=$@ bs=512 count=1 conv=notrunc
	dd if=$(BUILD)/boot/loader.bin of=$@ bs=512 count=1 seek=2 conv=notrunc
#测试system.bin是否小于100k，如果大于100k，下面dd命令写入会出错 
	test -n "$$(find $(BUILD)/system.bin -size -100k)" 
#将system.bin写入master.img 
	dd if=$(BUILD)/system.bin of=$@ bs=512 count=200 seek=10 conv=notrunc
#对硬盘master.img进行分区
	sfdisk $@ < $(SRC)/utils/master.sfdisk
#挂载设备
	sudo losetup /dev/loop0 --partscan $@
#创建minux文件系统
	sudo mkfs.minix -n 14 /dev/loop0p1
#挂载文件系统
	sudo mount /dev/loop0p1 /mnt
#切换所有者
	sudo chown ${USER} /mnt
#创建目录
	mkdir -p /mnt/bin
	mkdir -p /mnt/home
	mkdir -p /mnt/mnt
#创建文件
	echo "hello onix!!!, from root directory file ..." > /mnt/hello.txt
#拷贝程序
	for app in $(BUILTIN_APPS);\
	do\
		cp $$app /mnt/bin; \
	done

	echo "hello onix!!!" > /mnt/hello.txt
#卸载文件系统
	sudo umount /mnt
#卸载设备
	sudo losetup -d /dev/loop0


$(BUILD)/slave.img : $(SRC)/utils/slave.sfdisk
#创建一个32M的硬盘镜像
	yes | bximage -mode=create -hd=32 -imgmode=flat -sectsize=512 -q $@
#执行硬盘分区
	sfdisk $@ < $(SRC)/utils/slave.sfdisk
#挂载设备
	sudo losetup /dev/loop0 --partscan $@
#创建minix文件系统
	sudo mkfs.minix -n 14 /dev/loop0p1
#挂载文件系统
	sudo mount /dev/loop0p1 /mnt
#切换所有者
	sudo chown ${USER} /mnt
#创建文件
	echo "hello onix!!!, from root directory file ..." > /mnt/hello.txt
#卸载文件系统
	sudo umount /mnt
#卸载设备
	sudo losetup -d /dev/loop0


.PHONY: mount0
mount0: $(BUILD)/master.img
	sudo losetup /dev/loop0 --partscan $<
	sudo mount /dev/loop0p1 /mnt
	sudo chown ${USER} /mnt 

.PHONY: umount0
umount0: /dev/loop0
	sudo umount /mnt
	sudo losetup -d $<

.PHONY: mount1
mount1: $(BUILD)/slave.img
	sudo losetup /dev/loop0 --partscan $<
	sudo mount /dev/loop0p1 /mnt
	sudo chown ${USER} /mnt 

.PHONY: umount1
umount1: /dev/loop0
	sudo umount /mnt
	sudo losetup -d $<

IMAGES:= $(BUILD)/master.img $(BUILD)/slave.img
#生成master.img 和 slave.img硬盘
.PHONY: img
img: $(IMAGES)

#使用bochs虚拟机
# .PHONY: bochs
# bochs: $(IMAGES)
# 	bochs -q -f $(BOCHS)/bochsrc -unlock

# .PHONY: bochsgdb
# bochsgdb: $(IMAGES)
# 	bochsgdb -q -f $(BOCHS)/bochsrcgdb -unlock

#使用qemu虚拟机
QEMU:= qemu-system-i386 # 虚拟机
QEMU+= -m 32M # 内存
QEMU+= -audiodev pa,id=hda # 音频设备
QEMU+= -machine pcspk-audiodev=hda # pcspeaker 设备
QEMU+= -rtc base=localtime # 设备本地时间
##############################################################################
#stdio 表示将字符输出到终端 vc是qemu的默认终端，可以在TAB(View -> Show Tabs中打开
#udp是用udp协议传输字符数据，主要用netcat来调式 
#可以用nc建立服务器端，监听端口号6666：
#nc -ulp 6666
#可以用nc建立客户端，连接好刚建立好的服务器
#nc -u localhost 6666
##############################################################################
QEMU+= -chardev stdio,mux=on,id=com1 # 字符设备 1
# QEMU+= -chardev vc,mux=on,id=com1 # 字符设备 1
# QEMU+= -chardev vc,mux=on,id=com2 # 字符设备 2
# QEMU+= -chardev udp,mux=on,id=com2,port=7777,ipv4=on # 字符设备 2
QEMU+= -serial chardev:com1 # 串口 1
# QEMU+= -serial chardev:com2 # 串口 2

QEMU_DISK:= -drive file=$(BUILD)/master.img,if=ide,index=0,media=disk,format=raw # 主硬盘
QEMU_DISK+= -drive file=$(BUILD)/slave.img,if=ide,index=1,media=disk,format=raw # 从硬盘

QEMU_DISK_BOOT:=-boot c

QEMU_DEBUG:= -s -S

.PHONY: qemu
qemu: $(IMAGES)
	$(QEMU) $(QEMU_DISK) $(QEMU_DISK_BOOT)

.PHONY: qemugdb
qemugdb: $(IMAGES)
	$(QEMU) $(QEMU_DISK) $(QEMU_DISK_BOOT) $(QEMU_DEBUG)

# VMWare 硬盘格式转换
$(BUILD)/master.vmdk: $(BUILD)/master.img
	qemu-img convert -O vmdk $< $@

.PHONY:vmdk
vmdk: $(BUILD)/master.vmdk


#############################################################
#kernel.iso 光盘启动(由grub引导)
############################################################

#生成iso光盘文件
$(BUILD)/kernel.iso : $(BUILD)/kernel.bin $(SRC)/utils/grub.cfg
# 检测内核文件是否合法
	grub-file --is-x86-multiboot2 $<
# 创建 iso 目录
	mkdir -p $(BUILD)/iso/boot/grub
# 拷贝内核文件
	cp $< $(BUILD)/iso/boot
# 拷贝 grub 配置文件
	cp $(SRC)/utils/grub.cfg $(BUILD)/iso/boot/grub
# 生成 iso 文件
	grub-mkrescue $(BUILD)/iso -o $@ 

#制作iso光盘文件
.PHONY:cdrom
cdrom: $(BUILD)/kernel.iso

#使用bochs虚拟机从光盘启动
.PHONY: bochsb
bochsb: $(BUILD)/kernel.iso
	bochs -q -f ../bochs/bochsrc.grub -unlock

QEMU_CDROM := -drive file=$(BUILD)/kernel.iso,media=cdrom # 光盘镜像
QEMU_CDROM_BOOT:= -boot d

#使用qemu虚拟机从盘启动
.PHONY: qemub
qemub: $(BUILD)/kernel.iso
	$(QEMU) $(QEMU_CDROM) $(QEMU_CDROM_BOOT) \
	# $(QEMU_DEBUG)




############################################################
#clean
############################################################
.PHONY: clean
clean:
	rm -rf $(BUILD)

