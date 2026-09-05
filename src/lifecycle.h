#ifndef LIFECYCLE_H
#define LIFECYCLE_H

void lifecycle_install_signal_handlers(void);
int lifecycle_signal(void);
int lifecycle_exit_code(int signal_number);

#endif
