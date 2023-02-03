#include <sys/types.h>
#include <sys/conf.h>
#include <sys/kernel.h>
#include <sys/kthread.h>
#include <sys/module.h>
#include <sys/param.h>
#include <sys/proc.h>
#include <sys/systm.h>

#include <netinet/in.h>
#include <sys/socket.h>
#include <sys/socketvar.h>

#define PORT_NUMBER 20319
#define WAIT_TIME (4 * hz)

static struct proc *main_proc;
static bool main_stop;

static void netapp_udp_main(void *arg) {
	printf("Module process starting...\n");

	int result_code;
	struct socket *master_sock;
	struct sockaddr_in master_addr;

	// Create a new UDP socket
	result_code = socreate(
			PF_INET,
			&master_sock,
			SOCK_DGRAM,
			0,
			// 'curthread' is a built-in pointer to the currently active thread
			curthread->td_ucred,
			curthread
		);
	// If 'socreate()' returned an error code, terminate the process
	if (result_code != 0) {
		printf("Error creating socket: %d.  Terminating...\n", result_code);
		kproc_exit(result_code);
	}

	// Initialize the socket address data structure
	bzero(&master_addr, sizeof(master_addr));
	master_addr.sin_family = AF_INET;
	master_addr.sin_port = htons(PORT_NUMBER);
	master_addr.sin_addr.s_addr = htonl(INADDR_ANY);

	// Bind to the address in 'master_addr'
	result_code = sobind(
			master_sock,
			(struct sockaddr *) &master_addr,
			curthread
		);
	// If 'sobind()' returned an error code, destroy the socket and terminate the process
	if (result_code != 0) {
		printf("Error binding socket: %d.  Terminating...\n", result_code);
		soclose(master_sock);
		kproc_exit(result_code);
	}

	// Wait until the module is unloaded to terminate the process
	while (!main_stop) {
		tsleep(&main_stop, 0, "NOOP", WAIT_TIME);
	}
	printf("Module process terminating...\n");

	// Destroy the socket
	soclose(master_sock);

	kproc_exit(0);
}

static int netapp_udp_handler(struct module *module, int event, void *arg) {
	int return_code = 0;

	switch (event) {
		case MOD_LOAD:
			main_stop = false;
			// Create the module's "main" process
			kproc_create(netapp_udp_main, NULL, &main_proc, 0, 0, "netapp_udp_main");
			uprintf("NetApp UDP module successfully loaded\n");
			break;
		case MOD_UNLOAD:
			main_stop = true;
			uprintf("Waiting for module process to stop...\n");
			// Signal the module's "main" process to stop
			wakeup(&main_stop);
			// Then wait for it if necessary
			tsleep(main_proc, 0, "TWAIT", WAIT_TIME);
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
