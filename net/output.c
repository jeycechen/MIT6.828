#include "ns.h"

extern union Nsipc nsipcbuf;

void
output(envid_t ns_envid)
{
	binaryname = "ns_output";

	// LAB 6: Your code here:
	// 	- read a packet from the network server
	//	- send the packet to the device driver
	uint32_t whom;
	int perm;
	int32_t req;
	while(1){
		req = ipc_recv((envid_t *) &whom, &nsipcbuf, &perm);
		if(req != NSREQ_OUTPUT){
			cprintf("not a ns req output\n");
			continue;
		}
			
		struct jif_pkt* packet = &(nsipcbuf.pkt);
		while(sys_send_package(packet->jp_data, packet->jp_len) < 0){ // 如果发送不成功，挂起 循环发送
			sys_yield();
		}
	}
}
