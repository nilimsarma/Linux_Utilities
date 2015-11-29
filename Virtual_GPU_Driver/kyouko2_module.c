#include <linux/init.h>
#include <linux/module.h>
#include <linux/kernel.h>
#include <linux/kernel_stat.h>
#include <linux/fs.h>
#include <linux/cdev.h>
#include <linux/pci.h>
#include <linux/delay.h>
#include <linux/mm.h>
#include <linux/mman.h>
#include <linux/interrupt.h>
#include <linux/spinlock.h>

#include "kyouko2_module.h"
#include "kyouko2_common.h"

#define PCI_VENDOR_ID_CCORSI 0x1234
#define PCI_DEVICE_ID_CCORSI_KYOUKO2 0x1113
#define DMA_BUFFER_SIZE (124*1024)
#define NUM_DMA_BUFFER 8

MODULE_LICENSE("GPL");
MODULE_AUTHOR("Dhaval Parmar");
MODULE_DESCRIPTION("Driver for kyouko2 display card.");

struct kyouko2_vars {
	struct pci_dev* pci_dev;
	unsigned long p_control_base;
	unsigned long p_control_length;
	unsigned int* k_control_base;
	unsigned int* k_card_ram_base;
	unsigned long card_ram_base;
	unsigned long card_ram_length;
	dma_addr_t p_dma_base;
	dma_addr_t p_dma_base_array[NUM_DMA_BUFFER];
	unsigned int* k_dma_base;
	unsigned long u_dma_base[NUM_DMA_BUFFER];
	unsigned int graphics_mode_on;
	int fill;
	int drain;
	bool sleep;
	bool usr_done;
	spinlock_t my_lock;
	dma_arg dma_arg_data;
};

static struct kyouko2_vars kyouko2v;

static int kyouko2_probe(struct pci_dev *pci_dev, const struct pci_device_id *pci_id) {
	int ret = 0;
	kyouko2v.p_control_base = pci_resource_start(pci_dev, 1);
	kyouko2v.p_control_length = pci_resource_len(pci_dev, 1);
	kyouko2v.card_ram_base = pci_resource_start(pci_dev, 2);
	kyouko2v.card_ram_length = pci_resource_len(pci_dev, 2);  
	ret = pci_enable_device(pci_dev);
	if(ret<0) {
		printk(KERN_ALERT "\nError: Could not enable device!");
		return ret;
	}
	pci_set_master(pci_dev);
	kyouko2v.pci_dev = pci_dev;
	return 0;
}

static int kyouko2_open(struct inode *inode, struct file *fp) {
	printk(KERN_ALERT "\nOpening kyouko2...");
	kyouko2v.k_control_base = ioremap(kyouko2v.p_control_base, kyouko2v.p_control_length);
	kyouko2v.k_card_ram_base = ioremap(kyouko2v.card_ram_base, kyouko2v.card_ram_length);
	kyouko2v.graphics_mode_on = 0;
	printk(KERN_ALERT "\nOpened device kyouko2.");
	return 0;
}

static unsigned int K_READ_REG(unsigned int reg) {
	unsigned int value;
	udelay(1);
	rmb();
	value = *(kyouko2v.k_control_base+(reg>>2));
	return value;
}

static void K_WRITE_REG(unsigned int reg, unsigned int value) {
	*(kyouko2v.k_control_base+(reg>>2)) = value;
}

static int kyouko2_release(struct inode *inode, struct file *fp) {
	printk(KERN_ALERT "\nClosing kyouko2...");
	iounmap(kyouko2v.k_control_base);
	iounmap(kyouko2v.k_card_ram_base);
	return 0;
}

static int kyouko2_mmap(struct file *fp, struct vm_area_struct *vma) {
	int ret = 0;
	if(vma->vm_pgoff<<PAGE_SHIFT == 0x00) {
	ret = io_remap_pfn_range(vma, vma->vm_start,
	                         kyouko2v.p_control_base>>PAGE_SHIFT,
	                         vma->vm_end - vma->vm_start,
	                         vma->vm_page_prot);
	}
	else if(vma->vm_pgoff<<PAGE_SHIFT == 0x80000000) {
	ret = io_remap_pfn_range(vma, vma->vm_start,
	                         kyouko2v.card_ram_base>>PAGE_SHIFT,
	                         vma->vm_end - vma->vm_start,
	                         vma->vm_page_prot);
	}
	else if(vma->vm_pgoff<<PAGE_SHIFT == 0x40000000) {
	ret = io_remap_pfn_range(vma, vma->vm_start,
	                         kyouko2v.p_dma_base>>PAGE_SHIFT,
	                         vma->vm_end - vma->vm_start,
	                         vma->vm_page_prot);
	}
	return ret;
}

