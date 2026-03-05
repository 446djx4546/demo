#include "MenuCore.h"
#include <stdarg.h>
const char stringEXE_type[10] = "EXE";
const char stringDIR_type[10] = "DIR";
Node_t NodeArr[NodeArr_Num] = {0};
static int16_t NodeAllocHead = 0;
NodeBranch_t NodeBranchArr[NodeBranchArr_Num] = { 0 };
static int16_t NodeBranchAllocHead = 0;

static NodeAllocError ErrorRecord = Zero_Error;

Node_t * NodeAlloc(){
	if (NodeAllocHead < NodeArr_Num) {
		NodeAllocHead++;
		return &(NodeArr[NodeAllocHead-1]);
	}
	else {
		ErrorRecord = Node_OutOfRange;
		return NULL;
	}
}

NodeBranch_t* NodeBranchAlloc() {
	if (NodeBranchAllocHead < NodeBranchArr_Num) {
		NodeBranchAllocHead++;
		return &(NodeBranchArr[NodeBranchAllocHead-1]);
	}
	else {
		ErrorRecord = NodeBranch_OutOfRange;
		return NULL;
	}
}

Node_t* set_node(Node_t* node, NodeType type, const char* name, void* pointer) {
	if (node == NULL) {
		return NULL;
	}
	node->type = type;
	node->name = name;
	node->pointer = pointer;
	node->parentPointer = NULL;
	node->lastNode = NULL;
	node->nextNode = NULL;
	if (type == DIR_type && pointer != NULL) {
		((NodeBranch_t*)(pointer))->parentNode = node;
	}
	return node;
}
NodeBranch_t* set_branch(NodeBranch_t* nodeBranch, ...) {
	if (nodeBranch == NULL) {
		return NULL;
	}
	nodeBranch->baseNode = NULL;
	nodeBranch->parentNode = NULL;
	nodeBranch->childNum = 0;
	va_list args;
	va_start(args, nodeBranch);
	int16_t count = 0;
	Node_t* currentNode = NULL;
	Node_t* LastNode = NULL;
	while ((currentNode = va_arg(args, Node_t*)) != NULL) {
		if (count == 0) {
			nodeBranch->baseNode = currentNode;
		}
		currentNode->parentPointer = nodeBranch;
		currentNode->lastNode = LastNode;
		if (LastNode != NULL) {
			LastNode->nextNode = currentNode;
		}
		LastNode = currentNode;
		count++;
	}
	nodeBranch->childNum = count;
	block_init(&(nodeBranch->block), Line_Max, count);
	va_end(args);
	return nodeBranch;
}
