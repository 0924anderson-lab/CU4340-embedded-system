/*
 * Device driver for the VGA ball generator
 *
 * A Platform device implemented using the misc subsystem
 *
 * Adapted for position-controlled VGA ball
 */

#include <linux/module.h>
#include <linux/init.h>
#include <linux/errno.h>
#include <linux/version.h>
#include <linux/kernel.h>
#include <linux/platform_device.h>
#include <linux/miscdevice.h>
#include <linux/slab.h>
#include <linux/io.h>
#include <linux/of.h>
#include <linux/of_address.h>
#include <linux/fs.h>
#include <linux/uaccess.h>
#include <linux/types.h>
#include "vga_ball.h"

#define DRIVER_NAME "vga_ball"

/* Device registers */
#define BALL_X_LOW(x)   (x)
#define BALL_X_HIGH(x)  ((x) + 1)
#define BALL_Y_LOW(x)   ((x) + 2)
#define BALL_Y_HIGH(x)  ((x) + 3)

/*
 * Information about our device
 */
struct vga_ball_dev {
	struct resource res;
	void __iomem *virtbase;
	vga_ball_pos_t pos;
} dev;

/*
 * Write ball position to hardware
 * Register map:
 *   offset 0: X[7:0]
 *   offset 1: X[9:8] in bits [1:0]
 *   offset 2: Y[7:0]
 *   offset 3: Y[9:8] in bits [1:0]
 */
static void write_position(vga_ball_pos_t *pos)
{
	iowrite8(pos->x & 0xff, BALL_X_LOW(dev.virtbase));
	iowrite8((pos->x >> 8) & 0x3, BALL_X_HIGH(dev.virtbase));
	iowrite8(pos->y & 0xff, BALL_Y_LOW(dev.virtbase));
	iowrite8((pos->y >> 8) & 0x3, BALL_Y_HIGH(dev.virtbase));

	dev.pos = *pos;
}

/*
 * Handle ioctl() calls from userspace
 */
static long vga_ball_ioctl(struct file *f, unsigned int cmd, unsigned long arg)
{
	vga_ball_arg_t vla;

	switch (cmd) {
	case VGA_BALL_WRITE_POSITION:
		if (copy_from_user(&vla, (vga_ball_arg_t __user *)arg,
				   sizeof(vga_ball_arg_t)))
			return -EACCES;

		if (vla.pos.x > 639 || vla.pos.y > 479)
			return -EINVAL;

		write_position(&vla.pos);
		break;

	case VGA_BALL_READ_POSITION:
		vla.pos = dev.pos;
		if (copy_to_user((vga_ball_arg_t __user *)arg, &vla,
				 sizeof(vga_ball_arg_t)))
			return -EACCES;
		break;

	default:
		return -EINVAL;
	}

	return 0;
}

/* The operations our device knows how to do */
static const struct file_operations vga_ball_fops = {
	.owner = THIS_MODULE,
	.unlocked_ioctl = vga_ball_ioctl,
};

/* Information about our device for the misc framework */
static struct miscdevice vga_ball_misc_device = {
	.minor = MISC_DYNAMIC_MINOR,
	.name = DRIVER_NAME,
	.fops = &vga_ball_fops,
};

/*
 * Initialization code: get resources and set an initial position
 */
static int __init vga_ball_probe(struct platform_device *pdev)
{
	vga_ball_pos_t center = { 320, 240 };
	int ret;

	ret = misc_register(&vga_ball_misc_device);
	if (ret)
		return ret;

	ret = of_address_to_resource(pdev->dev.of_node, 0, &dev.res);
	if (ret) {
		ret = -ENOENT;
		goto out_deregister;
	}

	if (request_mem_region(dev.res.start, resource_size(&dev.res),
			       DRIVER_NAME) == NULL) {
		ret = -EBUSY;
		goto out_deregister;
	}

	dev.virtbase = of_iomap(pdev->dev.of_node, 0);
	if (dev.virtbase == NULL) {
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

/* Clean-up code */
static int vga_ball_remove(struct platform_device *pdev)
{
	iounmap(dev.virtbase);
	release_mem_region(dev.res.start, resource_size(&dev.res));
	misc_deregister(&vga_ball_misc_device);
	return 0;
}

/* Which compatible string(s) to search for in the Device Tree */
#ifdef CONFIG_OF
static const struct of_device_id vga_ball_of_match[] = {
	{ .compatible = "csee4840,vga_ball-1.0" },
	{ },
};
MODULE_DEVICE_TABLE(of, vga_ball_of_match);
#endif

/* Information for registering ourselves as a platform driver */
static struct platform_driver vga_ball_driver = {
	.driver = {
		.name = DRIVER_NAME,
		.owner = THIS_MODULE,
		.of_match_table = of_match_ptr(vga_ball_of_match),
	},
	.remove = __exit_p(vga_ball_remove),
};

/* Called when the module is loaded */
static int __init vga_ball_init(void)
{
	pr_info(DRIVER_NAME ": init\n");
	return platform_driver_probe(&vga_ball_driver, vga_ball_probe);
}

/* Called when the module is unloaded */
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