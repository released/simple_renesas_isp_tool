#ifndef F24_RUNTIME_HOOKS_H
#define F24_RUNTIME_HOOKS_H

/* Implemented independently by boot_platform.c or the bridge application. */
unsigned int get_tick(void);
void Timer_1ms_IRQ(void);

#endif
