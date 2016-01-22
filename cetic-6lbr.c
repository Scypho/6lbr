/*
 * Copyright (c) 2013, CETIC.
 * All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 * 3. Neither the name of the Institute nor the names of its contributors
 *    may be used to endorse or promote products derived from this software
 *    without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE INSTITUTE AND CONTRIBUTORS ``AS IS'' AND
 * ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
 * ARE DISCLAIMED.  IN NO EVENT SHALL THE INSTITUTE OR CONTRIBUTORS BE LIABLE
 * FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
 * DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS
 * OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION)
 * HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT
 * LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY
 * OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF
 * SUCH DAMAGE.
 */

/**
 * \file
 *         Main 6LBR process and initialisation
 * \author
 *         6LBR Team <6lbr@cetic.be>
 */

#define LOG6LBR_MODULE "6LBR"

#include "contiki.h"
#include "contiki-lib.h"
#include "contiki-net.h"
#include "net/ip/uip.h"
#include "net/ipv6/uip-nd6.h"
#include "net/rpl/rpl.h"
#include "net/netstack.h"
#include "net/rpl/rpl.h"

#include <stdio.h>
#include <string.h>
#include <ctype.h>
#include <stdlib.h>

#include "log-6lbr.h"

#include "cetic-6lbr.h"
#include "platform-init.h"
#include "packet-filter.h"
#include "eth-drv.h"
#include "nvm-config.h"
#include "rio.h"
#include "6lbr-hooks.h"

#if CETIC_6LBR_IP64
#include "ip64.h"
#include "ip64-ipv4-dhcp.h"
#include "ip64-eth.h"
#endif

#if CETIC_6LBR_LLSEC_WRAPPER
#include "llsec-wrapper.h"
#endif

#if WEBSERVER
#include "webserver.h"
#endif

#if CETIC_NODE_INFO
#include "node-info.h"
#endif

#if CETIC_NODE_CONFIG
#include "node-config.h"
#endif

#if WITH_COAPSERVER
#include "coap-server.h"
#endif

#if WITH_NVM_PROXY
#include "nvm-proxy.h"
#endif

#if CONTIKI_TARGET_NATIVE
#include "plugin.h"
#include "6lbr-watchdog.h"
#include "slip-config.h"
#include <arpa/inet.h>
#endif
#include "watchdog.h"

//Initialisation flags
int ethernet_ready = 0;
int eth_mac_addr_ready = 0;
int radio_ready = 0;
int radio_mac_addr_ready = 0;

//WSN
uip_lladdr_t wsn_mac_addr;
uip_ip6addr_t wsn_net_prefix;
uint8_t wsn_net_prefix_len;
uip_ipaddr_t wsn_ip_addr;
uip_ipaddr_t wsn_ip_local_addr;
rpl_dag_t *cetic_dag;

// Eth
ethaddr_t eth_mac_addr;
uip_lladdr_t eth_mac64_addr;
uip_ipaddr_t eth_ip_addr;
uip_ipaddr_t eth_net_prefix;
uip_ipaddr_t eth_ip_local_addr;
uip_ipaddr_t eth_dft_router;
uip_ip4addr_t eth_ip64_addr;
uip_ip4addr_t eth_ip64_netmask;
uip_ip4addr_t eth_ip64_gateway;

//Misc
unsigned long cetic_6lbr_startup;
static int security_ready = 0;

enum cetic_6lbr_restart_type_t cetic_6lbr_restart_type;

//Hooks
cetic_6lbr_allowed_node_hook_t cetic_6lbr_allowed_node_hook = cetic_6lbr_allowed_node_default_hook;

/*---------------------------------------------------------------------------*/
PROCESS_NAME(udp_server_process);
PROCESS_NAME(udp_client_process);

process_event_t cetic_6lbr_restart_event;
process_event_t cetic_6lbr_reload_event;

PROCESS(cetic_6lbr_process, "CETIC Bridge process");

AUTOSTART_PROCESSES(&cetic_6lbr_process);

/*---------------------------------------------------------------------------*/

#if CONTIKI_TARGET_NATIVE
static void
cetic_6lbr_save_ip(void)
{
  if (ip_config_file_name) {
    char str[INET6_ADDRSTRLEN];
#if CETIC_6LBR_SMARTBRIDGE
    inet_ntop(AF_INET6, (struct sockaddr_in6 *)&wsn_ip_addr, str, INET6_ADDRSTRLEN);
#else
    inet_ntop(AF_INET6, (struct sockaddr_in6 *)&eth_ip_addr, str, INET6_ADDRSTRLEN);
#endif
    FILE *ip_config_file = fopen(ip_config_file_name, "w");
    fprintf(ip_config_file, "%s\n", str);
    fclose(ip_config_file);
  }
}
#endif

