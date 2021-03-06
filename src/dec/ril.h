#ifndef RIL_H
#define RIL_H

#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>
#include <string.h>
#include "../dish/ascii/draw.h"
#include "../rdis.h"
#include "../arch/x86_64/x64assembler.h"

#define RIL_RW 3
#define RIL_READ 2
#define RIL_WRITE 1

/* Reverset Intermediate Language
 * Examples
 *
 * sub rsp, 0x10 			| x86
 * reg(rsp) -= imm(0x10) 		| ril
 *
 * mov eax, dword [ebp-0x4] 		| x86
 * reg(eax) = mem(reg(ebp) - imm(0x40))	| ril
 *
 * cmp dword [ebp-0x4], 0x9 		| x86
 * mem(reg(ebp) - imm(0x4)) == imm(0x9) | ril
 *
 * jle 0x62c 				| x86
 * <= addr(0x62c) 			| ril
 *
 * */

/*RIL Microcode
 * push eax
 * becomes
 * m[esp] = eax
 * esp += 0x4
 *
 * Microcode:
 * m[reg(esp)] = %read
 * reg(esp) += %read.size
 *
 * Reducing jumps
 * First jumps are changed to the condition and the flags
 *
 * je 0x600
 * if (ZF) goto 0x600;
 *
 * jle 0x600
 * if (CF && ZF) goto 0x0600;
 *
 * Then the algorithm goes to the last instruction in the basic block that sets the flags to jump
 *
 * cmp eax, 0
 * je 0x600
 *
 * (eax cmp 0) side affects: cf, zf, of, sf
 * if (zf) goto 0x600
 *
 * Replace the flag in the if statement with the compare operands and the flags symbol
 * FLAGS ZF:
 * 1: "=="
 * 0: "!="
 *
 * if (eax == 0) goto 0x600
 *
 * Example 2:
 *
 * cmp ecx, 0xa
 * jle 0x123
 *
 * (eax cmp 0xa) side affects: cf, zf, of, sf
 * if (cf || zf) goto 0x123
 *
 * FLAGS CF || ZF:
 * CF:
 * 1: "<"
 * 0: ">="
 * ZF:
 * 1: "=="
 * 0: "!="
 *
 * CF==1 || ZF==1: "<="
 *
 * if (eax <= 10) goto 0x123;
 * */

#define RIL_INSTR 	2
#define RIL_OPER 	1

enum ril_operation_type {
	RIL_NOP,
	RIL_ASSIGN,
	RIL_MEM_ASSIGN,
	RIL_ADD_ASSIGN,
	RIL_SUB_ASSIGN,
	RIL_NEG_ASSIGN,
	RIL_MUL_ASSIGN,
	RIL_DIV_ASSIGN,
	RIL_XOR_ASSIGN,
	RIL_AND_ASSIGN,
	RIL_PUSH,
	RIL_POP_ASSIGN,
	RIL_RETURN,
	RIL_COMPARE,
	RIL_CALL,
	RIL_AND,
	RIL_JUMP,
	RIL_CJUMP,
	RIL_PHI
};

enum ril_op_type {
	RIL_NONE,
	RIL_REG,
	RIL_VAL
};

typedef struct ril_instruction ril_instruction;
typedef struct ril_operation ril_operation;
typedef struct ril_operation_table ril_operation_table;

typedef ril_instruction*(*ril_operand_lift)(char *operand);

/*RIL Instruction Tree. Leafs are operands branches having formatting */
struct ril_instruction {
	int type;
	int comment;
	int size;
	struct ril_instruction *next;
	union {
		/*Operand*/
		struct {
			int operand_type;
			char *reg;
			uint64_t value;
			int ssa_iter;
		};
		/*Instruction*/
		struct {
			int op_type;
			int action;
			ril_instruction **write, **read;
			int nwrite, nread;
			char *format;
		};
	};
};


ril_instruction *ril_instr_init();
void ril_instr_destroy(ril_instruction *instr);

ril_instruction *ril_instr_dup(ril_instruction *instr);
void ril_instr_add(ril_instruction *branch, ril_instruction *leaf, int type);
int ril_instr_sn(char *buf, int max, ril_instruction *instr);

/*Mnemonic -> ril operation hash table*/
struct ril_operation_table {
	ril_operation **buckets;
	int num_buckets;
	ril_operand_lift opr_decode;
};

/* Disassembly format and IL format strings
 * mov eax, edx
 * mov:
 * $w=$1, $r=$2
 * $r = $w
 *
 * jle 0x614
 * jle:
 * $r=$1
 * goto %r
 * */

struct ril_operation {
	/*Hash Table Vars*/
	unsigned long hash;
	struct ril_operation *next;

	/*Operation data*/
	char *name;
	char * dformat;
	char * ilformat;
	int action;
	//sideaffects, etc
};

ril_instruction *ril_instr_lift(ril_operation_table *table, r_disasm *dis);
int ril_dformat_parse(const char * dformat, int num_op);

ril_operation_table *ril_table_init(int num_buckets, ril_operand_lift opr_decode);
void ril_table_destroy(ril_operation_table *table);

void ril_table_insert(ril_operation_table *table, ril_operation *entry);
void ril_table_remove(ril_operation_table *table, ril_operation *entry);
void ril_table_resize(ril_operation_table *table, int new_size);
ril_operation *ril_table_lookup(ril_operation_table *table, const char *name);

unsigned long hash_mnem(const char *mnem);
void ril_oper_add(ril_operation **head, ril_operation *e);
ril_operation *ril_oper_find(ril_operation *head, long hash, const char *name);

ril_operation *ril_oper_init(char *name, int action, char *dformat, char *ilformat);
void ril_oper_destroy(ril_operation *operation;);

#endif