inline void sync(void) {
	while(K_READ_REG(FIFO_DEPTH)>0);
}

DECLARE_WAIT_QUEUE_HEAD(dma_snooze);

static irqreturn_t dma_intr_handler(int irq, void* dev_id, struct pt_regs* regs)
{
	unsigned long p_drain_dma_base;
	unsigned int flags;
	
	flags = K_READ_REG(INFO_STATUS);
	K_WRITE_REG(INFO_STATUS, flags&0x0F);
	sync();

	//spurious interrupts
	if( (flags&0x02) == 0x00 )	{
		return(IRQ_NONE);
	}

	//user is not done filling buffers.
	//Common case
	if(kyouko2v.usr_done == 0) {
	
		//queue is full
		if ( (kyouko2v.sleep == 1) ) {
				kyouko2v.sleep = 0;
				wake_up_interruptible(&dma_snooze);
		}
		
		kyouko2v.drain = (kyouko2v.drain+1)%NUM_DMA_BUFFER;	

		//queue not empty
		if(kyouko2v.drain != kyouko2v.fill) {
			p_drain_dma_base = kyouko2v.p_dma_base_array[kyouko2v.drain];
			K_WRITE_REG(STREAM_BUFFER_A_ADD, p_drain_dma_base);
			K_WRITE_REG(STREAM_BUFFER_A_CONFIG, kyouko2v.dma_arg_data.count);
			sync();
		}
		return (IRQ_HANDLED);
	}
	
	//User might be done filling buffers and waiting for all the pending interrupts to trigger before exiting the program
	kyouko2v.drain = (kyouko2v.drain+1)%NUM_DMA_BUFFER;	
	
	//queue is empty
	if(kyouko2v.drain == kyouko2v.fill) { 
	
		kyouko2v.sleep = 0;
		wake_up_interruptible(&dma_snooze);
		return (IRQ_HANDLED);
	}

	//queue not empty
	if(kyouko2v.drain != kyouko2v.fill) {
		p_drain_dma_base = kyouko2v.p_dma_base_array[kyouko2v.drain];
		K_WRITE_REG(STREAM_BUFFER_A_ADD, p_drain_dma_base);
		K_WRITE_REG(STREAM_BUFFER_A_CONFIG, kyouko2v.dma_arg_data.count);
		sync();
	}
	return (IRQ_HANDLED);
}

static void initiate_transfer(void)
{
	unsigned long p_drain_dma_base;
	unsigned long flags;

	spin_lock_irqsave( &(kyouko2v.my_lock), flags);
	
	//queue empty
	if(kyouko2v.fill == kyouko2v.drain){	
	
		spin_unlock_irqrestore( &(kyouko2v.my_lock), flags);
		kyouko2v.fill = (kyouko2v.fill+1)%NUM_DMA_BUFFER;		
		p_drain_dma_base = kyouko2v.p_dma_base_array[kyouko2v.drain];
		
		K_WRITE_REG(STREAM_BUFFER_A_ADD, p_drain_dma_base);
		K_WRITE_REG(STREAM_BUFFER_A_CONFIG, kyouko2v.dma_arg_data.count);
		sync();
		return;
	}

	//queue not empty
	kyouko2v.fill = (kyouko2v.fill+1)%NUM_DMA_BUFFER;
	
	//queue is full, wait
	if(kyouko2v.fill == kyouko2v.drain) { 
		
		kyouko2v.sleep = 1;
		spin_unlock_irqrestore( &(kyouko2v.my_lock), flags);
		
		//incase interrupts are triggered before user process can go to sleep and queue becomes empty
		//sleep == 0, set by the interrupt handler and user process will not go to sleep
		wait_event_interruptible(dma_snooze, kyouko2v.sleep == 0);
	}
	else {
		spin_unlock_irqrestore( &(kyouko2v.my_lock), flags);
	}
	return;
}

