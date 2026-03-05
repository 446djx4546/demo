#ifndef MenuSelector_H
#define MenuSelector_H

#include "MenuSetting.h"

extern NodeBranch_t* Selector;
typedef void (*FuncPtr)(void);

void SelectUP();
void SelectDOWN();
void SelectINorRUN();
void SelectOut();			

void OLEDPrintStringLine(int line, const char* str);

void PrintSelector();

#endif // MenuSelector_H
