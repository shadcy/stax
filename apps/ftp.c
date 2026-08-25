#include "lwip/tcp.h"
#include "lwip/ip_addr.h"
#include "lwip/ip4_addr.h"
#include "../include/command.h"
#include "../include/console.h"
#include "fatfs/ff.h"
#include "../include/string.h"

#define FTP_CTRL_PORT 2121
#define FTP_LINE_MAX 160
#define FTP_PATH_MAX 96

extern int net_poll(void);

typedef enum {
    FTP_DATA_IDLE = 0,
    FTP_DATA_STOR,
    FTP_DATA_LIST
} ftp_data_mode_t;

static struct tcp_pcb *ftp_listen_pcb;
static struct tcp_pcb *ftp_ctrl_pcb;
static struct tcp_pcb *ftp_data_pcb;
static ftp_data_mode_t ftp_data_mode = FTP_DATA_IDLE;
static ip_addr_t ftp_active_ip;
static u16_t ftp_active_port;
static int ftp_active_ready;
static char ftp_line[FTP_LINE_MAX];
static int ftp_line_len;
static char ftp_store_path[FTP_PATH_MAX];
static FIL ftp_file;
static int ftp_file_open;
static int ftp_store_received;
static int ftp_store_idle_polls;

static void ftp_close_data(void);

static void ftp_send(const char *s) {
    if (ftp_ctrl_pcb) {
        tcp_write(ftp_ctrl_pcb, s, strlen(s), TCP_WRITE_FLAG_COPY);
        tcp_output(ftp_ctrl_pcb);
    }
}

static void ftp_sendf_path(const char *prefix, const char *path, const char *suffix) {
    ftp_send(prefix);
    ftp_send(path);
    ftp_send(suffix);
}

static void ftp_upper(char *s) {
    while (*s) {
        if (*s >= 'a' && *s <= 'z')
            *s = (char)(*s - 'a' + 'A');
        s++;
    }
}

static int ftp_starts_with(const char *s, const char *prefix) {
    return strncmp(s, prefix, strlen(prefix)) == 0;
}

static char *ftp_arg(char *line) {
    while (*line && *line != ' ')
        line++;
    while (*line == ' ')
        line++;
    return line;
}

static void ftp_copy_path(char *dst, const char *src) {
    int i = 0;
    while (src[i] && i < FTP_PATH_MAX - 1) {
        dst[i] = src[i];
        i++;
    }
    dst[i] = '\0';
}

static void ftp_finish_store(int ok) {
    if (ftp_file_open) {
        f_close(&ftp_file);
        ftp_file_open = 0;
    }
    ftp_data_mode = FTP_DATA_IDLE;
    ftp_close_data();
    ftp_send(ok ? "226 Transfer complete.\r\n" : "451 Write failed.\r\n");
}

static err_t ftp_data_recv(void *arg, struct tcp_pcb *tpcb, struct pbuf *p, err_t err) {
    (void)arg;
    (void)err;
    if (p == NULL) {
        if (ftp_data_mode == FTP_DATA_STOR) {
            ftp_finish_store(1);
        } else {
            ftp_close_data();
        }
        return ERR_OK;
    }

    if (ftp_data_mode == FTP_DATA_STOR && ftp_file_open) {
        ftp_store_received = 1;
        ftp_store_idle_polls = 0;
        for (struct pbuf *q = p; q != NULL; q = q->next) {
            UINT bw = 0;
            FRESULT res = f_write(&ftp_file, q->payload, q->len, &bw);
            if (res != FR_OK || bw != q->len) {
                pbuf_free(p);
                ftp_finish_store(0);
                return ERR_OK;
            }
        }
    }
    tcp_recved(tpcb, p->tot_len);
    pbuf_free(p);
    return ERR_OK;
}

static err_t ftp_data_poll(void *arg, struct tcp_pcb *tpcb) {
    (void)arg;
    (void)tpcb;
    if (ftp_data_mode == FTP_DATA_STOR && ftp_store_received) {
        ftp_store_idle_polls++;
        if (ftp_store_idle_polls >= 4)
            ftp_finish_store(1);
    }
    return ERR_OK;
}

static void ftp_data_err(void *arg, err_t err) {
    (void)arg;
    (void)err;
    ftp_data_pcb = NULL;
    if (ftp_data_mode == FTP_DATA_STOR)
        ftp_finish_store(1);
}

