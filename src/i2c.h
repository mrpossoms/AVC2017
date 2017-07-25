#ifndef AVC_I2C
#define AVC_I2C

#include <inttypes.h>
#include <sys/types.h>
#include <sys/ioctl.h>

#ifdef __linux__
#include <linux/i2c-dev.h>
#endif

#ifdef __cplusplus
extern "C" {
#endif

extern int I2C_BUS_FDS[2];

int i2c_write(int fd, uint8_t devAddr, uint8_t dstReg, uint8_t byte);
int i2c_write_bytes(int fd, uint8_t devAddr, uint8_t dstReg, uint8_t* srcBuf, size_t bytes);
int i2c_read(int fd, uint8_t devAddr, uint8_t srcReg, void* dstBuf, size_t bytes);

#ifdef __cplusplus
}
#endif

#endif
