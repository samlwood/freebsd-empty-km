#include <sys/types.h>
#include <sys/conf.h>
#include <sys/kernel.h>
#include <sys/module.h>
#include <sys/param.h>
#include <sys/systm.h>

static int event_handler(struct module *module, int event, void *arg) {
	int return_code = 0;

	switch (event) {
		case MOD_LOAD:
			uprintf("Hello module loaded\n");
			break;
		case MOD_UNLOAD:
			uprintf("Hello module unloaded\n");
			break;
		default:
			return_code = EOPNOTSUPP;
			break;
	}

	return return_code;
}

static moduledata_t hello_data = {
	.name = "hello",
	.evhand = event_handler,
	.priv = NULL
};

DECLARE_MODULE(hello, hello_data, SI_SUB_DRIVERS, SI_ORDER_MIDDLE);