static long kyouko2_ioctl(struct file* fp, unsigned int cmd, unsigned long arg) 
{
	unsigned long flags;
	unsigned long u_dma_buffer;
	int i;

	float red = 1.0;
	float blue = 1.0;
	float green = 1.0;
	float alpha = 1.0;

	switch(cmd) {
	case VMODE:
	  if(arg == GRAPHICS_ON) {
	  	
	    printk(KERN_ALERT "\nTurning ON Graphics...");
	    
		K_WRITE_REG(FRAME_COLUMNS, 1024);        
		K_WRITE_REG(FRAME_ROWS, 768);        
		K_WRITE_REG(FRAME_ROWPITCH, 1024*4);        
		K_WRITE_REG(FRAME_PIXELFORMAT, 0xf888);
		K_WRITE_REG(FRAME_STARTADDRESS, 0);
		sync();

		K_WRITE_REG(DAC_WIDTH, 1024);
		K_WRITE_REG(DAC_HEIGHT, 768);
		K_WRITE_REG(DAC_OFFSETX, 0);
		K_WRITE_REG(DAC_OFFSETY, 0);
		K_WRITE_REG(DAC_FRAME, 0);
		sync();

		K_WRITE_REG(CONFIG_ACCELERATION, 0x40000000);
		K_WRITE_REG(CONFIG_MODESET, 0);

		K_WRITE_REG(CLEARCOLOR, *((unsigned int*)&red));
		K_WRITE_REG(CLEARCOLOR + 0x0004, *((unsigned int*)&green));
		K_WRITE_REG(CLEARCOLOR + 0x0008, *((unsigned int*)&blue));
		K_WRITE_REG(CLEARCOLOR + 0x000c, *((unsigned int*)&alpha));
		sync();

		K_WRITE_REG(RASTERFLUSH, 0);
		K_WRITE_REG(RASTERCLEAR, 1);
		sync();
	    kyouko2v.graphics_mode_on = 1;
		printk(KERN_ALERT "\nGraphics ON...");
	  }
	  
	  else if(arg == GRAPHICS_OFF) {
	    K_WRITE_REG(CONFIG_REBOOT, 0);
		sync();
	    kyouko2v.graphics_mode_on = 0;
	    printk(KERN_ALERT "\nGraphics OFF...");
	  }
	  break;

	case SYNC:
	  sync();
	  break;

	case FLUSH:
	  K_WRITE_REG(RASTERFLUSH, 0);
	  sync();
	  break;

	case BIND_DMA:

		//configure interrupts
		pci_enable_msi(kyouko2v.pci_dev);
		if ( request_irq(kyouko2v.pci_dev->irq, (irq_handler_t)dma_intr_handler, IRQF_DISABLED|IRQF_SHARED, "dma_intr_handler", &kyouko2v) != 0)
			printk(KERN_ALERT "\nrequest_irq error !!!");
		//request failure hasn't been handles except for a print message
		//Ideally user shouldn't be allowed to proceed
		
		K_WRITE_REG(CONFIG_INTERRUPT, 0x02);
		sync();

		//Allocate 8 DMA buffers
		kyouko2v.k_dma_base = pci_alloc_consistent(kyouko2v.pci_dev, DMA_BUFFER_SIZE*NUM_DMA_BUFFER, &(kyouko2v.p_dma_base) );		
		u_dma_buffer = vm_mmap(fp, 0, DMA_BUFFER_SIZE*NUM_DMA_BUFFER, PROT_READ|PROT_WRITE, MAP_SHARED, 0x40000000);

		for(i = 0; i<NUM_DMA_BUFFER; i++)
		{
			kyouko2v.u_dma_base[i] = u_dma_buffer + DMA_BUFFER_SIZE*i;
			kyouko2v.p_dma_base_array[i] = kyouko2v.p_dma_base + DMA_BUFFER_SIZE*i;
		}

		//Initialize variables
		kyouko2v.fill = 0;
		kyouko2v.drain = 0;
		kyouko2v.usr_done = 0;
		kyouko2v.sleep = 0;
		spin_lock_init(&(kyouko2v.my_lock) );

		//Return to user the address of first DMA buffer	
		if( copy_to_user ( (void*)arg, &(kyouko2v.u_dma_base[kyouko2v.fill]), sizeof(unsigned int*)) != 0 )
			printk(KERN_ALERT "\nCopy to user in BIND_DMA could not copy all elements");
		
		break;

	case START_DMA:

		//Receive from user the count value for DMA buffer
		if( copy_from_user (&(kyouko2v.dma_arg_data), (void*)arg, sizeof(dma_arg)) != 0)
			printk(KERN_ALERT "\nCopy from user in START_DMA could not copy all elements");

		//if count is zero, return the same buffer address
		if( ((dma_arg*)arg)->count != 0)	initiate_transfer();

		//Return the next buffer address to user
		if( copy_to_user ( &( ((dma_arg*)arg)->u_dma_addr ), &(kyouko2v.u_dma_base[kyouko2v.fill]), sizeof(unsigned int*)) != 0 )
			printk(KERN_ALERT "\nCopy to user in START_DMA could not copy all elements");
		
		break;

	case UNBIND_DMA:

		//User has finished with all its data. Now it has to wait till pending interrrupts are triggered before exiting the program
		//queue is not empty.. wait
		spin_lock_irqsave( &(kyouko2v.my_lock), flags);
		if(kyouko2v.fill != kyouko2v.drain) { 

			kyouko2v.usr_done = 1;
			kyouko2v.sleep = 1;

			spin_unlock_irqrestore( &(kyouko2v.my_lock), flags);
			//incase interrupt is triggered before user process can go to sleep and queue becomes empty
			//sleep == 0, set by the interrupt handler and user process will not go to sleep
			wait_event_interruptible(dma_snooze, kyouko2v.sleep == 0);
		}
		else {
			spin_unlock_irqrestore( &(kyouko2v.my_lock), flags);
		}

		//return all allocated resources
		free_irq(kyouko2v.pci_dev->irq, &kyouko2v);
		pci_disable_msi(kyouko2v.pci_dev);
		vm_munmap(kyouko2v.u_dma_base[0], DMA_BUFFER_SIZE*NUM_DMA_BUFFER);
		pci_free_consistent(kyouko2v.pci_dev, DMA_BUFFER_SIZE*NUM_DMA_BUFFER, kyouko2v.k_dma_base, kyouko2v.p_dma_base);
		
		break;
		
	default:
	  	break;
	}
	return 0;
}

