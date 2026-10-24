#ifndef RH850_MODEL_PORT_H
#define RH850_MODEL_PORT_H
#define Port0 0
#define Port10 10
#define Alt1 1
#define Alt2 2
#define Alt4 4
#define Alt5 5
#define Alt7 7
#define Output 1
#define Input 0
void R_PORT_SetAltFunc(int port,int pin,int alternate,int direction);
#endif
