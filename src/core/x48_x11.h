#pragma once

typedef struct disp_t {
  unsigned int w, h;
  int offset;
  int lines;
  int mapped;
} disp_t;

extern disp_t disp;
extern int opt_gx;
extern int in_debugger;
int GetEvent(void);
void adjust_contrast(int contrast);
void refresh_icon(void);
