#ifndef LWIP_LWIPOPTS_H
#define LWIP_LWIPOPTS_H

#define NO_SYS 1

/* Memory configuration */
#define MEM_ALIGNMENT 4
#define MEM_SIZE 16384
#define MEMP_NUM_PBUF 16
#define MEMP_NUM_UDP_PCB 4
#define MEMP_NUM_TCP_PCB 4
#define MEMP_NUM_TCP_PCB_LISTEN 4
#define MEMP_NUM_TCP_SEG 16
#define MEMP_NUM_SYS_TIMEOUT 8

/* Pbuf configuration */
#define PBUF_POOL_SIZE 16
#define PBUF_POOL_BUFSIZE 1536

/* IPv4 */
#define LWIP_IPV4 1
#define LWIP_ICMP 1
#define LWIP_IGMP 0
#define LWIP_DHCP 1
#define LWIP_DHCP_DOES_ACD_CHECK 0
#define LWIP_DNS 1

/* Protocols */
#define LWIP_UDP 1
#define LWIP_TCP 1
#define LWIP_RAW 1

/* Disable APIs requiring OS threading (NO_SYS=1) */
#define LWIP_NETCONN 0
#define LWIP_SOCKET 0

/* System Config */
#define SYS_LIGHTWEIGHT_PROT 0

/* Logging (can map to T-OS printk if desired) */
#define LWIP_DEBUG 0
#define CHECKSUM_CHECK_IP 0
#define CHECKSUM_CHECK_UDP 0
#define CHECKSUM_CHECK_TCP 0

#define LWIP_PLATFORM_DIAG(x) do { extern void kprintf(const char *, ...); kprintf x; } while(0)
#define LWIP_PLATFORM_ASSERT(x) do { extern void kprintf(const char *, ...); kprintf("Assertion \"%s\" failed at line %d in %s\n", x, __LINE__, __FILE__); } while(0)

/* Enable static ARP entries */
#define ETHARP_SUPPORT_STATIC_ENTRIES 1

#endif /* LWIP_LWIPOPTS_H */