void
cetic_6lbr_set_prefix(uip_ipaddr_t * prefix, unsigned len,
                      uip_ipaddr_t * ipaddr)
{
#if CETIC_6LBR_SMARTBRIDGE
  int new_prefix = !uip_ipaddr_prefixcmp(&wsn_net_prefix, prefix, len);
  int new_dag_prefix = cetic_dag != NULL && !uip_ipaddr_prefixcmp(&cetic_dag->prefix_info.prefix, prefix, len);
  if((nvm_data.mode & CETIC_MODE_WAIT_RA_MASK) == 0) {
    LOG6LBR_DEBUG("Ignoring RA\n");
    return;
  }

  if(new_prefix) {
    LOG6LBR_6ADDR(INFO, prefix, "Setting prefix : ");
    uip_ipaddr_copy(&wsn_ip_addr, ipaddr);
    uip_ipaddr_copy(&wsn_net_prefix, prefix);
    wsn_net_prefix_len = len;
    LOG6LBR_6ADDR(INFO, &wsn_ip_addr, "Tentative global IPv6 address : ");
#if CONTIKI_TARGET_NATIVE
  cetic_6lbr_save_ip();
#endif
  }
  if(new_dag_prefix) {
    rpl_set_prefix(cetic_dag, prefix, len);
    LOG6LBR_6ADDR(INFO, prefix, "Setting DAG prefix : ");
    rpl_repair_root(RPL_DEFAULT_INSTANCE);
  }
#endif
}

int cetic_6lbr_allowed_node_default_hook(rpl_dag_t *dag, uip_ipaddr_t *prefix, int prefix_len)
{
  return 1;
}

