/*
 * A tool to read/write pci device's config space, and show it's IO space and memory space
 * Example:
 * 	rw_pci_config -s 0 -d 25
 * equals:
 *	lspci -x -s 00:19.0
 *
 * Type 0 PCI config space head (64 bytes):
 *	DW |    Byte3    |    Byte2    |    Byte1    |     Byte0     | Addr
 *	---+---------------------------------------------------------+-----
 *	 0 | 　　　　Device ID 　　　　| 　　　　Vendor ID 　　　　　|　00
 *	---+---------------------------------------------------------+-----
 *	 1 | 　　　　　Status　　　　　| 　　　　 Command　　　　　　|　04
 *	---+---------------------------------------------------------+-----
 *	 2 | 　　　　　　　Class Code　　　　　　　　|　Revision ID　|　08
 *	---+---------------------------------------------------------+-----
 *	 3 | 　　BIST　　| Header Type | Latency Timer | Cache Line  |　0C
 *	---+---------------------------------------------------------+-----
 *	 4 | 　　　　　　　　　　Base Address 0　　　　　　　　　　　|　10
 *	---+---------------------------------------------------------+-----
 *	 5 | 　　　　　　　　　　Base Address 1　　　　　　　　　　　|　14
 *	---+---------------------------------------------------------+-----
 *	 6 | 　　　　　　　　　　Base Address 2　　　　　　　　　　　|　18
 *	---+---------------------------------------------------------+-----
 *	 7 | 　　　　　　　　　　Base Address 3　　　　　　　　　　　|　1C
 *	---+---------------------------------------------------------+-----
 *	 8 | 　　　　　　　　　　Base Address 4　　　　　　　　　　　|　20
 *	---+---------------------------------------------------------+-----
 *	 9 | 　　　　　　　　　　Base Address 5　　　　　　　　　　　|　24
 *	---+---------------------------------------------------------+-----
 *	10 | 　　　　　　　　　CardBus CIS pointer　　　　　　　　　 |　28
 *	---+---------------------------------------------------------+-----
 *	11 |　　Subsystem Device ID　　| 　　Subsystem Vendor ID　　 |　2C
 *	---+---------------------------------------------------------+-----
 *	12 | 　　　　　　　Expansion ROM Base Address　　　　　　　　|　30
 *	---+---------------------------------------------------------+-----
 *	13 | 　　　　　　　Reserved(Capability List)　　　　　　　　 |　34
 *	---+---------------------------------------------------------+-----
 *	14 | 　　　　　　　　　　　Reserved　　　　　　　　　　　　　|　38
 *	---+---------------------------------------------------------+-----
 *	15 | 　Max_Lat　 | 　Min_Gnt　 | 　IRQ Pin　 | 　IRQ Line　　|　3C
 *	-------------------------------------------------------------------
 *
 */
#include <sys/io.h>
#include <stdio.h>
#include <stdlib.h>
#include <getopt.h>
#include <unistd.h>
#include <sys/mman.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <string.h>

#define ADDR_CONFIG_REG				0x0CF8
#define DATA_CONFIG_REG				0x0CFC
#define PCI_CONFIG_ADDR(bus, dev, fn, reg) 	(0x80000000 | (bus << 16) | (dev << 11) | (fn << 8) | (reg & ~3))

void usage()
{
	printf("Usage: readpci [-bdfrth]\n\
	        -a   addr  :   specify bar address default 0\n\
	        -b   bus   :   specify PCI bus number(default 0)\n\
	        -d   dev   :   device number(default 0)\n\
	        -f   fn    :   function number(default 0)\n\
	        -r   reg   :   register address(must be multiple of 4, default 0)\n\
	        -p   port  :   specify port number default 0\n\
	        -v   value :   write a integer value into the address\n\
	        -h         :   print this help text\n ");
	exit(-1);
}

