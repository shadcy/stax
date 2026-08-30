#ifndef LWIP_LWIPOPTS_H
#define LWIP_LWIPOPTS_H

#define NO_SYS 1

/* Memory configuration */
#define MEM_ALIGNMENT 4
#define MEM_SIZE (16 * 1024)
#define MEMP_NUM_PBUF 16
#define MEMP_NUM_UDP_PCB 4
#define MEMP_NUM_TCP_PCB 4
#define MEMP_NUM_TCP_PCB_LISTEN 2
#define MEMP_NUM_TCP_SEG 16
#define MEMP_NUM_SYS_TIMEOUT 16
#define MEMP_NUM_NETBUF 8
#define MEMP_NUM_NETCONN 4

/* Pbuf configuration */
#define PBUF_POOL_SIZE 16
#define PBUF_POOL_BUFSIZE 1536

/* IPv4 */
#define LWIP_IPV4 1
#define LWIP_ICMP 1
#define LWIP_IGMP 0
#define LWIP_DHCP 0
#define LWIP_DHCP_DOES_ACD_CHECK 0
#define LWIP_DNS 1

/* Protocols */
#define LWIP_UDP 1
#define LWIP_TCP 1
#define LWIP_RAW 1

/* TCP tuning for HTTP clients (browser/fetch) behind QEMU/NAT */
#define TCP_MSS 1460
#define TCP_WND (4 * TCP_MSS)
#define TCP_SND_BUF (4 * TCP_MSS)
#define TCP_SND_QUEUELEN ((4 * (TCP_SND_BUF) + (TCP_MSS - 1)) / (TCP_MSS))
#define LWIP_TCP_TIMESTAMPS 0
#define TCP_OVERSIZE TCP_MSS
#define TCP_SYNMAXRTX 8
#define TCP_MAXRTX 8

/* Disable APIs requiring OS threading (NO_SYS=1) */
#define LWIP_NETCONN 0
#define LWIP_SOCKET 0

/* System Config — single-threaded poll model (see net_poll). */
#define SYS_LIGHTWEIGHT_PROT 0

/* DNS: avoid RAND_SRC_PORT (needs a real LWIP_RAND). Keep TXID rotation. */
#define LWIP_DNS_SECURE LWIP_DNS_SECURE_RAND_XID
#define DNS_TABLE_SIZE 8
#define DNS_MAX_RETRIES 6
#define DNS_MAX_SERVERS 2

/* Built-in A records so the browser works even if QEMU DNS is flaky. */
#define DNS_LOCAL_HOSTLIST 1
#define DNS_LOCAL_HOSTLIST_INIT \
    { DNS_LOCAL_HOSTLIST_ELEM("httpforever.com", IPADDR4_INIT_BYTES(104, 21, 4, 210)), \
      DNS_LOCAL_HOSTLIST_ELEM("example.com",     IPADDR4_INIT_BYTES(93, 184, 216, 34)), \
      DNS_LOCAL_HOSTLIST_ELEM("wiby.me",         IPADDR4_INIT_BYTES(172, 93, 49, 252)), \
      DNS_LOCAL_HOSTLIST_ELEM("68k.news",        IPADDR4_INIT_BYTES(134, 209, 213, 152)), \
      DNS_LOCAL_HOSTLIST_ELEM("neverssl.com",    IPADDR4_INIT_BYTES(34, 223, 155, 91)), \
      DNS_LOCAL_HOSTLIST_ELEM("pool.ntp.org",    IPADDR4_INIT_BYTES(129, 6, 15, 28)), \
      DNS_LOCAL_HOSTLIST_ELEM("time.google.com", IPADDR4_INIT_BYTES(216, 239, 35, 0)) }

/* SNTP Client Configuration */
#include <stdint.h>
extern void rtc_set_epoch(uint32_t epoch);
#define SNTP_SERVER_DNS 1
#define SNTP_MAX_SERVERS 3
#define SNTP_SERVER_ADDRESS "pool.ntp.org"
#define SNTP_SET_SYSTEM_TIME(sec) rtc_set_epoch((uint32_t)(sec))
#define SNTP_STARTUP_DELAY 0
#define SNTP_CHECK_RESPONSE 0
#define SNTP_UPDATE_DELAY 15000
#define SNTP_RETRY_TIMEOUT 3000

#include "lwip/arch.h"
/* Logging - disable verbose debug spam to keep UART usable */
#define LWIP_DEBUG 1
#define CHECKSUM_CHECK_IP 0
#define CHECKSUM_CHECK_UDP 0
#define CHECKSUM_CHECK_TCP 0
#define CHECKSUM_GEN_IP 1
#define CHECKSUM_GEN_UDP 1
#define CHECKSUM_GEN_TCP 1
#define CHECKSUM_GEN_ICMP 1

/* Per-module debug: all OFF (flip DNS on temporarily when debugging) */
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