void
cetic_6lbr_init(void)
{
  uip_ds6_addr_t *local = uip_ds6_get_link_local(-1);

  uip_ipaddr_copy(&wsn_ip_local_addr, &local->ipaddr);

  LOG6LBR_6ADDR(INFO, &wsn_ip_local_addr, "Tentative local IPv6 address ");

#if CETIC_6LBR_SMARTBRIDGE

  if((nvm_data.mode & CETIC_MODE_WAIT_RA_MASK) == 0)    //Manual configuration
  {
    memcpy(wsn_net_prefix.u8, &nvm_data.wsn_net_prefix,
           sizeof(nvm_data.wsn_net_prefix));
    wsn_net_prefix_len = nvm_data.wsn_net_prefix_len;
    if((nvm_data.mode & CETIC_MODE_WSN_AUTOCONF) != 0)  //Address auto configuration
    {
      uip_ipaddr_copy(&wsn_ip_addr, &wsn_net_prefix);
      uip_ds6_set_addr_iid(&wsn_ip_addr, &uip_lladdr);
      uip_ds6_addr_add(&wsn_ip_addr, 0, ADDR_AUTOCONF);
    } else {
      memcpy(wsn_ip_addr.u8, &nvm_data.wsn_ip_addr,
             sizeof(nvm_data.wsn_ip_addr));
      uip_ds6_addr_add(&wsn_ip_addr, 0, ADDR_MANUAL);
    }
    LOG6LBR_6ADDR(INFO, &wsn_ip_addr, "Tentative global IPv6 address ");
    memcpy(eth_dft_router.u8, &nvm_data.eth_dft_router,
           sizeof(nvm_data.eth_dft_router));
    if ( !uip_is_addr_unspecified(&eth_dft_router) ) {
      uip_ds6_defrt_add(&eth_dft_router, 0);
    }
  } else {                            //End manual configuration
    uip_create_unspecified(&wsn_net_prefix);
    wsn_net_prefix_len = 0;
    uip_create_unspecified(&wsn_ip_addr);
  }
#endif

#if CETIC_6LBR_ROUTER
  //WSN network configuration
  memcpy(wsn_net_prefix.u8, &nvm_data.wsn_net_prefix,
         sizeof(nvm_data.wsn_net_prefix));
  wsn_net_prefix_len = nvm_data.wsn_net_prefix_len;
  if((nvm_data.mode & CETIC_MODE_WSN_AUTOCONF) != 0)    //Address auto configuration
  {
    uip_ipaddr_copy(&wsn_ip_addr, &wsn_net_prefix);
    uip_ds6_set_addr_iid(&wsn_ip_addr, &uip_lladdr);
    uip_ds6_addr_add(&wsn_ip_addr, 0, ADDR_AUTOCONF);
  } else {
    memcpy(wsn_ip_addr.u8, &nvm_data.wsn_ip_addr,
           sizeof(nvm_data.wsn_ip_addr));
    uip_ds6_addr_add(&wsn_ip_addr, 0, ADDR_MANUAL);
  }
  LOG6LBR_6ADDR(INFO, &wsn_ip_addr, "Tentative global IPv6 address (WSN) ");

  //Ethernet network configuration
  memcpy(eth_net_prefix.u8, &nvm_data.eth_net_prefix,
         sizeof(nvm_data.eth_net_prefix));
  memcpy(eth_dft_router.u8, &nvm_data.eth_dft_router,
         sizeof(nvm_data.eth_dft_router));

  if ( !uip_is_addr_unspecified(&eth_dft_router) ) {
    uip_ds6_defrt_add(&eth_dft_router, 0);
  }

  eth_mac64_addr.addr[0] = eth_mac_addr[0];
  eth_mac64_addr.addr[1] = eth_mac_addr[1];
  eth_mac64_addr.addr[2] = eth_mac_addr[2];
  eth_mac64_addr.addr[3] = CETIC_6LBR_ETH_EXT_A;
  eth_mac64_addr.addr[4] = CETIC_6LBR_ETH_EXT_B;
  eth_mac64_addr.addr[5] = eth_mac_addr[3];
  eth_mac64_addr.addr[6] = eth_mac_addr[4];
  eth_mac64_addr.addr[7] = eth_mac_addr[5];

  if((nvm_data.mode & CETIC_MODE_ETH_AUTOCONF) != 0)    //Address auto configuration
  {
    uip_ipaddr_copy(&eth_ip_addr, &eth_net_prefix);
    uip_ds6_set_addr_iid(&eth_ip_addr, &eth_mac64_addr);
    uip_ds6_addr_add(&eth_ip_addr, 0, ADDR_AUTOCONF);
  } else {
    memcpy(eth_ip_addr.u8, &nvm_data.eth_ip_addr,
           sizeof(nvm_data.eth_ip_addr));
    uip_ds6_addr_add(&eth_ip_addr, 0, ADDR_MANUAL);
  }
  LOG6LBR_6ADDR(INFO, &eth_ip_addr, "Tentative global IPv6 address (ETH) ");

  //Ugly hack : in order to set WSN local address as the default address
  //We must add it afterwards as uip_ds6_addr_add allocates addr from the end of the list
  uip_ds6_addr_rm(local);

  uip_create_linklocal_prefix(&eth_ip_local_addr);
  uip_ds6_set_addr_iid(&eth_ip_local_addr, &eth_mac64_addr);
  uip_ds6_addr_add(&eth_ip_local_addr, 0, ADDR_AUTOCONF);

  uip_ds6_addr_add(&wsn_ip_local_addr, 0, ADDR_AUTOCONF);

  //Prefix and RA configuration
#if UIP_CONF_IPV6_RPL
  uint8_t publish = (nvm_data.ra_prefix_flags & CETIC_6LBR_MODE_SEND_PIO) != 0;
  uip_ds6_prefix_add(&eth_net_prefix, nvm_data.eth_net_prefix_len, publish,
                     nvm_data.ra_prefix_flags,
                     nvm_data.ra_prefix_vtime, nvm_data.ra_prefix_ptime);
#else
  uip_ds6_prefix_add(&eth_net_prefix, nvm_data.eth_net_prefix_len, 0, 0, 0, 0);
  uint8_t publish = (nvm_data.ra_prefix_flags & CETIC_6LBR_MODE_SEND_PIO) != 0;
  uip_ds6_prefix_add(&wsn_net_prefix, nvm_data.wsn_net_prefix_len, publish,
		             nvm_data.ra_prefix_flags,
		             nvm_data.ra_prefix_vtime, nvm_data.ra_prefix_ptime);
#endif

#if UIP_CONF_IPV6_RPL
  if ((nvm_data.ra_rio_flags & CETIC_6LBR_MODE_SEND_RIO) != 0 ) {
    uip_ds6_route_info_add(&wsn_net_prefix, nvm_data.wsn_net_prefix_len, nvm_data.ra_rio_flags, nvm_data.ra_rio_lifetime);
  }
#endif
  if ((nvm_data.mode & CETIC_MODE_ROUTER_RA_DAEMON) != 0 ) {
    LOG6LBR_INFO("RA Daemon enabled\n");
  } else {
    LOG6LBR_INFO("RA Daemon disabled\n");
  }
#endif
}

