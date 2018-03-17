/* ps_ioc.h
 *
 * We provide an abstraction over inversion-of-control frameworks.
 * MacOS and Windows both prefer to control applications through native IoC.
 * We want to play nice with that, and we do.
 * We also provide a generic IoC implementation for other platforms like Linux.
 * All three units export the same symbols, defined here.
 *
 * To use this interface, you must enable exactly one of these units:
 *   - macioc
 *   - msioc
 *   - genioc
 */

#ifndef PS_IOC_H
#define PS_IOC_H

struct ps_cmdline {
  const char *saved_game_path; // If not NULL, restore from this saved game.
  const char *resources_path; // REQUIRED.
  const char *input_config_path; // REQUIRED.
  int fullscreen;
  int akgl_strategy;
};

struct ps_ioc_delegate {
  int (*init)(const struct ps_cmdline *cmdline);
  void (*quit)();
  int (*update)();
};

/* The real "main" function should be just a call to this.
 */
int ps_ioc_main(int argc,char **argv,const struct ps_ioc_delegate *delegate);

/* Terminate the program.
 * Use the force if we must abort immediately.
 */
void ps_ioc_quit(int force);

#endif
