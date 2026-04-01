#ifndef _VGA_BALL_H
#define _VGA_BALL_H

#ifdef __KERNEL__
#include <linux/ioctl.h>
#else
#include <sys/ioctl.h>
#endif

#define VGA_BALL_SCREEN_WIDTH 640
#define VGA_BALL_SCREEN_HEIGHT 480
#define VGA_BALL_RADIUS 8

typedef struct {
	unsigned short x;
	unsigned short y;
} vga_ball_pos_t;

typedef struct {
	vga_ball_pos_t pos;
} vga_ball_arg_t;

#define VGA_BALL_MAGIC 'q'

/* ioctls and their arguments */
#define VGA_BALL_WRITE_POSITION _IOW(VGA_BALL_MAGIC, 1, vga_ball_arg_t)
#define VGA_BALL_READ_POSITION  _IOR(VGA_BALL_MAGIC, 2, vga_ball_arg_t)

#endif
