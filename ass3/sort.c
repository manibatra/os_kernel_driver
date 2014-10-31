/*
 * sort.c 
 *
 *
 */
#include <asm/uaccess.h> /* added for access_ok check in scull_sort_ioctl */
#include <linux/module.h>
#include <linux/moduleparam.h>
#include <linux/kernel.h>	/* printk(), min() */
#include <linux/slab.h>		/* kmalloc() */
#include <linux/fs.h>		/* everything... */
#include <linux/proc_fs.h>
#include <linux/errno.h>	/* error codes */
#include <linux/types.h>	/* size_t */
#include <linux/fcntl.h>
#include <linux/poll.h>
#include <linux/cdev.h>
#include <linux/sched.h>
#include <linux/sort.h> //for sorting the buffer content
#include "scull.h"		/* local definitions */

struct scull_sort {
	wait_queue_head_t inq, outq;       /* read and write queues */
    char *buffer, *end;                /* begin of buf, end of buf */
    int buffersize;                    /* used in pointer arithmetic */
    char *rp, *wp;                     /* where to read, where to write */
    int nreaders, nwriters;            /* number of openings for r/w */
    struct fasync_struct *async_queue; /* asynchronous readers */
    struct mutex mut;              /* mutual exclusion semaphore */
    struct cdev cdev;                  /* Char device structure */
};

/* parameters */
static int scull_sort_nr_devs = SCULL_S_NR_DEVS;	/* number of sort devices, 1 for now, can be incrsd in future */
int scull_sort_buffer =  SCULL_S_BUFFER;	/* buffer size */
dev_t scull_sort_devno;			/* Our first device number */

module_param(scull_sort_nr_devs, int, 0);	/* FIXME check perms */
module_param(scull_sort_buffer, int, 0);

static struct scull_sort *scull_sort_devices;

static int scull_sort_fasync(int fd, struct file *filp, int mode);
static int spacefree(struct scull_sort *dev);
/*
 * Open and close
 */


static int scull_sort_open(struct inode *inode, struct file *filp)
{
	struct scull_sort *dev;

	//printk(KERN_NOTICE "opening the device\n");
	//helps to find the right structure for dev
	dev = container_of(inode->i_cdev, struct scull_sort, cdev);
	filp->private_data = dev;

	if (mutex_lock_interruptible(&dev->mut))
		return -ERESTARTSYS;
	if (!dev->buffer) {
		/* allocate the buffer, set read/write pointers */
		dev->buffer = kmalloc(scull_sort_buffer, GFP_KERNEL);
		dev->buffersize = scull_sort_buffer;
		dev->end = dev->buffer + dev->buffersize;
		dev->rp = dev->wp = dev->buffer; /* rd and wr from the beginning */
		if (!dev->buffer) {
			mutex_unlock(&dev->mut);
			return -ENOMEM;
		}
	}

	/* use f_mode,not  f_flags: it's cleaner (fs/open.c tells why) */
	if (filp->f_mode & FMODE_READ)
		dev->nreaders++;
	if (filp->f_mode & FMODE_WRITE)
		dev->nwriters++;
	mutex_unlock(&dev->mut);
	//no attempts will be made to call pread and pwrite 
	return nonseekable_open(inode, filp);
}



static int scull_sort_release(struct inode *inode, struct file *filp)
{
	struct scull_sort *dev = filp->private_data;

	/* remove this filp from the asynchronously notified filp's */
	scull_sort_fasync(-1, filp, 0);
	mutex_lock(&dev->mut);
	if (filp->f_mode & FMODE_READ)
		dev->nreaders--;
	if (filp->f_mode & FMODE_WRITE)
		dev->nwriters--;
	mutex_unlock(&dev->mut);
	return 0;
}

//function to compare chars in the sort function
static int cmp_func(const void *lhs, const void *rhs) {
	
	char lhs_char = * (const char *) (lhs);
	char rhs_char = * (const char *) (rhs);

	if(lhs_char < rhs_char) return -1;
	if(lhs_char > rhs_char) return 1;
	
	return 0;
}

/*
 * Data management: read and write
 */

//free up space after each read, lock should be held whenever func called
void free_space(struct scull_sort *dev) {
	
	int i;
	//return if the read pointer is at the starting	
	if(dev->rp == dev->buffer)
		return;	
	//shift the data to the begining of the buffer
	for(i = 0; i <= (dev->wp - dev->rp); i++) {
		*(dev->buffer + i) = *(dev->rp + i);
	}

	//reposition the read and the write pointers
	dev->wp -= (dev->rp - dev->buffer);
	dev->rp = dev->buffer;
}
	
