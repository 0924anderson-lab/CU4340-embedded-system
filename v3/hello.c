/*
 * Userspace program that bounces the VGA ball by repeatedly updating
 * its position through the device driver.
 */

#include <fcntl.h>
#include <stdio.h>
#include <sys/ioctl.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <unistd.h>

#include "vga_ball.h"

#define DEVICE_NAME "/dev/vga_ball"
#define STEP_X 5
#define STEP_Y 4
#define FRAME_DELAY_US 16000

static int vga_ball_fd;

static int set_ball_position(unsigned short x, unsigned short y)
{
	vga_ball_arg_t vla;

	vla.pos.x = x;
	vla.pos.y = y;

	return ioctl(vga_ball_fd, VGA_BALL_WRITE_POSITION, &vla);
}

static int get_ball_position(vga_ball_pos_t *pos)
{
	vga_ball_arg_t vla;

	if (ioctl(vga_ball_fd, VGA_BALL_READ_POSITION, &vla))
		return -1;

	*pos = vla.pos;
	return 0;
}

int main(void)
{
	vga_ball_pos_t pos;
	int dx = STEP_X;
	int dy = STEP_Y;

	printf("VGA ball bouncing demo started\n");

	vga_ball_fd = open(DEVICE_NAME, O_RDWR);
	if (vga_ball_fd == -1) {
		perror("open(/dev/vga_ball) failed");
		return 1;
	}

	if (get_ball_position(&pos) == -1) {
		perror("ioctl(VGA_BALL_READ_POSITION) failed");
		close(vga_ball_fd);
		return 1;
	}

	printf("Initial position: (%u, %u)\n", pos.x, pos.y);

	for (;;) {
		if ((int)pos.x + dx >= VGA_BALL_SCREEN_WIDTH - VGA_BALL_RADIUS ||
		    (int)pos.x + dx < VGA_BALL_RADIUS)
			dx = -dx;

		if ((int)pos.y + dy >= VGA_BALL_SCREEN_HEIGHT - VGA_BALL_RADIUS ||
		    (int)pos.y + dy < VGA_BALL_RADIUS)
			dy = -dy;

		pos.x += dx;
		pos.y += dy;

		if (set_ball_position(pos.x, pos.y) == -1) {
            printf("bad pos=(%u,%u)\n", pos.x, pos.y);
            perror("ioctl(VGA_BALL_WRITE_POSITION) failed");
            close(vga_ball_fd);
            return 1;
        }

		usleep(FRAME_DELAY_US);
	}
}