void
cetic_6lbr_init_finalize(void)
{
#if UIP_CONF_IPV6_RPL && CETIC_6LBR_DODAG_ROOT
  if((nvm_data.rpl_config & CETIC_6LBR_MODE_MANUAL_DODAG) != 0) {
    //Manual DODAG ID
    cetic_dag = rpl_set_root(nvm_data.rpl_instance_id, (uip_ipaddr_t*)&nvm_data.rpl_dodag_id);
  } else {
    //Automatic DODAG ID
    if((nvm_data.rpl_config & CETIC_6LBR_MODE_GLOBAL_DODAG) != 0) {
      //DODAGID = global address used !
      cetic_dag = rpl_set_root(nvm_data.rpl_instance_id, &wsn_ip_addr);
    } else {
      //DODAGID = link-local address used !
      cetic_dag = rpl_set_root(nvm_data.rpl_instance_id, &wsn_ip_local_addr);
    }
  }
#if CETIC_6LBR_SMARTBRIDGE
  if((nvm_data.mode & CETIC_MODE_WAIT_RA_MASK) == 0) {
    rpl_set_prefix(cetic_dag, &wsn_net_prefix, nvm_data.wsn_net_prefix_len);
  }
#else
  rpl_set_prefix(cetic_dag, &wsn_net_prefix, nvm_data.wsn_net_prefix_len);
#endif
  LOG6LBR_6ADDR(INFO, &cetic_dag->dag_id, "Configured as DODAG Root ");
#endif

#if CETIC_6LBR_IP64
  if((nvm_data.global_flags & CETIC_GLOBAL_IP64) != 0) {
    LOG6LBR_INFO("Starting IP64\n");
    ip64_eth_addr_set((struct ip64_eth_addr *)eth_mac_addr);
    ip64_init();
    if((nvm_data.eth_ip64_flags & CETIC_6LBR_IP64_DHCP) == 0) {
      memcpy(&eth_ip64_addr, nvm_data.eth_ip64_addr, sizeof(nvm_data.eth_ip64_addr));
      memcpy(&eth_ip64_netmask, nvm_data.eth_ip64_netmask, sizeof(nvm_data.eth_ip64_netmask));
      memcpy(&eth_ip64_gateway, nvm_data.eth_ip64_gateway, sizeof(nvm_data.eth_ip64_gateway));
      ip64_set_ipv4_address(&eth_ip64_addr, &eth_ip64_netmask);
      ip64_set_draddr(&eth_ip64_gateway);
    } else {
      ip64_ipv4_dhcp_init();
    }
  }
#endif

#if RESOLV_CONF_SUPPORTS_MDNS
  if((nvm_data.global_flags & CETIC_GLOBAL_MDNS) != 0) {
    LOG6LBR_INFO("Starting MDNS\n");
    process_start(&resolv_process, NULL);
    resolv_set_hostname((char *)nvm_data.dns_host_name);
#if RESOLV_CONF_SUPPORTS_DNS_SD
    if((nvm_data.dns_flags & CETIC_6LBR_DNS_DNS_SD) != 0) {
      resolv_add_service("_6lbr._tcp", "", 80);
    }
#endif
  }
#endif

#if CETIC_6LBR_TRANSPARENTBRIDGE
#if CETIC_6LBR_LEARN_RPL_MAC
  LOG6LBR_INFO("Starting as RPL Relay\n");
#else
  LOG6LBR_INFO("Starting as Full TRANSPARENT-BRIDGE\n");
#endif
#elif CETIC_6LBR_SMARTBRIDGE
  LOG6LBR_INFO("Starting as SMART-BRIDGE\n");
#elif CETIC_6LBR_ROUTER
#if UIP_CONF_IPV6_RPL
  LOG6LBR_INFO("Starting as RPL ROUTER\n");
#else
  LOG6LBR_INFO("Starting as NDP ROUTER\n");
#endif
#elif CETIC_6LBR_6LR
  LOG6LBR_INFO("Starting as 6LR\n");
#else
  LOG6LBR_INFO("Starting in UNKNOWN mode\n");
#endif

#if CONTIKI_TARGET_NATIVE
  cetic_6lbr_save_ip();
#endif
}