static ssize_t scull_sort_read (struct file *filp, char __user *buf, size_t count,
                loff_t *f_pos)
{
	struct scull_sort *dev = filp->private_data;
	//printk(KERN_CRIT "%s %s\n", dev->rp, dev->wp);
	if (mutex_lock_interruptible(&dev->mut))
		return -ERESTARTSYS;
	
	while (dev->rp == dev->wp) { /* nothing to read */
		mutex_unlock(&dev->mut); /* release the lock */
		if (filp->f_flags & O_NONBLOCK)
			return -EAGAIN;
		PDEBUG("\"%s\" reading: going to sleep\n", current->comm);
		if (wait_event_interruptible(dev->inq, (dev->rp != dev->wp))) {
			return -ERESTARTSYS; /* signal: tell the fs layer to handle it */
		}
		/* otherwise loop, but first reacquire the lock */
		if (mutex_lock_interruptible(&dev->mut))
			return -ERESTARTSYS;
	}
	//sort the data
	sort(dev->rp, dev->wp - dev->rp, sizeof(char), cmp_func, NULL);

	/* ok, data is there, return something */
	if (dev->wp > dev->rp)
		count = min(count, (size_t)(dev->wp - dev->rp));
	if (copy_to_user(buf, dev->rp, count)) {
		printk(KERN_CRIT "error in copying data to user space");
		mutex_unlock(&dev->mut);
		return -EFAULT;
	}
	dev->rp += count;
	//free up space in buffer
	free_space(dev);
	mutex_unlock(&dev->mut);

	/* finally, awake any writers and return */
	wake_up_interruptible(&dev->outq);
	PDEBUG("\"%s\" did read %li bytes\n",current->comm, (long)count);
	return count;
}

/* How much space is free? */
static int spacefree(struct scull_sort *dev)
{
	if (dev->rp == dev->wp)
		return dev->buffersize - 1;
	return (((dev->buffersize + dev->buffer) - dev->wp) - 1);
}

static ssize_t scull_sort_write(struct file *filp, const char __user *buf, size_t count,
                loff_t *f_pos)
{
	struct scull_sort *dev = filp->private_data;
	size_t total = count; //total data to be written
	size_t written = 0; //indicates the amount of data that has been written
	//printk("hello from write in sort.c\n");
	//char buffy[12];
	if (mutex_lock_interruptible(&dev->mut))
		return -ERESTARTSYS;

	//code to handle sleep	
	free_space(dev);
	mutex_unlock(&dev->mut);
	while(written < total) { //loop while entire data not written
		if (mutex_lock_interruptible(&dev->mut))
			return -ERESTARTSYS;
		while(spacefree(dev) == 0) {// loop while space not free
			mutex_unlock(&dev->mut);
			if(filp->f_flags & O_NONBLOCK)
				return -EAGAIN; //return if O_NONBLOCK
			if(wait_event_interruptible(dev->outq, spacefree(dev) > 0)) {
				return written;
			}
			if(mutex_lock_interruptible(&dev->mut))
				return -ERESTARTSYS;
		}


	/* ok, space is there, accept something */
		count = min(count, (size_t)spacefree(dev));
		if (copy_from_user(dev->wp, buf, count)) {
			mutex_unlock(&dev->mut);
			return -EFAULT;
		}
		dev->wp += count;
		written += count;
		buf += count; //move the source pointer
		count = (total - written); //data left to be written 
		mutex_unlock(&dev->mut);
	/* finally, awake any reader */
		wake_up_interruptible(&dev->inq);  
	}


	/* and signal asynchronous readers, explained late in chapter 5 */
	if (dev->async_queue)
		kill_fasync(&dev->async_queue, SIGIO, POLL_IN);
	//printk(KERN_CRIT "returning with %li chars written\n", (long)written);
	return written;
}


/* Poll and fsync function */
static unsigned int scull_sort_poll(struct file *filp, poll_table *wait)
{
	return 0;
}


static int scull_sort_fasync(int fd, struct file *filp, int mode)
{

	struct scull_sort *dev = filp->private_data;

	return fasync_helper(fd, filp, mode, &dev->async_queue);
}