static void ftp_close_data(void) {
    if (ftp_data_pcb) {
        tcp_arg(ftp_data_pcb, NULL);
        tcp_recv(ftp_data_pcb, NULL);
        tcp_err(ftp_data_pcb, NULL);
        tcp_close(ftp_data_pcb);
        ftp_data_pcb = NULL;
    }
}

static void ftp_write_data(const char *s) {
    if (ftp_data_pcb)
        tcp_write(ftp_data_pcb, s, strlen(s), TCP_WRITE_FLAG_COPY);
}

static void ftp_list_now(void) {
    if (!ftp_data_pcb) {
        ftp_send("425 Use PASV first.\r\n");
        return;
    }

    DIR dir;
    FILINFO fno;
    FRESULT res = f_opendir(&dir, ".");
    if (res != FR_OK) {
        ftp_send("550 Failed to open directory.\r\n");
        ftp_close_data();
        return;
    }

    ftp_send("150 Opening data connection.\r\n");
    while (f_readdir(&dir, &fno) == FR_OK && fno.fname[0] != 0) {
        char line[160];
        const char *prefix = (fno.fattrib & AM_DIR) ? "drwxr-xr-x 1 tios tios " : "-rw-r--r-- 1 tios tios ";
        char sizebuf[16];
        int sz = 0;
        unsigned int n = (unsigned int)fno.fsize;
        if (n == 0) {
            sizebuf[sz++] = '0';
        } else {
            char tmp[16];
            int ti = 0;
            while (n > 0 && ti < (int)sizeof(tmp)) {
                tmp[ti++] = (char)('0' + (n % 10));
                n /= 10;
            }
            while (ti > 0)
                sizebuf[sz++] = tmp[--ti];
        }
        sizebuf[sz] = '\0';

        int pos = 0;
        const char *p = prefix;
        while (*p && pos < (int)sizeof(line) - 1) line[pos++] = *p++;
        p = sizebuf;
        while (*p && pos < (int)sizeof(line) - 1) line[pos++] = *p++;
        p = " Jan 01 00:00 ";
        while (*p && pos < (int)sizeof(line) - 1) line[pos++] = *p++;
        p = fno.fname;
        while (*p && pos < (int)sizeof(line) - 3) line[pos++] = *p++;
        line[pos++] = '\r';
        line[pos++] = '\n';
        line[pos] = '\0';
        ftp_write_data(line);
    }
    f_closedir(&dir);
    tcp_output(ftp_data_pcb);
    ftp_close_data();
    ftp_data_mode = FTP_DATA_IDLE;
    ftp_send("226 Transfer complete.\r\n");
}

static err_t ftp_data_connected(void *arg, struct tcp_pcb *tpcb, err_t err) {
    (void)arg;
    if (err != ERR_OK) {
        ftp_send("425 Cannot open data connection.\r\n");
        ftp_finish_store(0);
        return err;
    }
    ftp_data_pcb = tpcb;
    tcp_recv(ftp_data_pcb, ftp_data_recv);
    tcp_poll(ftp_data_pcb, ftp_data_poll, 2);
    tcp_err(ftp_data_pcb, ftp_data_err);
    if (ftp_data_mode == FTP_DATA_LIST) {
        ftp_list_now();
    } else {
        ftp_send("150 Data connection open.\r\n");
    }
    return ERR_OK;
}

static int ftp_open_active_data(void) {
    if (!ftp_active_ready)
        return -1;
    ftp_close_data();
    ftp_data_pcb = tcp_new();
    if (!ftp_data_pcb)
        return -1;
    tcp_err(ftp_data_pcb, ftp_data_err);
    if (tcp_connect(ftp_data_pcb, &ftp_active_ip, ftp_active_port, ftp_data_connected) != ERR_OK) {
        tcp_close(ftp_data_pcb);
        ftp_data_pcb = NULL;
        return -1;
    }
    return 0;
}