/*---------------------------------------------------------------------------*/
static void llsec_bootstrap_cb(void)
{
  security_ready = 1;
  LOG6LBR_INFO("Security layer initialized\n");
}
/*---------------------------------------------------------------------------*/

PROCESS_THREAD(cetic_6lbr_process, ev, data)
{
  static struct etimer timer;
  static int addr_number;
  PROCESS_BEGIN();

  cetic_6lbr_restart_event = process_alloc_event();
  cetic_6lbr_reload_event = process_alloc_event();
  cetic_6lbr_startup = clock_seconds();

#if CONTIKI_TARGET_NATIVE
  slip_config_handle_arguments(contiki_argc, contiki_argv);
  if (watchdog_interval) {
    process_start(&native_6lbr_watchdog, NULL);
  } else {
    LOG6LBR_WARN("6LBR Watchdog disabled\n");
  }
#endif

  LOG6LBR_INFO("Starting 6LBR version " CETIC_6LBR_VERSION " (" CONTIKI_VERSION_STRING ")\n");

  load_nvm_config();

  platform_init();

#if !CETIC_6LBR_ONE_ITF
  platform_radio_init();
  while(!radio_ready) {
    PROCESS_PAUSE();
  }
#endif

  eth_drv_init();

  while(!ethernet_ready) {
    PROCESS_PAUSE();
  }
#if CETIC_6LBR_LLSEC_WRAPPER
  llsec_wrapper_init();
#endif
  NETSTACK_LLSEC.bootstrap(llsec_bootstrap_cb);
  while(!security_ready) {
    PROCESS_PAUSE();
  }

  //clean up any early packet
  uip_len = 0;
  process_start(&tcpip_process, NULL);

  PROCESS_PAUSE();

#if CETIC_NODE_INFO
  node_info_init();
#endif

  packet_filter_init();

  cetic_6lbr_init();

  //Wait result of DAD on 6LBR addresses
  LOG6LBR_INFO("Checking addresses duplication\n");
  addr_number = uip_ds6_get_addr_number(-1);
  etimer_set(&timer, CLOCK_SECOND);
  while(uip_ds6_get_addr_number(ADDR_TENTATIVE) > 0) {
    PROCESS_WAIT_EVENT_UNTIL(ev == PROCESS_EVENT_TIMER);
    etimer_set(&timer, CLOCK_SECOND);
  }
  if(uip_ds6_get_addr_number(-1) != addr_number) {
    LOG6LBR_FATAL("Addresses duplication failed");
    watchdog_reboot();
  }
  cetic_6lbr_init_finalize();

#if CETIC_NODE_CONFIG
  node_config_init();
#endif

#if WEBSERVER
  webserver_init();
#endif
#if UDPSERVER
  process_start(&udp_server_process, NULL);
#endif
#if UDPCLIENT
  process_start(&udp_client_process, NULL);
#endif
#if WITH_COAPSERVER
  coap_server_init();
#endif

#if WITH_NVM_PROXY
  nvm_proxy_init();
#endif

#if CONTIKI_TARGET_NATIVE
  plugins_load();
#endif

  LOG6LBR_INFO("CETIC 6LBR Started\n");

  PROCESS_WAIT_EVENT_UNTIL(ev == cetic_6lbr_restart_event);
  etimer_set(&timer, CLOCK_SECOND);
  PROCESS_WAIT_EVENT_UNTIL(ev == PROCESS_EVENT_TIMER);
#if CONTIKI_TARGET_NATIVE
  switch (cetic_6lbr_restart_type) {
    case CETIC_6LBR_RESTART:
      LOG6LBR_INFO("Exiting...\n");
      exit(0);
      break;
    case CETIC_6LBR_REBOOT:
      LOG6LBR_INFO("Rebooting...\n");
      system("reboot");
      break;
    case CETIC_6LBR_HALT:
      LOG6LBR_INFO("Halting...\n");
      system("halt");
      break;
    default:
      //We should never end up here...
      exit(1);
  }
  //We should never end up here...
  exit(1);
#else
  LOG6LBR_INFO("Rebooting...\n");
  watchdog_reboot();
#endif

  PROCESS_END();
}
