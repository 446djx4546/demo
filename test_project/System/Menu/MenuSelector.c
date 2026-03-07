#include "MenuSelector.h"

NodeBranch_t* Selector;

void SelectUP() {
	block_decrease(&(Selector->block));
}

void SelectDOWN() {
	block_increase(&(Selector->block));
}

void SelectINorRUN() {
	Node_t* baseNode = Selector->baseNode;
	int num = Selector->block.pointer;
	Node_t* currentNode = baseNode;
	for (int i = 0; i < num; i++) {
		currentNode = currentNode->nextNode;
	}
	if (currentNode == NULL) return;
	if (currentNode->type == EXE_type) {
		if (currentNode->pointer != NULL) {
			((FuncPtr)(uintptr_t)(currentNode->pointer))();
		}
	}
	else if (currentNode->type == DIR_type) {
		if (currentNode->pointer != NULL) {
			Selector = currentNode->pointer;
		}
	}
}

void SelectOut() {
	if (Selector->parentNode == NULL) return;
	Node_t* nodeParent = Selector->parentNode;
	if (nodeParent->parentPointer == NULL) return;
	Selector = nodeParent->parentPointer;
}

void OLEDPrintStringLine(int line, const char* str) {
	printf("[OLED:%d] [%-16.16s]\n", line, str);
}

void PrintSelector() {
	NodeBranch_t* selector = Selector;
	if (selector == NULL) return;
	block_t* blk = &selector->block;
	Node_t* node = selector->baseNode;
	for (int i = 0; i < blk->block_pointer && node != NULL; i++) {
		node = node->nextNode;
	}
    
    int i; // 把变量 i 提出来，为了后面清除多余行使用
	for (i = 0; i < blk->length && node != NULL; i++) {
		int index = blk->block_pointer + i;
		char buf[64];

		if (index == blk->pointer) {
			snprintf(buf, sizeof(buf), "> %s",node->name);
		}
		else {
			snprintf(buf, sizeof(buf), "  %s",node->name);
		}
		PrintStringLine(i, buf);

		node = node->nextNode;
	}
    
    // 【关键修复】：如果当前菜单项少于屏幕最大显示行数 (blk->length)，
    // 用纯空格把剩余的行覆盖掉，防止显示上一页的残留字符。
    for (; i < 4; i++) {
        PrintStringLine(i, "                "); // 打印16个空格
    }
}
