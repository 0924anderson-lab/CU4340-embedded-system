/*
 * Device driver for the VGA ball generator
 *
 * A platform device implemented using the misc subsystem.
 * Userspace controls the center position of a stationary ball through
 * two 10-bit coordinates.
 */

#include <linux/module.h>
#include <linux/init.h>
#include <linux/errno.h>
#include <linux/kernel.h>
#include <linux/platform_device.h>
#include <linux/miscdevice.h>
#include <linux/io.h>
#include <linux/of.h>
#include <linux/of_address.h>
#include <linux/fs.h>
#include <linux/uaccess.h>

#include "vga_ball.h"

#define DRIVER_NAME "vga_ball"

/*
 * Register map expected by the hardware in vga_ball.sv:
 *   offset 0: ball_x[7:0]
 *   offset 1: ball_x[9:8] in bits [1:0]
 *   offset 2: ball_y[7:0]
 *   offset 3: ball_y[9:8] in bits [1:0]
 */
#define BALL_X_LOW(base)  (base)
#define BALL_X_HIGH(base) ((base) + 1)
#define BALL_Y_LOW(base)  ((base) + 2)
#define BALL_Y_HIGH(base) ((base) + 3)

struct vga_ball_dev {
	struct resource res;
	void __iomem *virtbase;
	vga_ball_pos_t pos;
};

static struct vga_ball_dev dev;

static bool vga_ball_pos_valid(const vga_ball_pos_t *pos)
{
	return pos->x >= VGA_BALL_RADIUS &&
	       pos->x < VGA_BALL_SCREEN_WIDTH - VGA_BALL_RADIUS &&
	       pos->y >= VGA_BALL_RADIUS &&
	       pos->y < VGA_BALL_SCREEN_HEIGHT - VGA_BALL_RADIUS;
}

static void write_position(const vga_ball_pos_t *pos)
{
	iowrite8(pos->x & 0xff, BALL_X_LOW(dev.virtbase));
	iowrite8((pos->x >> 8) & 0x3, BALL_X_HIGH(dev.virtbase));
	iowrite8(pos->y & 0xff, BALL_Y_LOW(dev.virtbase));
	iowrite8((pos->y >> 8) & 0x3, BALL_Y_HIGH(dev.virtbase));
	dev.pos = *pos;
}

static long vga_ball_ioctl(struct file *f, unsigned int cmd, unsigned long arg)
{
	vga_ball_arg_t vla;

	(void)f;

	switch (cmd) {
	case VGA_BALL_WRITE_POSITION:
		if (copy_from_user(&vla, (void __user *)arg, sizeof(vla)))
			return -EFAULT;

		if (!vga_ball_pos_valid(&vla.pos))
			return -EINVAL;

		write_position(&vla.pos);
		return 0;

	case VGA_BALL_READ_POSITION:
		vla.pos = dev.pos;
		if (copy_to_user((void __user *)arg, &vla, sizeof(vla)))
			return -EFAULT;
		return 0;

	default:
		return -EINVAL;
	}
}

static const struct file_operations vga_ball_fops = {
	.owner = THIS_MODULE,
	.unlocked_ioctl = vga_ball_ioctl,
};

static struct miscdevice vga_ball_misc_device = {
	.minor = MISC_DYNAMIC_MINOR,
	.name = DRIVER_NAME,
	.fops = &vga_ball_fops,
};

static int __init vga_ball_probe(struct platform_device *pdev)
{
	vga_ball_pos_t center = {
		.x = VGA_BALL_SCREEN_WIDTH / 2,
		.y = VGA_BALL_SCREEN_HEIGHT / 2,
	};
	int ret;

	ret = misc_register(&vga_ball_misc_device);
	if (ret)
		return ret;

	ret = of_address_to_resource(pdev->dev.of_node, 0, &dev.res);
	if (ret) {
		ret = -ENOENT;
		goto out_deregister;
	}

	if (!request_mem_region(dev.res.start, resource_size(&dev.res),
				DRIVER_NAME)) {
		ret = -EBUSY;
		goto out_deregister;
	}

	dev.virtbase = of_iomap(pdev->dev.of_node, 0);
	if (!dev.virtbase) {
		ret = -ENOMEM;
		goto out_release_mem_region;
	}

	write_position(&center);

	pr_info(DRIVER_NAME ": mapped registers at %pa, initial position=(%u,%u)\n",
		&dev.res.start, center.x, center.y);

	return 0;

out_release_mem_region:
	release_mem_region(dev.res.start, resource_size(&dev.res));
out_deregister:
	misc_deregister(&vga_ball_misc_device);
	return ret;
}

static int vga_ball_remove(struct platform_device *pdev)
{
	iounmap(dev.virtbase);
	release_mem_region(dev.res.start, resource_size(&dev.res));
	misc_deregister(&vga_ball_misc_device);
	return 0;
}

#ifdef CONFIG_OF
static const struct of_device_id vga_ball_of_match[] = {
	{ .compatible = "csee4840,vga_ball-1.0" },
	{ },
};
MODULE_DEVICE_TABLE(of, vga_ball_of_match);
#endif

static struct platform_driver vga_ball_driver = {
	.driver = {
		.name = DRIVER_NAME,
		.owner = THIS_MODULE,
		.of_match_table = of_match_ptr(vga_ball_of_match),
	},
	.remove = __exit_p(vga_ball_remove),
};

static int __init vga_ball_init(void)
{
	pr_info(DRIVER_NAME ": init\n");
	return platform_driver_probe(&vga_ball_driver, vga_ball_probe);
}

static void __exit vga_ball_exit(void)
{
	platform_driver_unregister(&vga_ball_driver);
	pr_info(DRIVER_NAME ": exit\n");
}

module_init(vga_ball_init);
module_exit(vga_ball_exit);

MODULE_LICENSE("GPL");
MODULE_AUTHOR("Stephen A. Edwards, Columbia University");
MODULE_DESCRIPTION("VGA ball position driver");
