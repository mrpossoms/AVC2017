#include "i2c.h"

#include <sys/ioctl.h>
#include <fcntl.h>
#include <unistd.h>
#include <string.h>

#ifdef __linux__
#include <linux/i2c-dev.h>
#endif

int I2C_BUS_FDS[2];

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

int i2c_write_bytes(int fd, uint8_t devAddr, uint8_t dstReg, uint8_t* srcBuf, size_t bytes)
{
	#ifndef __linux__
		return 1;
	#else

		uint8_t buf[bytes + 1];
		buf[0] = dstReg; 
		memcpy(buf + 1, srcBuf, bytes);

		ioctl(fd, I2C_SLAVE, devAddr);
		write(fd, buf, bytes + 1);

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
