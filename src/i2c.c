#include "i2c.h"
#include "bno055.h"
#include "drv_pwm.h"
#include "sys.h"

#include <sys/ioctl.h>
#include <stdio.h>
#include <stdlib.h>
#include <fcntl.h>
#include <unistd.h>
#include <string.h>
#include <errno.h>

#ifdef __linux__
#include <linux/i2c-dev.h>
#endif

int I2C_BUS_FD;
struct bno055_t I2C_bno055;

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

		int buf_len = bytes + 1;
		uint8_t buf[buf_len];
		buf[0] = dstReg;
		memcpy(buf + 1, srcBuf, bytes);

		ioctl(fd, I2C_SLAVE, devAddr);
		if(write(fd, buf, buf_len) != buf_len)
		{
			return -1;
		}

		return 0;
	#endif
}

int i2c_read(int fd, uint8_t devAddr, uint8_t srcReg, void* dstBuf, size_t bytes)
{
#ifndef __linux__
	return 1;
#else

	ioctl(fd, I2C_SLAVE, devAddr);
	if(write(fd, &srcReg, 1) != 1)
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


s8 BNO055_I2C_bus_read(u8 dev_addr, u8 reg_addr, u8 *reg_data, u8 cnt)
{
	s32 BNO055_iERROR = BNO055_INIT_VALUE;

	if(i2c_read(I2C_BUS_FD, dev_addr, reg_addr, reg_data, cnt))
	{
		BNO055_iERROR = -1;
	}

	return (s8)BNO055_iERROR;
}


s8 BNO055_I2C_bus_write(u8 dev_addr, u8 reg_addr, u8 *reg_data, u8 cnt)
{
	s32 BNO055_iERROR = BNO055_INIT_VALUE;

	if(i2c_write_bytes(I2C_BUS_FD, dev_addr, reg_addr, reg_data, cnt))
	{
		BNO055_iERROR = -1;
	}

	return (s8)BNO055_iERROR;
}


void BNO055_delay_msec(u32 msek)
{
	usleep(1000 * msek);
}


void i2c_uninit()
{
	close(I2C_BUS_FD);
}


int i2c_init(const char* path)
{
	// open bus files
	I2C_BUS_FD = open(path, O_RDWR);

	if(I2C_BUS_FD < 0)
	{
		//EXIT("Failed to open I2C bus '%s'", path);
		return -1;
	}

	// Initalize the BNO055 driver
	I2C_bno055.bus_write = BNO055_I2C_bus_write;
	I2C_bno055.bus_read  = BNO055_I2C_bus_read;
	I2C_bno055.delay_msec= BNO055_delay_msec;
	I2C_bno055.dev_addr  = BNO055_I2C_ADDR1;

	if(bno055_init(&I2C_bno055) != BNO055_SUCCESS)
	{
		return -1;
	}


	bno055_set_power_mode(BNO055_POWER_MODE_NORMAL);
	//bno055_set_operation_mode(BNO055_OPERATION_MODE_IMUPLUS);//NDOF);
	bno055_set_operation_mode(BNO055_OPERATION_MODE_NDOF);

	uint8_t cal_status = 0;

	bno055_get_sys_calib_stat(&cal_status);

	b_log("BNO055 sys cal status: %d", cal_status);

	return 0;
}


int poll_i2c_devs(raw_state_t* state, raw_action_t* action, int* odo)
{
	uint8_t mode = 0;
	int res;

	if(action)
	{
		res = pwm_get_action(action);
		if(res) return res;
	}

	if(odo)
	{
		*odo = pwm_get_odo();
		if(*odo < 0) return *odo;
	}

	res = bno055_get_operation_mode(&mode);

	if(!state) return 3;

	if(bno055_read_accel_xyz((struct bno055_accel_t*)state->acc))
	{
		EXIT("Error reading from BNO055\n");
	}

	if(bno055_read_gyro_xyz((struct bno055_gyro_t*)state->rot_rate))
	{
		EXIT("Error reading from BNO055\n");
	}

	return 0;
}
