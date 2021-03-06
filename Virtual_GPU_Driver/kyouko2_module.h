#define MAJOR_NUM 500
#define MINOR_NUM 127

#define CONFIG_REBOOT 0x1000
#define CONFIG_MODESET 0x1008
#define CONFIG_ACCELERATION 0x1010
#define CONFIG_INTERRUPT 0X100c

#define FRAME_COLUMNS 0x8000
#define FRAME_ROWS 0x8004
#define FRAME_ROWPITCH 0x8008
#define FRAME_PIXELFORMAT 0x800c
#define FRAME_STARTADDRESS 0x8010

#define DAC_WIDTH 0x9000
#define DAC_HEIGHT 0x9004
#define DAC_OFFSETX 0x9008
#define DAC_OFFSETY 0x900c
#define DAC_FRAME 0x9010

#define RASTER_PRIMITIVE 0x3000
#define RASTER_EMIT 0x3004
#define RASTERFLUSH 0x3ffc
#define RASTERCLEAR 0x3008

#define FIFO_DEPTH 0x4004
#define INFO_STATUS 0x4008

#define DRAW_VERTEX_COORD4F_X 0x5000
#define DRAW_VERTEX_COORD4F_Y 0x5004
#define DRAW_VERTEX_COORD4F_Z 0x5008
#define DRAW_VERTEX_COORD4F_W 0x500C

#define DRAW_VERTEX_COLOR4F_B 0x5010
#define DRAW_VERTEX_COLOR4F_G 0x5014
#define DRAW_VERTEX_COLOR4F_R 0x5018
#define DRAW_VERTEX_COLOR4F_A 0x501C

#define CLEARCOLOR 0x5100

#define STREAM_BUFFER_A_ADD		0x2000
#define STREAM_BUFFER_A_CONFIG	0x2008
#define STREAM_BUFFER_B_ADD		0x2004
#define STREAM_BUFFER_B_CONFIG	0x200c






