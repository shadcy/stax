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
int command_register(const char *name, const char *desc, cmd_func_t func);

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
void cmd_doom(int argc, char *argv[]);
void cmd_doomgfx(int argc, char *argv[]);
void cmd_doom2gfx(int argc, char *argv[]);
void cmd_viewimg(int argc, char *argv[]);
void cmd_fbtest(int argc, char *argv[]);
void cmd_ls(int argc, char *argv[]);
void cmd_cd(int argc, char *argv[]);
void cmd_pwd(int argc, char *argv[]);
void cmd_touch(int argc, char *argv[]);
void cmd_rm(int argc, char *argv[]);
void cmd_cat(int argc, char *argv[]);
void cmd_mkdir(int argc, char *argv[]);
void cmd_nano(int argc, char *argv[]);
void cmd_game(int argc, char *argv[]);
void cmd_test_game(int argc, char *argv[]);
void cmd_slime(int argc, char *argv[]);

#endif
