/* ============================================================================
 * TIOS — command.h
 * Command system interface
 * ============================================================================ */

#ifndef COMMAND_H
#define COMMAND_H

/* Command function type */
typedef void (*cmd_func_t)(int argc, char *argv[]);

/* Command structure */
typedef struct {
    const char *name;
    const char *desc;
    cmd_func_t func;
} command_t;

/* Command system functions */
void command_init(void);
void command_process(char *input);
void command_show_help(void);

/* Individual command functions */
void cmd_help(int argc, char *argv[]);
void cmd_clear(int argc, char *argv[]);
void cmd_status(int argc, char *argv[]);
void cmd_mem(int argc, char *argv[]);
void cmd_tasks(int argc, char *argv[]);
void cmd_fs(int argc, char *argv[]);
void cmd_test(int argc, char *argv[]);
void cmd_read(int argc, char *argv[]);
void cmd_snake(int argc, char *argv[]);

#endif
