#include "i2c.h"

#include <sys/ioctl.h>
#include <fcntl.h>
#include <unistd.h>

#ifdef __linux__
#include <linux/i2c-dev.h>
#endif

int i2c_write(int fd, uint8_t devAddr, uint8_t dstReg, uint8_t byte)
{
#ifndef __linux__
	return 1;
#else

	uint8_t buf[] = { dstReg, byte };

	ioctl(fd, I2C_SLAVE, devAddr);
	write(fd, buf, 2);

	return 0;
#endif
}

int i2c_read(int fd, uint8_t devAddr, uint8_t srcReg, void* dstBuf, size_t bytes)
{
#ifndef __linux__
	return 1;
#else

	ioctl(fd, I2C_SLAVE, devAddr);
	
	uint8_t commByte = 0x80 | srcReg;
	if(write(fd, &commByte, 1) != 1)
	{
		return -1;
	}
	
	if(read(fd, dstBuf, bytes) != bytes)
	{
		return -2;
	}

	return 0;
#endif
}
