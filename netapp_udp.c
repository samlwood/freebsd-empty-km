#include <sys/types.h>
#include <sys/conf.h>
#include <sys/kernel.h>
#include <sys/kthread.h>
#include <sys/module.h>
#include <sys/param.h>
#include <sys/systm.h>

/*
 * This pointer will be modified to point to the location in memory (not
 * managed by this program) where the information about the module's main
 * process is stored.
 */
struct proc *main_proc;
/*
 * This boolean flag indicates to the main process when to terminate.
 */
bool main_stop;

/*
 * This function can be thought of as the "main()" of the kernel module.
 * Everything it does should happen in here.
 */
static void netapp_udp_main(void *arg) {
	printf("Module process starting...\n");
	/*
	 * This loop is doing two different things with the 'main_stop' flag:
	 *
	 * 1. It is checking at once every four seconds to see if it is set to
	 *    "true".  If it is, the process exits; if it is not, it waits at most
	 *    another four seconds.
	 * 2. It is sleeping on the *address* of 'main_stop'.  The way the
	 *    'tsleep()' function works is that its first parameter is an arbitrary
	 *    pointer that doesn't even necessarily have to point to anything.  This
	 *    pointer serves as a "channel number" that another part of the program
	 *    can call 'wakeup()' on to start the process running again.  What that
	 *    means here is that when the event handler (discussed below) calls
	 *    'wakeup()' on 'main_stop', this process immediately starts up again,
	 *    without having to wait the full four seconds.
	 *
	 * It is necessary that this loop calls some kind of sleep function.
	 * Otherwise, the operating system will hang.
	 */
	while (!main_stop) {
		tsleep(&main_stop, 0, "WAIT", 4 * hz);
	}
	printf("Module process terminating...\n");
	kproc_exit(0);
}

/*
 * This function, at the very least, is called when the module is loaded and
 * unloaded.  This is *not* analagous to the 'main()' function of the module,
 * since it *does not* spawn a persistent process.  This should be used for
 * housekeeping only, not actual logic.
 */
static int netapp_udp_handler(struct module *module, int event, void *arg) {
	int return_code = 0;

	switch (event) {
		case MOD_LOAD:
			main_stop = false;
			/*
			 * This creates the process where the kernel module's actual
			 * operations will live.  The first parameter is the name of the
			 * function to call, the second is a void pointer that will be
			 * passed to it as its only argument.  The third is a pointer to the
			 * main process pointer discussed at the top of this file.  The
			 * fourth is used for flags, which we don't have to worry about.
			 * The fifth is the size of the program's stack; zero gives it the
			 * default size.  The last appears to just be a description.
			 */
			kproc_create(netapp_udp_main, NULL, &main_proc, 0, 0, "netapp_udp_main");
			uprintf("NetApp UDP module successfully loaded\n");
			break;
		case MOD_UNLOAD:
			main_stop = true;
			uprintf("Waiting for module process to stop...\n");
			/*
			 * This is the call to 'wakeup()' referenced above in
			 * 'netapp_udp_main()'.
			 */
			wakeup(&main_stop);
			/*
			 * It is very important that we do not finish unloading the module
			 * until *after* its process has terminated.  If we do, the
			 * operating system will crash.  This uses the same 'tsleep()'
			 * function as described above, except the "channel" it listens on
			 * is the pointer to the main process struct.  You'll notice that
			 * 'netapp_udp_main()' does not contain a call to 'wakeup()' on this
			 * channel.  But, as the kproc(9) discusses, the 'kproc_exit()'
			 * function that 'netapp_udp_main()' uses to terminate itself *does*
			 * call 'wakeup()' on this pointer.
			 */
			tsleep(main_proc, 0, "WAIT", 4 * hz);
			uprintf("Unloading NetApp UDP module...\n");
			break;
		default:
			return_code = EOPNOTSUPP;
			break;
	}

	return return_code;
}

static moduledata_t netapp_udp_data = {
	.name = "netapp_udp",
	.evhand = netapp_udp_handler,
	.priv = NULL
};

DECLARE_MODULE(netapp_udp, netapp_udp_data, SI_SUB_DRIVERS, SI_ORDER_MIDDLE);
