#ifndef LWIP_LWIPOPTS_H
#define LWIP_LWIPOPTS_H

#define NO_SYS 1

/* Memory configuration */
#define MEM_ALIGNMENT 4
#define MEM_SIZE 32768
#define MEMP_NUM_PBUF 32
#define MEMP_NUM_UDP_PCB 4
#define MEMP_NUM_TCP_PCB 8
#define MEMP_NUM_TCP_PCB_LISTEN 4
#define MEMP_NUM_TCP_SEG 32
#define MEMP_NUM_SYS_TIMEOUT 8

/* Pbuf configuration */
#define PBUF_POOL_SIZE 32
#define PBUF_POOL_BUFSIZE 1536

/* IPv4 */
#define LWIP_IPV4 1
#define LWIP_ICMP 1
#define LWIP_IGMP 0
#define LWIP_DHCP 1
#define LWIP_DHCP_DOES_ACD_CHECK 0
#define LWIP_DNS 1

/* Protocols */
#define LWIP_ARP 1
#define ARP_TABLE_SIZE 10
#define ARP_QUEUEING 1
#define LWIP_UDP 1
#define LWIP_TCP 1
#define LWIP_RAW 1

/* Disable APIs requiring OS threading (NO_SYS=1) */
#define LWIP_NETCONN 0
#define LWIP_SOCKET 0

/* System Config */
#define SYS_LIGHTWEIGHT_PROT 0

#include "lwip/arch.h"
/* Logging - disable verbose debug spam to keep UART usable */
#define LWIP_DEBUG 0
#define CHECKSUM_CHECK_IP 0
#define CHECKSUM_CHECK_UDP 0
#define CHECKSUM_CHECK_TCP 0

/* Per-module debug: all OFF */
#define ETHARP_DEBUG      LWIP_DBG_OFF
#define NETIF_DEBUG       LWIP_DBG_OFF
#define PBUF_DEBUG        LWIP_DBG_OFF
#define API_LIB_DEBUG     LWIP_DBG_OFF
#define API_MSG_DEBUG     LWIP_DBG_OFF
#define SOCKETS_DEBUG     LWIP_DBG_OFF
#define ICMP_DEBUG        LWIP_DBG_OFF
#define IGMP_DEBUG        LWIP_DBG_OFF
#define INET_DEBUG        LWIP_DBG_OFF
#define IP_DEBUG          LWIP_DBG_OFF
#define IP_REASS_DEBUG    LWIP_DBG_OFF
#define RAW_DEBUG         LWIP_DBG_OFF
#define MEM_DEBUG         LWIP_DBG_OFF
#define MEMP_DEBUG        LWIP_DBG_OFF
#define SYS_DEBUG         LWIP_DBG_OFF
#define TIMERS_DEBUG      LWIP_DBG_OFF
#define TCP_DEBUG         LWIP_DBG_OFF
#define TCP_INPUT_DEBUG   LWIP_DBG_OFF
#define TCP_FR_DEBUG      LWIP_DBG_OFF
#define TCP_RTO_DEBUG     LWIP_DBG_OFF
#define TCP_CWND_DEBUG    LWIP_DBG_OFF
#define TCP_WND_DEBUG     LWIP_DBG_OFF
#define TCP_OUTPUT_DEBUG  LWIP_DBG_OFF
#define TCP_RST_DEBUG     LWIP_DBG_OFF
#define TCP_QLEN_DEBUG    LWIP_DBG_OFF
#define UDP_DEBUG         LWIP_DBG_OFF
#define TCPIP_DEBUG       LWIP_DBG_OFF
#define SLIP_DEBUG        LWIP_DBG_OFF
#define DHCP_DEBUG        LWIP_DBG_OFF
#define AUTOIP_DEBUG      LWIP_DBG_OFF
#define DNS_DEBUG         LWIP_DBG_OFF
#define IP6_DEBUG         LWIP_DBG_OFF

#undef LWIP_PLATFORM_DIAG
#define LWIP_PLATFORM_DIAG(x) do { extern void kprintf(const char *, ...); kprintf x; } while(0)
#undef LWIP_PLATFORM_ASSERT
#define LWIP_PLATFORM_ASSERT(x) do { extern void kprintf(const char *, ...); kprintf("ASSERT: \"%s\" at %s:%d\n", x, __FILE__, __LINE__); } while(0)

/* Enable static ARP entries */
#define ETHARP_SUPPORT_STATIC_ENTRIES 1

#endif /* LWIP_LWIPOPTS_H */
