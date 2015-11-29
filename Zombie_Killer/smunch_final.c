#include<linux/linkage.h>
#include<linux/kernel.h>
#include<linux/syscalls.h>
#include<asm/signal.h>
#include<linux/sched.h>
#include<linux/time.h>
#include<linux/wait.h>

/*This code is to implement sleep_on in Linux 3.18.1*/


DECLARE_WAIT_QUEUE_HEAD(gone);
wait_queue_head_t *q=&gone;
#define wq_write_lock_irq spin_lock_irq //write_lock_irq
#define wq_write_lock_irqsave spin_lock_irqsave //write_lock_irqsave
#define wq_write_unlock_irqrestore spin_unlock_irqrestore //write_unlock_irqrestore
#define wq_write_unlock spin_unlock //write_unlock


#define SLEEP_ON_VAR                            \
        unsigned long flags;                    \
         wait_queue_t wait;                      \
         init_waitqueue_entry(&wait, current);
#define SLEEP_ON_HEAD                                   \
         wq_write_lock_irqsave(&q->lock,flags);          \
         __add_wait_queue(q, &wait);                     \
         wq_write_unlock(&q->lock);
 
#define SLEEP_ON_TAIL                                           \
         wq_write_lock_irq(&q->lock);                            \
         __remove_wait_queue(q, &wait);                          \
         wq_write_unlock_irqrestore(&q->lock,flags);


SYSCALL_DEFINE0(deepsleep)
{	 
SLEEP_ON_VAR

set_current_state(TASK_UNINTERRUPTIBLE);

SLEEP_ON_HEAD
schedule();
SLEEP_ON_TAIL
return 0;
}




/*This is the smunch super killer*/


SYSCALL_DEFINE2(smunch, int, pid, unsigned long, bit_pattern)
{
	int ret=-1;
	unsigned long flags;
	struct task_struct *t;
	rcu_read_lock();
	t=pid_task(find_vpid((pid_t)pid), PIDTYPE_PID);
	rcu_read_unlock();

	lock_task_sighand(t, &flags);
	if(!thread_group_empty(t))
	{
		unlock_task_sighand(t, &flags);
		return ret;
	}

	ret=0;
	if(t->exit_state & EXIT_TRACE)		//check zombie or dead
	{
		printk(KERN_ALERT "Exit state is Zombie!!\n");
		if(bit_pattern & (1ul<< (SIGKILL-1)))		//kill signal on zombie
		{
			sigaddsetmask(&(t->pending.signal), bit_pattern);
			unlock_task_sighand(t, &flags);
			printk(KERN_ALERT "Release...\n");
			release_task(t);
		}
		else {
			printk(KERN_ALERT "Kill bit not set!!\n");
			unlock_task_sighand(t, &flags);
		}
	}
	else if(t->state & TASK_NORMAL)			//sleeping ?
	{
		printk(KERN_ALERT "Trying to Wake up process\n");	
		sigaddsetmask(&(t->pending.signal), bit_pattern);
		set_tsk_thread_flag(t,TIF_SIGPENDING);
		wake_up_process(t);	
		unlock_task_sighand(t, &flags);
	}
	else {									//process is neither zombie nor sleeping
		printk(KERN_ALERT "The process is probably Running!!\n");
		ret = -1;
		unlock_task_sighand(t, &flags);
	}
	return ret;
}