int operaMem(int bar, int offset, int modValue, int isWrite)
{
	int i;
	int fd;
	char* mem;

	//open /dev/mem with read and write mode
	if ((fd = open ("/dev/mem", O_RDWR)) < 0) {
		perror ("open error\n");
		return -1;
	}

	//map physical memory 0-10 bytes
	mem = mmap (0, 4096, PROT_READ | PROT_WRITE, MAP_SHARED, fd, bar);
	if (mem == MAP_FAILED) {
		perror ("mmap error:\n");
		return 1;
	}

	int value = *((int *)(mem + offset));
	printf("The value at 0x%x is 0x%x\n", bar + offset, value);
	if (isWrite == 1) {
		printf("Write value 0x%x at 0x%x\n", modValue, bar + offset);
		memcpy(mem + offset, &modValue, 4);
		printf("Reread the value at 0x%x is 0x%x\n", bar + offset, *((int *)(mem + offset)));
	}

	munmap (mem, 4096); //destroy map memory
	close (fd);         //close file
	return 0;
}

int parseInt(char *str)
{
    int value = 0;

    char tmpChar;
    while((tmpChar = *str) != '\0')
    {
        if(tmpChar >= 'A' && tmpChar <= 'F')
        {
            tmpChar = tmpChar - 'A' + 10;
        }
        else if(tmpChar >= 'a' && tmpChar <= 'f')
        {
            tmpChar = tmpChar - 'a' + 10;
        }
        else
        {
            tmpChar -= '0';
        }
        value = value * 16 + tmpChar;
        str += 1;
    }
    return value;
}

int main(int argc, char **argv)
{
	unsigned long val = 0;
	char options[] = "a:b:d:f:r:v:p:h";
	int addr = 0, bus = 0, dev = 0, fn = 0, reg = 0, port = 0;
	int opt, value = 0, isWrite = 0, isReadBar = 0;

	while ((opt = getopt(argc, argv, options)) != -1) {
		switch (opt) {
		case 'a':
		    addr = parseInt(optarg);
		    break;
		case 'b':	/* bus number */
		    bus = atoi(optarg);
		    break;
		case 'd':	/* device number */
		    dev = atoi(optarg);
		    break;
		case 'f':	/* function number */
		    fn = atoi(optarg);
		    break;
		case 'r':	/* register number */
		    reg = parseInt(optarg);
		    break;
		case 'p':
		    port = atoi(optarg);
		    isReadBar = 1;
		    break;
		case 'v':
		    value = parseInt(optarg);
		    isWrite = 1;
		    break;
		case 'h':
		default:
		    usage();
		    break;
		}
	}

	iopl(3);

	if (isWrite == 0) {	/* read PCI config space */
		if (isReadBar == 0) {
			int i;
			for(i = 0; i < 64; i += 4) {
			    outl(PCI_CONFIG_ADDR(bus, dev, fn, i), ADDR_CONFIG_REG);
			    val = inl(DATA_CONFIG_REG);
			    printf("PCI:Bus %d, DEV %d, FUNC %d, REG %x, Value is %x\n", bus, dev, fn, i, val);
			}
		} else {	/* */
			int pointAddr = val + port * 0x1000;

			outl(PCI_CONFIG_ADDR(bus, dev, fn, 0x10), ADDR_CONFIG_REG);
			val = inl(DATA_CONFIG_REG) & 0xfffffff0;
			printf("The base address value is 0x%x\n", val);

			printf("The offset address value is 0x%x\n", pointAddr + addr);
			operaMem(pointAddr, addr, 0, 0);
		}
	} else {	/* write PCI config space */
		int pointAddr = val + port * 0x1000;

		outl(PCI_CONFIG_ADDR(bus, dev, fn, 0x10), ADDR_CONFIG_REG);
		val = inl(DATA_CONFIG_REG) & 0xfffffff0;
		printf("The base address value is 0x%x\n", val);

		printf("The offset address value is 0x%x\n", pointAddr + addr);
		operaMem(pointAddr, addr, value, 1);
	}
	return 0;
}
