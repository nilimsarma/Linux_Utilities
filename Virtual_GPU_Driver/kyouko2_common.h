#define GRAPHICS_ON 1
#define GRAPHICS_OFF 0

#define VMODE		_IOW(0xcc, 0, unsigned long)
#define BIND_DMA	_IOW(0xcc, 1, unsigned long)
#define START_DMA	_IOWR(0xcc, 2, unsigned long)
#define SYNC		_IO(0xcc, 3)
#define FLUSH		_IO(0xcc, 4)
#define UNBIND_DMA 	_IOW(0xcc, 5, unsigned long)

typedef struct
{
	float* u_dma_addr;
	unsigned int count;
} dma_arg;
