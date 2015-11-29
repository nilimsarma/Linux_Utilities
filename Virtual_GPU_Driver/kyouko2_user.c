#include <stdio.h>
#include <stdlib.h>
#include <fcntl.h>
#include <linux/ioctl.h>
#include <unistd.h>
#include <stdint.h>
#include "kyouko2_common.h"

#define ITER_MAX 25
//#define RANDOM_TRIANGLES

typedef struct {
	uint32_t address	:14;
	uint32_t count		:10;
	uint32_t opcode		:8;
}dma_header;

float gen_random_num (float min, float max)
{
	return (rand() - 0.0)*(max - min)/(RAND_MAX - 0.0) + min;
}

int main() {
	int fd, result, i, triangle_count, vertices_count;
	float* p;
	int test;

	triangle_count  = 100;
	vertices_count = triangle_count*3;
	
	dma_header hdr;
	hdr.address = 0x1045;
	hdr.count = vertices_count;
	hdr.opcode = 0x14;

	dma_arg dma_arg_data;
	
	fd = open("/dev/kyouko2", O_RDWR);
	
	ioctl(fd, VMODE, GRAPHICS_ON);
	ioctl(fd, SYNC);

	ioctl(fd, BIND_DMA, &dma_arg_data.u_dma_addr);	

	//Receive address of buffer
	p = dma_arg_data.u_dma_addr; 
	*p = *(float *)&hdr;
	++p;
	
	int itr  = 0;
	int row, col;
	row = 0; col = 0;

//For testing purposes divides screen in 25 parts and let each buffer be mapped to a distinct part
//Thus 25 buffers can be tested for correct operation
//Otherwise use the flag RANDOM_TRAINGLES for random triangles
	do {
		for(i=0; i<triangle_count ;i++)
		{
			//Vertex 1
			
			*p++ = gen_random_num(0.0, 1.0); 		//B
			*p++ = gen_random_num(0.0, 1.0); 		//G
			*p++ = gen_random_num(0.0, 1.0); 		//R

#ifndef RANDOM_TRIANGLES
			*p++ = -0.8 + col*0.4; 					//X
			*p++ = -1.0 + row*0.4;   				//Y
			*p++ = 0.0; 		   					//Z
#else
			*p++ = gen_random_num(-1.0, 1.0);		//B
			*p++ = gen_random_num(-1.0, 1.0);		//G
			*p++ = gen_random_num(-1.0, 1.0);		//R
#endif
			//Vertex 2
			
			*p++ = gen_random_num(0.0, 1.0); 		//B
			*p++ = gen_random_num(0.0, 1.0); 		//G
			*p++ = gen_random_num(0.0, 1.0); 		//R

#ifndef RANDOM_TRIANGLES
			if( i%2 == 0)
				*p++ = -1.0 + col*0.4; 				//X
			else
				*p++ = -0.6 + col*0.4;
			*p++ = -0.8 + row*0.4;   				//Y
			*p++ = 0.0; 		   					//Z
#else
			*p++ = gen_random_num(-1.0, 1.0);		//B
			*p++ = gen_random_num(-1.0, 1.0);		//G
			*p++ = gen_random_num(-1.0, 1.0);		//R
#endif
			
			//Vertex 3
			
			*p++ = gen_random_num(0.0, 1.0); 		//B
			*p++ = gen_random_num(0.0, 1.0); 		//G
			*p++ = gen_random_num(0.0, 1.0); 		//R

#ifndef RANDOM_TRIANGLES
			*p++ = -0.8 + col*0.4; 					//X
			*p++ = -0.6 + row*0.4;   				//Y
			*p++ = 0.0; 		   					//Z
#else
			*p++ = gen_random_num(-1.0, 1.0);		//B
			*p++ = gen_random_num(-1.0, 1.0);		//G
			*p++ = gen_random_num(-1.0, 1.0);		//R
#endif
			
		}

		dma_arg_data.count = ( p - dma_arg_data.u_dma_addr )*sizeof(float);
		
		ioctl(fd, START_DMA, &dma_arg_data);	

		//Receive address of next buffer
		p = dma_arg_data.u_dma_addr; 
		*p = *(float *)&hdr;	p++;

		if(++col == 5) {
			col = 0; 
			if(++row == 5){
				row = 0;
			}
		}
		
	}while(++itr<ITER_MAX);
	
	ioctl(fd, SYNC);
	ioctl(fd, UNBIND_DMA, test);
	
	ioctl(fd, FLUSH);
	ioctl(fd, SYNC);
	
	sleep(5);
	ioctl(fd, VMODE, GRAPHICS_OFF);

	close(fd);
	return 0;
}