static void ftp_start_store(const char *path) {
    if (path == NULL || path[0] == '\0') {
        ftp_send("501 Missing filename.\r\n");
        return;
    }
    if (ftp_file_open) {
        f_close(&ftp_file);
        ftp_file_open = 0;
    }
    ftp_copy_path(ftp_store_path, path);
    FRESULT res = f_open(&ftp_file, ftp_store_path, FA_WRITE | FA_CREATE_ALWAYS);
    if (res != FR_OK) {
        ftp_send("550 Cannot create file.\r\n");
        return;
    }
    ftp_file_open = 1;
    ftp_store_received = 0;
    ftp_store_idle_polls = 0;
    ftp_data_mode = FTP_DATA_STOR;
    if (ftp_open_active_data() != 0) {
        ftp_finish_store(0);
        ftp_send("425 Use active mode with PORT first.\r\n");
    }
}

static int ftp_parse_port(const char *arg, ip_addr_t *ip, u16_t *port) {
    unsigned int nums[6];
    for (int i = 0; i < 6; i++) {
        nums[i] = 0;
        if (*arg < '0' || *arg > '9')
            return 0;
        while (*arg >= '0' && *arg <= '9') {
            nums[i] = nums[i] * 10 + (unsigned int)(*arg - '0');
            arg++;
        }
        if (i != 5) {
            if (*arg != ',')
                return 0;
            arg++;
        }
    }
    for (int i = 0; i < 6; i++) {
        if (nums[i] > 255)
            return 0;
    }

    ip4_addr_t ip4;
    if (nums[0] == 127)
        IP4_ADDR(&ip4, 10, 0, 2, 2);
    else
        IP4_ADDR(&ip4, nums[0], nums[1], nums[2], nums[3]);
    ip_addr_copy_from_ip4(*ip, ip4);
    *port = (u16_t)((nums[4] << 8) | nums[5]);
    return 1;
}

static void ftp_handle_line(char *line) {
    char cmd[FTP_LINE_MAX];
    int i = 0;
    while (line[i] && i < FTP_LINE_MAX - 1) {
        cmd[i] = line[i];
        i++;
    }
    cmd[i] = '\0';
    ftp_upper(cmd);

    if (ftp_starts_with(cmd, "USER")) {
        ftp_send("331 Anonymous login ok, send password.\r\n");
    } else if (ftp_starts_with(cmd, "PASS")) {
        ftp_send("230 Login successful.\r\n");
    } else if (ftp_starts_with(cmd, "SYST")) {
        ftp_send("215 UNIX Type: L8\r\n");
    } else if (ftp_starts_with(cmd, "FEAT")) {
        ftp_send("211-Features\r\n PASV\r\n TYPE I\r\n211 End\r\n");
    } else if (ftp_starts_with(cmd, "TYPE")) {
        ftp_send("200 Type set.\r\n");
    } else if (ftp_starts_with(cmd, "NOOP")) {
        ftp_send("200 OK.\r\n");
    } else if (ftp_starts_with(cmd, "PWD") || ftp_starts_with(cmd, "XPWD")) {
        char cwd[128];
        if (f_getcwd(cwd, sizeof(cwd)) == FR_OK)
            ftp_sendf_path("257 \"", cwd, "\"\r\n");
        else
            ftp_send("550 Failed to get cwd.\r\n");
    } else if (ftp_starts_with(cmd, "CWD")) {
        FRESULT res = f_chdir(ftp_arg(line));
        ftp_send(res == FR_OK ? "250 Directory changed.\r\n" : "550 Failed to change directory.\r\n");
    } else if (ftp_starts_with(cmd, "MKD")) {
        FRESULT res = f_mkdir(ftp_arg(line));
        ftp_send(res == FR_OK ? "257 Directory created.\r\n" : "550 Failed to create directory.\r\n");
    } else if (ftp_starts_with(cmd, "DELE")) {
        FRESULT res = f_unlink(ftp_arg(line));
        ftp_send(res == FR_OK ? "250 Deleted.\r\n" : "550 Failed to delete.\r\n");
    } else if (ftp_starts_with(cmd, "PASV")) {
        ftp_send("502 Passive mode is not supported. Use active mode.\r\n");
    } else if (ftp_starts_with(cmd, "PORT")) {
        if (ftp_parse_port(ftp_arg(line), &ftp_active_ip, &ftp_active_port)) {
            ftp_active_ready = 1;
            ftp_send("200 PORT command successful.\r\n");
        } else {
            ftp_send("501 Bad PORT syntax.\r\n");
        }
    } else if (ftp_starts_with(cmd, "EPRT")) {
        ftp_send("502 Use PORT for active mode.\r\n");
    } else if (ftp_starts_with(cmd, "LIST") || ftp_starts_with(cmd, "NLST")) {
        ftp_data_mode = FTP_DATA_LIST;
        if (ftp_open_active_data() != 0)
            ftp_send("425 Use active mode with PORT first.\r\n");
    } else if (ftp_starts_with(cmd, "STOR")) {
        ftp_start_store(ftp_arg(line));
    } else if (ftp_starts_with(cmd, "QUIT")) {
        ftp_send("221 Goodbye.\r\n");
        tcp_close(ftp_ctrl_pcb);
        ftp_ctrl_pcb = NULL;
    } else {
        ftp_send("502 Command not implemented.\r\n");
    }
}