struct file_operations kyouko2_fops = {
	.open = kyouko2_open,
	.release = kyouko2_release,
	.mmap = kyouko2_mmap,
	.unlocked_ioctl = kyouko2_ioctl,
	.owner = THIS_MODULE
};

struct pci_device_id kyouko2_dev_ids[] = {
	{PCI_DEVICE(PCI_VENDOR_ID_CCORSI, PCI_DEVICE_ID_CCORSI_KYOUKO2)},
	{0}
};

static void kyouko2_remove(struct pci_dev *pci_dev) {
	pci_disable_device(pci_dev);
}

struct pci_driver kyouko2_pci_drv = {
	.name = "kyouko2_driver",
	.id_table = kyouko2_dev_ids,
	.probe = kyouko2_probe,
	.remove = kyouko2_remove
};

struct cdev kyouko2;

static int kyouko2_init(void) {
	  int ret = 0;
	  printk(KERN_ALERT "\nInitializing kyouko2...");
	  
	  cdev_init(&kyouko2, &kyouko2_fops);
	  cdev_add(&kyouko2, MKDEV(MAJOR_NUM,MINOR_NUM), 1);
	  
	  ret = pci_register_driver(&kyouko2_pci_drv);
	  if(ret<0) {
	    printk(KERN_ALERT "\nError: Could not register driver!");
	    return ret;
	  }
	  return 0;
}

static void kyouko2_exit(void) {
	  printk(KERN_ALERT "\nExiting kyouko2...");
	  pci_unregister_driver(&kyouko2_pci_drv);
	  cdev_del(&kyouko2);
}

module_init(kyouko2_init);
module_exit(kyouko2_exit);

