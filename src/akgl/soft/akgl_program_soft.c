#include "akgl/akgl_internal.h"

/* Object lifecycle.
 */
 
struct akgl_program_soft *akgl_program_soft_new() {
  struct akgl_program_soft *program=calloc(1,sizeof(struct akgl_program_soft));
  if (!program) return 0;

  program->refc=1;
  program->magic=AKGL_MAGIC_PROGRAM_SOFT;

  return program;
}

void akgl_program_soft_del(struct akgl_program_soft *program) {
  if (!program) return;
  if (program->refc-->1) return;

  free(program);
}

int akgl_program_soft_ref(struct akgl_program_soft *program) {
  if (!program) return -1;
  if (program->refc<1) return -1;
  if (program->refc==INT_MAX) return -1;
  program->refc++;
  return 0;
}