static err_t ftp_ctrl_recv(void *arg, struct tcp_pcb *tpcb, struct pbuf *p, err_t err) {
    (void)arg;
    (void)tpcb;
    (void)err;
    if (p == NULL) {
        ftp_ctrl_pcb = NULL;
        ftp_close_data();
        return ERR_OK;
    }

    tcp_recved(tpcb, p->tot_len);
    for (struct pbuf *q = p; q != NULL; q = q->next) {
        const char *data = (const char *)q->payload;
        for (u16_t i = 0; i < q->len; i++) {
            char c = data[i];
            if (c == '\r')
                continue;
            if (c == '\n') {
                ftp_line[ftp_line_len] = '\0';
                if (ftp_line_len > 0)
                    ftp_handle_line(ftp_line);
                ftp_line_len = 0;
            } else if (ftp_line_len < FTP_LINE_MAX - 1) {
                ftp_line[ftp_line_len++] = c;
            }
        }
    }
    pbuf_free(p);
    return ERR_OK;
}

static void ftp_ctrl_err(void *arg, err_t err) {
    (void)arg;
    (void)err;
    ftp_ctrl_pcb = NULL;
    ftp_close_data();
}

static err_t ftp_ctrl_accept(void *arg, struct tcp_pcb *newpcb, err_t err) {
    (void)arg;
    if (err != ERR_OK || newpcb == NULL)
        return ERR_VAL;
    if (ftp_ctrl_pcb) {
        tcp_write(newpcb, "421 Only one FTP session is supported.\r\n", 40, TCP_WRITE_FLAG_COPY);
        tcp_close(newpcb);
        return ERR_OK;
    }

    ftp_ctrl_pcb = newpcb;
    ftp_line_len = 0;
    ftp_data_mode = FTP_DATA_IDLE;
    tcp_recv(ftp_ctrl_pcb, ftp_ctrl_recv);
    tcp_err(ftp_ctrl_pcb, ftp_ctrl_err);
    ftp_send("220 T-OS FTP ready. Use active mode.\r\n");
    return ERR_OK;
}

static int ftp_server_start(void) {
    if (ftp_listen_pcb)
        return 0;

    struct tcp_pcb *pcb = tcp_new();
    if (!pcb)
        return -1;
    if (tcp_bind(pcb, IP_ADDR_ANY, FTP_CTRL_PORT) != ERR_OK) {
        tcp_close(pcb);
        return -1;
    }
    ftp_listen_pcb = tcp_listen(pcb);
    if (!ftp_listen_pcb)
        return -1;
    tcp_accept(ftp_listen_pcb, ftp_ctrl_accept);
    return 0;
}

void cmd_ftp(int argc, char *argv[]) {
    if (argc >= 2 && strcmp(argv[1], "status") == 0) {
        kputs(ftp_listen_pcb ? "FTP server: running\n" : "FTP server: stopped\n");
        kputs("Linux client: ftp -A 127.0.0.1 2121\n");
        return;
    }

    if (ftp_server_start() == 0) {
        kputs("FTP server running.\n");
        kputs("From Linux: ftp -A 127.0.0.1 2121\n");
        kputs("Use anonymous login and active mode.\n");
    } else {
        kputs("Failed to start FTP server.\n");
    }
}

void ftp_init(void) {
    command_register("ftp", "Start FTP server for uploads", cmd_ftp);
    ftp_server_start();
}
