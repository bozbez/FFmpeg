#ifndef _CEDAR_VE_H_
#define _CEDAR_VE_H_

enum IOCTL_CMD {
	IOCTL_UNKOWN = 0x100,
	IOCTL_GET_ENV_INFO,
	IOCTL_WAIT_VE_DE,
	IOCTL_WAIT_VE_EN,
	IOCTL_RESET_VE,
	IOCTL_ENABLE_VE,
	IOCTL_DISABLE_VE,
	IOCTL_SET_VE_FREQ,

	IOCTL_CONFIG_AVS2 = 0x200,
	IOCTL_GETVALUE_AVS2 ,
	IOCTL_PAUSE_AVS2 ,
	IOCTL_START_AVS2 ,
	IOCTL_RESET_AVS2 ,
	IOCTL_ADJUST_AVS2,
	IOCTL_ENGINE_REQ,
	IOCTL_ENGINE_REL,
	IOCTL_ENGINE_CHECK_DELAY,
	IOCTL_GET_IC_VER,
	IOCTL_ADJUST_AVS2_ABS,
	IOCTL_FLUSH_CACHE,
	IOCTL_SET_REFCOUNT,

	IOCTL_READ_REG = 0x300,
	IOCTL_WRITE_REG,

	IOCTL_SET_VOL = 0x400,
	IOCTL_WAIT_JPEG_DEC = 0x500,
};

struct cedarv_env_infomation {
	unsigned int phymem_start;
	int  phymem_total_size;
	unsigned int  address_macc;
};

struct cedarv_cache_range {
	long start;
	long end;
};

struct cedarv_regop {
    unsigned int addr;
    unsigned int value;
};

#endif
