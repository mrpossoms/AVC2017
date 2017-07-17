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

int i2c_write(int fd, uint8_t devAddr, uint8_t dstReg, uint8_t byte);
int i2c_read(int fd, uint8_t devAddr, uint8_t srcReg, void* dstBuf, size_t bytes);

#ifdef __cplusplus
}
#endif

#endif