/*IOCTL function */
long scull_sort_ioctl(struct file *filp, unsigned int cmd, unsigned long arg)
{
	int err = 0;
    int retval = 0;

	struct scull_sort *dev = filp->private_data;
        /*
 *          extract the type and number bitfields, and don't decode
 *           wrong cmds: return ENOTTY (inappropriate ioctl) before access_ok()
 *      */
    if (_IOC_TYPE(cmd) != SCULL_IOC_MAGIC) return -ENOTTY;
    if (_IOC_NR(cmd) > SCULL_IOC_MAXNR) return -ENOTTY;

        /*
 *          the direction is a bitmask, and VERIFY_WRITE catches R/W
 *          transfers. `Type' is user-oriented, while
 *          access_ok is kernel-oriented, so the concept of "read" and
 *          "write" is reversed
 *     */
    if (_IOC_DIR(cmd) & _IOC_READ)
    	err = !access_ok(VERIFY_WRITE, (void __user *)arg, _IOC_SIZE(cmd));
    else if (_IOC_DIR(cmd) & _IOC_WRITE)
        err =  !access_ok(VERIFY_READ, (void __user *)arg, _IOC_SIZE(cmd));
    if (err) return -EFAULT;

    switch(cmd) { //checking for the commands
		
    	case SCULL_IOCRESET: //reset the device
			mutex_lock(&dev->mut);
			dev->rp=dev->buffer;
			dev->wp=dev->buffer;
			dev->nreaders = 0;
			dev->nwriters = 0;
			mutex_unlock(&dev->mut);	
            break;

	  	default:  /* invalid cmd given */
        	return -ENOTTY;
  	}
   	return retval;

}


/*
 * The file operations for the sort device
 */
struct file_operations scull_sort_fops = {
	.owner =	THIS_MODULE,
	.llseek =	no_llseek,
	.read =		scull_sort_read,
	.write =	scull_sort_write,
	.poll =		scull_sort_poll,
	.unlocked_ioctl = scull_sort_ioctl,
	.open =		scull_sort_open,
	.release =	scull_sort_release,
	.fasync =	scull_sort_fasync,
};


/*
 * Set up a cdev entry.
 */
static void scull_sort_setup_cdev(struct scull_sort *dev, int index)
{
	int err, devno = scull_sort_devno + index;
	cdev_init(&dev->cdev, &scull_sort_fops);
	dev->cdev.owner = THIS_MODULE;
	err = cdev_add (&dev->cdev, devno, 1);
	/* Fail gracefully if need be */
	if (err)
		printk(KERN_NOTICE "Error %d adding scullsort%d", err, index);
}

 

/*
 * Initialize the sort devs; return how many we did.
 */
int scull_sort_init(dev_t firstdev)
{
	int i, result;
	result = register_chrdev_region(firstdev, scull_sort_nr_devs, "sculls");
	if (result < 0) {
		printk(KERN_NOTICE "Unable to get scullp region, error %d\n", result);
		return 0;
	}
	scull_sort_devno = firstdev;
	scull_sort_devices = kmalloc(scull_sort_nr_devs * sizeof(struct scull_sort), GFP_KERNEL);
	if (scull_sort_devices == NULL) {
		unregister_chrdev_region(firstdev, scull_sort_nr_devs);
		return 0;
	} //code left here in case no of devices have to be inc in the future
	memset(scull_sort_devices, 0, scull_sort_nr_devs * sizeof(struct scull_sort));
	for (i = 0; i < scull_sort_nr_devs; i++) {
		init_waitqueue_head(&(scull_sort_devices[i].inq));
		init_waitqueue_head(&(scull_sort_devices[i].outq));
		mutex_init(&scull_sort_devices[i].mut);
		scull_sort_setup_cdev(scull_sort_devices + i, i);
	}
#ifdef SCULL_DEBUG
	create_proc_read_entry("scullsort", 0, NULL, scull_read_p_mem, NULL);
#endif  
	//printk("the no of devices are %d", scull_sort_nr_devs);
	return scull_sort_nr_devs;
}

/*
 * This is called by cleanup_module or on failure.
 * It is required to never fail, even if nothing was initialized first
 */
void scull_sort_cleanup(void)
{
	int i;

#ifdef SCULL_DEBUG
	remove_proc_entry("scullsort", NULL);
#endif

	if (!scull_sort_devices)
		return; /* nothing else to release */
	//printk(KERN_CRIT "removing the device\n");

	for (i = 0; i < scull_sort_nr_devs; i++) {
		cdev_del(&scull_sort_devices[i].cdev);
		kfree(scull_sort_devices[i].buffer);
	}
	kfree(scull_sort_devices);
	unregister_chrdev_region(scull_sort_devno, scull_sort_nr_devs);
	scull_sort_devices = NULL; /* pedantic */
}
