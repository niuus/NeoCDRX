#include <stdio.h>
#include <stdlib.h>
#include "m68k.h"
#include "m68000.h"
#include "state.h"

/* global access */

struct m68k_memory_interface m68k_memory_intf;
offs_t m68k_encrypted_opcode_start[MAX_CPU];
offs_t m68k_encrypted_opcode_end[MAX_CPU];


void m68k_set_encrypted_opcode_range(int cpunum, offs_t start, offs_t end)
{
	m68k_encrypted_opcode_start[cpunum] = start;
	m68k_encrypted_opcode_end[cpunum] = end;
}


#ifndef A68K0

/****************************************************************************
 * 16-bit data memory interface
 ****************************************************************************/

static data32_t readlong_d16(offs_t address)
{
	data32_t result = program_read_word_16be(address) << 16;
	return result | program_read_word_16be(address + 2);
}

static void writelong_d16(offs_t address, data32_t data)
{
	program_write_word_16be(address, data >> 16);
	program_write_word_16be(address + 2, data);
}

/* interface for 24-bit address bus, 16-bit data bus (68000, 68010) */
static const struct m68k_memory_interface interface_d16 =
{
	0,
	program_read_byte_16be,
	program_read_word_16be,
	readlong_d16,
	program_write_byte_16be,
	program_write_word_16be,
	writelong_d16
};

#endif /* A68K0 */

/****************************************************************************
 * 32-bit data memory interface
 ****************************************************************************/

#ifndef A68K2

/* potentially misaligned 16-bit reads with a 32-bit data bus (and 24-bit address bus) */
static data16_t readword_d32(offs_t address)
{
	data16_t result;

	if (!(address & 1))
		return program_read_word_32be(address);
	result = program_read_byte_32be(address) << 8;
	return result | program_read_byte_32be(address + 1);
}

/* potentially misaligned 16-bit writes with a 32-bit data bus (and 24-bit address bus) */
static void writeword_d32(offs_t address, data16_t data)
{
	if (!(address & 1))
	{
		program_write_word_32be(address, data);
		return;
	}
	program_write_byte_32be(address, data >> 8);
	program_write_byte_32be(address + 1, data);
}

/* potentially misaligned 32-bit reads with a 32-bit data bus (and 24-bit address bus) */
static data32_t readlong_d32(offs_t address)
{
	data32_t result;

	if (!(address & 3))
		return program_read_dword_32be(address);
	else if (!(address & 1))
	{
		result = program_read_word_32be(address) << 16;
		return result | program_read_word_32be(address + 2);
	}
	result = program_read_byte_32be(address) << 24;
	result |= program_read_word_32be(address + 1) << 8;
	return result | program_read_byte_32be(address + 3);
}

/* potentially misaligned 32-bit writes with a 32-bit data bus (and 24-bit address bus) */
static void writelong_d32(offs_t address, data32_t data)
{
	if (!(address & 3))
	{
		program_write_dword_32be(address, data);
		return;
	}
	else if (!(address & 1))
	{
		program_write_word_32be(address, data >> 16);
		program_write_word_32be(address + 2, data);
		return;
	}
	program_write_byte_32be(address, data >> 24);
	program_write_word_32be(address + 1, data >> 8);
	program_write_byte_32be(address + 3, data);
}

/* interface for 32-bit data bus (68EC020, 68020) */
static const struct m68k_memory_interface interface_d32 =
{
	WORD_XOR_BE(0),
	program_read_byte_32be,
	readword_d32,
	readlong_d32,
	program_write_byte_32be,
	writeword_d32,
	writelong_d32
};


/* global access */
struct m68k_memory_interface m68k_memory_intf;

#endif /* A68K2 */


static void set_irq_line(int irqline, int state)
{
	if (irqline == IRQ_LINE_NMI)
		irqline = 7;
	switch(state)
	{
		case CLEAR_LINE:
			m68k_set_irq(0);
			break;
		case ASSERT_LINE:
			m68k_set_irq(irqline);
			break;
		default:
			m68k_set_irq(irqline);
			break;
	}
}


/****************************************************************************
 * 68000 section
 ****************************************************************************/

#ifndef A68K0

static UINT8 m68000_reg_layout[] = {
	M68K_PC, M68K_ISP, -1,
	M68K_SR, M68K_USP, -1,
	M68K_D0, M68K_A0, -1,
	M68K_D1, M68K_A1, -1,
	M68K_D2, M68K_A2, -1,
	M68K_D3, M68K_A3, -1,
	M68K_D4, M68K_A4, -1,
	M68K_D5, M68K_A5, -1,
	M68K_D6, M68K_A6, -1,
	M68K_D7, M68K_A7, 0
};

static UINT8 m68000_win_layout[] = {
	48, 0,32,13,	/* register window (top right) */
	 0, 0,47,13,	/* disassembler window (top left) */
	 0,14,47, 8,	/* memory #1 window (left, middle) */
	48,14,32, 8,	/* memory #2 window (right, middle) */
	 0,23,80, 1 	/* command line window (bottom rows) */
};

static void m68000_init(void)
{
	m68k_init();
	m68k_set_cpu_type(M68K_CPU_TYPE_68000);
	m68k_memory_intf = interface_d16;
	m68k_state_register("m68000");
}

static void m68000_reset(void* param)
{
	m68k_pulse_reset();
}

static void m68000_exit(void)
{
	/* nothing to do */
}

static int m68000_execute(int cycles)
{
	return m68k_execute(cycles);
}

static void m68000_get_context(void *dst)
{
	m68k_get_context(dst);
}

static void m68000_set_context(void *src)
{
	if (m68k_memory_intf.read8 != program_read_byte_16be)
		m68k_memory_intf = interface_d16;
	m68k_set_context(src);
}

static offs_t m68000_dasm(char *buffer, offs_t pc)
{
	M68K_SET_PC_CALLBACK(pc);
#ifdef MAME_DEBUG
	return m68k_disassemble( buffer, pc, M68K_CPU_TYPE_68000 );
#else
	sprintf( buffer, "$%04X", m68k_read_immediate_16(pc) );
	return 2;
#endif
}


/****************************************************************************
 * M68010 section
 ****************************************************************************/
#if HAS_M68010

static UINT8 m68010_reg_layout[] = {
	M68K_PC,  M68K_ISP, -1,
	M68K_SR,  M68K_USP, -1,
	M68K_SFC, M68K_VBR, -1,
	M68K_DFC, -1,
	M68K_D0,  M68K_A0, -1,
	M68K_D1,  M68K_A1, -1,
	M68K_D2,  M68K_A2, -1,
	M68K_D3,  M68K_A3, -1,
	M68K_D4,  M68K_A4, -1,
	M68K_D5,  M68K_A5, -1,
	M68K_D6,  M68K_A6, -1,
	M68K_D7,  M68K_A7, 0
};

void m68010_init(void)
{
	m68k_init();
	m68k_set_cpu_type(M68K_CPU_TYPE_68010);
	m68k_memory_intf = interface_d16;
	m68k_state_register("m68010");
}

static offs_t m68010_dasm(char *buffer, offs_t pc)
{
	M68K_SET_PC_CALLBACK(pc);
#ifdef MAME_DEBUG
	return m68k_disassemble(buffer, pc, M68K_CPU_TYPE_68010);
#else
	sprintf( buffer, "$%04X", m68k_read_immediate_16(pc) );
	return 2;
#endif
}

#endif /* HAS_M68010 */

#endif /* A68K0 */



#ifndef A68K2

/****************************************************************************
 * M68020 section
 ****************************************************************************/

static UINT8 m68020_reg_layout[] = {
	M68K_PC,  M68K_MSP, -1,
	M68K_SR,  M68K_ISP, -1,
	M68K_SFC, M68K_USP, -1,
	M68K_DFC, M68K_VBR, -1,
	M68K_D0,  M68K_A0, -1,
	M68K_D1,  M68K_A1, -1,
	M68K_D2,  M68K_A2, -1,
	M68K_D3,  M68K_A3, -1,
	M68K_D4,  M68K_A4, -1,
	M68K_D5,  M68K_A5, -1,
	M68K_D6,  M68K_A6, -1,
	M68K_D7,  M68K_A7, 0
};

static UINT8 m68020_win_layout[] = {
	48, 0,32,13,	/* register window (top right) */
	 0, 0,47,13,	/* disassembler window (top left) */
	 0,14,47, 8,	/* memory #1 window (left, middle) */
	48,14,32, 8,	/* memory #2 window (right, middle) */
	 0,23,80, 1 	/* command line window (bottom rows) */
};


static void m68020_init(void)
{
	m68k_init();
	m68k_set_cpu_type(M68K_CPU_TYPE_68020);
	m68k_memory_intf = interface_d32;
	m68k_state_register("m68020");
}

static void m68020_reset(void* param)
{
	m68k_pulse_reset();
}

static void m68020_exit(void)
{
	/* nothing to do */
}

static int m68020_execute(int cycles)
{
	return m68k_execute(cycles);
}

static void m68020_get_context(void *dst)
{
	m68k_get_context(dst);
}

static void m68020_set_context(void *src)
{
	if (m68k_memory_intf.read8 != program_read_byte_32be)
		m68k_memory_intf = interface_d32;
	m68k_set_context(src);
}

static offs_t m68020_dasm(char *buffer, offs_t pc)
{
	M68K_SET_PC_CALLBACK(pc);
#ifdef MAME_DEBUG
	return m68k_disassemble(buffer, pc, M68K_CPU_TYPE_68020);
#else
	sprintf( buffer, "$%04X", m68k_read_immediate_16(pc) );
	return 2;
#endif
}


/****************************************************************************
 * M680EC20 section
 ****************************************************************************/

#if HAS_M68EC020

static void m68ec020_init(void)
{
	m68k_init();
	m68k_set_cpu_type(M68K_CPU_TYPE_68EC020);
	m68k_memory_intf = interface_d32;
	m68k_state_register("m68ec020");
}

static offs_t m68ec020_dasm(char *buffer, offs_t pc)
{
	M68K_SET_PC_CALLBACK(pc);
#ifdef MAME_DEBUG
	return m68k_disassemble(buffer, pc, M68K_CPU_TYPE_68EC020);
#else
	sprintf( buffer, "$%04X", m68k_read_immediate_16(pc) );
	return 2;
#endif
}
#endif /* HAS_M68EC020 */

#endif /* A68K2 */



#ifndef A68K0

/**************************************************************************
 * Generic set_info
 **************************************************************************/

static void m68000_set_info(UINT32 state, union cpuinfo *info)
{
	switch (state)
	{
		/* --- the following bits of info are set as 64-bit signed integers --- */
		case CPUINFO_INT_IRQ_STATE + 0:				set_irq_line(0, info->i);					break;
		case CPUINFO_INT_IRQ_STATE + 1:				set_irq_line(1, info->i);					break;
		case CPUINFO_INT_IRQ_STATE + 2:				set_irq_line(2, info->i);					break;
		case CPUINFO_INT_IRQ_STATE + 3:				set_irq_line(3, info->i);					break;
		case CPUINFO_INT_IRQ_STATE + 4:				set_irq_line(4, info->i);					break;
		case CPUINFO_INT_IRQ_STATE + 5:				set_irq_line(5, info->i);					break;
		case CPUINFO_INT_IRQ_STATE + 6:				set_irq_line(6, info->i);					break;
		case CPUINFO_INT_IRQ_STATE + 7:				set_irq_line(7, info->i);					break;

		case CPUINFO_INT_PC:  						m68k_set_reg(M68K_REG_PC, info->i&0x00ffffff); break;
		case CPUINFO_INT_REGISTER + M68K_PC:  		m68k_set_reg(M68K_REG_PC, info->i);			break;
		case CPUINFO_INT_SP:
		case CPUINFO_INT_REGISTER + M68K_SP:  		m68k_set_reg(M68K_REG_SP, info->i);			break;
		case CPUINFO_INT_REGISTER + M68K_ISP: 		m68k_set_reg(M68K_REG_ISP, info->i);		break;
		case CPUINFO_INT_REGISTER + M68K_USP: 		m68k_set_reg(M68K_REG_USP, info->i);		break;
		case CPUINFO_INT_REGISTER + M68K_SR:  		m68k_set_reg(M68K_REG_SR, info->i);			break;
		case CPUINFO_INT_REGISTER + M68K_D0:  		m68k_set_reg(M68K_REG_D0, info->i);			break;
		case CPUINFO_INT_REGISTER + M68K_D1:  		m68k_set_reg(M68K_REG_D1, info->i);			break;
		case CPUINFO_INT_REGISTER + M68K_D2:  		m68k_set_reg(M68K_REG_D2, info->i);			break;
		case CPUINFO_INT_REGISTER + M68K_D3:  		m68k_set_reg(M68K_REG_D3, info->i);			break;
		case CPUINFO_INT_REGISTER + M68K_D4:  		m68k_set_reg(M68K_REG_D4, info->i);			break;
		case CPUINFO_INT_REGISTER + M68K_D5:  		m68k_set_reg(M68K_REG_D5, info->i);			break;
		case CPUINFO_INT_REGISTER + M68K_D6:  		m68k_set_reg(M68K_REG_D6, info->i);			break;
		case CPUINFO_INT_REGISTER + M68K_D7:  		m68k_set_reg(M68K_REG_D7, info->i);			break;
		case CPUINFO_INT_REGISTER + M68K_A0:  		m68k_set_reg(M68K_REG_A0, info->i);			break;
		case CPUINFO_INT_REGISTER + M68K_A1:  		m68k_set_reg(M68K_REG_A1, info->i);			break;
		case CPUINFO_INT_REGISTER + M68K_A2:  		m68k_set_reg(M68K_REG_A2, info->i);			break;
		case CPUINFO_INT_REGISTER + M68K_A3:  		m68k_set_reg(M68K_REG_A3, info->i);			break;
		case CPUINFO_INT_REGISTER + M68K_A4:  		m68k_set_reg(M68K_REG_A4, info->i);			break;
		case CPUINFO_INT_REGISTER + M68K_A5:  		m68k_set_reg(M68K_REG_A5, info->i);			break;
		case CPUINFO_INT_REGISTER + M68K_A6:  		m68k_set_reg(M68K_REG_A6, info->i);			break;
		case CPUINFO_INT_REGISTER + M68K_A7:  		m68k_set_reg(M68K_REG_A7, info->i);			break;
		
		/* --- the following bits of info are set as pointers to data or functions --- */
		case CPUINFO_PTR_IRQ_CALLBACK:				m68k_set_int_ack_callback(info->irqcallback); break;
	}
}


/**************************************************************************
 * Generic get_info
 **************************************************************************/

void m68000_get_info(UINT32 state, union cpuinfo *info)
{
	int sr;

	switch (state)
	{
		/* --- the following bits of info are returned as 64-bit signed integers --- */
		case CPUINFO_INT_CONTEXT_SIZE:					info->i = m68k_get_context(NULL);		break;
		case CPUINFO_INT_IRQ_LINES:						info->i = 8;							break;
		case CPUINFO_INT_DEFAULT_IRQ_VECTOR:			info->i = -1;							break;
		case CPUINFO_INT_ENDIANNESS:					info->i = CPU_IS_BE;					break;
		case CPUINFO_INT_CLOCK_DIVIDER:					info->i = 1;							break;
		case CPUINFO_INT_MIN_INSTRUCTION_BYTES:			info->i = 2;							break;
		case CPUINFO_INT_MAX_INSTRUCTION_BYTES:			info->i = 10;							break;
		case CPUINFO_INT_MIN_CYCLES:					info->i = 4;							break;
		case CPUINFO_INT_MAX_CYCLES:					info->i = 158;							break;
		
		case CPUINFO_INT_DATABUS_WIDTH + ADDRESS_SPACE_PROGRAM:	info->i = 16;					break;
		case CPUINFO_INT_ADDRBUS_WIDTH + ADDRESS_SPACE_PROGRAM: info->i = 24;					break;
		case CPUINFO_INT_ADDRBUS_SHIFT + ADDRESS_SPACE_PROGRAM: info->i = 0;					break;
		case CPUINFO_INT_DATABUS_WIDTH + ADDRESS_SPACE_DATA:	info->i = 0;					break;
		case CPUINFO_INT_ADDRBUS_WIDTH + ADDRESS_SPACE_DATA: 	info->i = 0;					break;
		case CPUINFO_INT_ADDRBUS_SHIFT + ADDRESS_SPACE_DATA: 	info->i = 0;					break;
		case CPUINFO_INT_DATABUS_WIDTH + ADDRESS_SPACE_IO:		info->i = 0;					break;
		case CPUINFO_INT_ADDRBUS_WIDTH + ADDRESS_SPACE_IO: 		info->i = 0;					break;
		case CPUINFO_INT_ADDRBUS_SHIFT + ADDRESS_SPACE_IO: 		info->i = 0;					break;

		case CPUINFO_INT_IRQ_STATE + 0:					info->i = 0;	/* fix me */			break;
		case CPUINFO_INT_IRQ_STATE + 1:					info->i = 0;	/* fix me */			break;
		case CPUINFO_INT_IRQ_STATE + 2:					info->i = 0;	/* fix me */			break;
		case CPUINFO_INT_IRQ_STATE + 3:					info->i = 0;	/* fix me */			break;
		case CPUINFO_INT_IRQ_STATE + 4:					info->i = 0;	/* fix me */			break;
		case CPUINFO_INT_IRQ_STATE + 5:					info->i = 0;	/* fix me */			break;
		case CPUINFO_INT_IRQ_STATE + 6:					info->i = 0;	/* fix me */			break;
		case CPUINFO_INT_IRQ_STATE + 7:					info->i = 0;	/* fix me */			break;

		case CPUINFO_INT_PREVIOUSPC:					info->i = m68k_get_reg(NULL, M68K_REG_PPC); break;

		case CPUINFO_INT_PC:							info->i = m68k_get_reg(NULL, M68K_REG_PC)&0x00ffffff; break;
		case CPUINFO_INT_REGISTER + M68K_PC:			info->i = m68k_get_reg(NULL, M68K_REG_PC); break;
		case CPUINFO_INT_SP:
		case CPUINFO_INT_REGISTER + M68K_SP:			info->i = m68k_get_reg(NULL, M68K_REG_SP); break;
		case CPUINFO_INT_REGISTER + M68K_ISP:			info->i = m68k_get_reg(NULL, M68K_REG_ISP); break;
		case CPUINFO_INT_REGISTER + M68K_USP:			info->i = m68k_get_reg(NULL, M68K_REG_USP); break;
		case CPUINFO_INT_REGISTER + M68K_SR:			info->i = m68k_get_reg(NULL, M68K_REG_SR); break;
		case CPUINFO_INT_REGISTER + M68K_D0:			info->i = m68k_get_reg(NULL, M68K_REG_D0); break;
		case CPUINFO_INT_REGISTER + M68K_D1:			info->i = m68k_get_reg(NULL, M68K_REG_D1); break;
		case CPUINFO_INT_REGISTER + M68K_D2:			info->i = m68k_get_reg(NULL, M68K_REG_D2); break;
		case CPUINFO_INT_REGISTER + M68K_D3:			info->i = m68k_get_reg(NULL, M68K_REG_D3); break;
		case CPUINFO_INT_REGISTER + M68K_D4:			info->i = m68k_get_reg(NULL, M68K_REG_D4); break;
		case CPUINFO_INT_REGISTER + M68K_D5:			info->i = m68k_get_reg(NULL, M68K_REG_D5); break;
		case CPUINFO_INT_REGISTER + M68K_D6:			info->i = m68k_get_reg(NULL, M68K_REG_D6); break;
		case CPUINFO_INT_REGISTER + M68K_D7:			info->i = m68k_get_reg(NULL, M68K_REG_D7); break;
		case CPUINFO_INT_REGISTER + M68K_A0:			info->i = m68k_get_reg(NULL, M68K_REG_A0); break;
		case CPUINFO_INT_REGISTER + M68K_A1:			info->i = m68k_get_reg(NULL, M68K_REG_A1); break;
		case CPUINFO_INT_REGISTER + M68K_A2:			info->i = m68k_get_reg(NULL, M68K_REG_A2); break;
		case CPUINFO_INT_REGISTER + M68K_A3:			info->i = m68k_get_reg(NULL, M68K_REG_A3); break;
		case CPUINFO_INT_REGISTER + M68K_A4:			info->i = m68k_get_reg(NULL, M68K_REG_A4); break;
		case CPUINFO_INT_REGISTER + M68K_A5:			info->i = m68k_get_reg(NULL, M68K_REG_A5); break;
		case CPUINFO_INT_REGISTER + M68K_A6:			info->i = m68k_get_reg(NULL, M68K_REG_A6); break;
		case CPUINFO_INT_REGISTER + M68K_A7:			info->i = m68k_get_reg(NULL, M68K_REG_A7); break;
		case CPUINFO_INT_REGISTER + M68K_PREF_ADDR:		info->i = m68k_get_reg(NULL, M68K_REG_PREF_ADDR); break;
		case CPUINFO_INT_REGISTER + M68K_PREF_DATA:		info->i = m68k_get_reg(NULL, M68K_REG_PREF_DATA); break;

		/* --- the following bits of info are returned as pointers to data or functions --- */
		case CPUINFO_PTR_SET_INFO:						info->setinfo = m68000_set_info;		break;
		case CPUINFO_PTR_GET_CONTEXT:					info->getcontext = m68000_get_context;	break;
		case CPUINFO_PTR_SET_CONTEXT:					info->setcontext = m68000_set_context;	break;
		case CPUINFO_PTR_INIT:							info->init = m68000_init;				break;
		case CPUINFO_PTR_RESET:							info->reset = m68000_reset;				break;
		case CPUINFO_PTR_EXIT:							info->exit = m68000_exit;				break;
		case CPUINFO_PTR_EXECUTE:						info->execute = m68000_execute;			break;
		case CPUINFO_PTR_BURN:							info->burn = NULL;						break;
		case CPUINFO_PTR_DISASSEMBLE:					info->disassemble = m68000_dasm;		break;
		case CPUINFO_PTR_IRQ_CALLBACK:					/* fix me */							break;
		case CPUINFO_PTR_INSTRUCTION_COUNTER:			info->icount = &m68k_ICount;			break;
		case CPUINFO_PTR_REGISTER_LAYOUT:				info->p = m68000_reg_layout;			break;
		case CPUINFO_PTR_WINDOW_LAYOUT:					info->p = m68000_win_layout;			break;

		/* --- the following bits of info are returned as NULL-terminated strings --- */
		case CPUINFO_STR_NAME:							strcpy(info->s = cpuintrf_temp_str(), "68000"); break;
		case CPUINFO_STR_CORE_FAMILY:					strcpy(info->s = cpuintrf_temp_str(), "Motorola 68K"); break;
		case CPUINFO_STR_CORE_VERSION:					strcpy(info->s = cpuintrf_temp_str(), "3.2"); break;
		case CPUINFO_STR_CORE_FILE:						strcpy(info->s = cpuintrf_temp_str(), __FILE__); break;
		case CPUINFO_STR_CORE_CREDITS:					strcpy(info->s = cpuintrf_temp_str(), "Copyright 1999-2000 Karl Stenerud. All rights reserved. (2.1 fixes HJB)"); break;

		case CPUINFO_STR_FLAGS:
			sr = m68k_get_reg(NULL, M68K_REG_SR);
			sprintf(info->s = cpuintrf_temp_str(), "%c%c%c%c%c%c%c%c%c%c%c%c%c%c%c%c",
				sr & 0x8000 ? 'T':'.',
				sr & 0x4000 ? '?':'.',
				sr & 0x2000 ? 'S':'.',
				sr & 0x1000 ? '?':'.',
				sr & 0x0800 ? '?':'.',
				sr & 0x0400 ? 'I':'.',
				sr & 0x0200 ? 'I':'.',
				sr & 0x0100 ? 'I':'.',
				sr & 0x0080 ? '?':'.',
				sr & 0x0040 ? '?':'.',
				sr & 0x0020 ? '?':'.',
				sr & 0x0010 ? 'X':'.',
				sr & 0x0008 ? 'N':'.',
				sr & 0x0004 ? 'Z':'.',
				sr & 0x0002 ? 'V':'.',
				sr & 0x0001 ? 'C':'.');
			break;

		case CPUINFO_STR_REGISTER + M68K_PC:			sprintf(info->s = cpuintrf_temp_str(), "PC :%08X", m68k_get_reg(NULL, M68K_REG_PC)); break;
		case CPUINFO_STR_REGISTER + M68K_SR:  			sprintf(info->s = cpuintrf_temp_str(), "SR :%04X", m68k_get_reg(NULL, M68K_REG_SR)); break;
		case CPUINFO_STR_REGISTER + M68K_SP:  			sprintf(info->s = cpuintrf_temp_str(), "SP :%08X", m68k_get_reg(NULL, M68K_REG_SP)); break;
		case CPUINFO_STR_REGISTER + M68K_ISP: 			sprintf(info->s = cpuintrf_temp_str(), "ISP:%08X", m68k_get_reg(NULL, M68K_REG_ISP)); break;
		case CPUINFO_STR_REGISTER + M68K_USP: 			sprintf(info->s = cpuintrf_temp_str(), "USP:%08X", m68k_get_reg(NULL, M68K_REG_USP)); break;
		case CPUINFO_STR_REGISTER + M68K_D0:			sprintf(info->s = cpuintrf_temp_str(), "D0 :%08X", m68k_get_reg(NULL, M68K_REG_D0)); break;
		case CPUINFO_STR_REGISTER + M68K_D1:			sprintf(info->s = cpuintrf_temp_str(), "D1 :%08X", m68k_get_reg(NULL, M68K_REG_D1)); break;
		case CPUINFO_STR_REGISTER + M68K_D2:			sprintf(info->s = cpuintrf_temp_str(), "D2 :%08X", m68k_get_reg(NULL, M68K_REG_D2)); break;
		case CPUINFO_STR_REGISTER + M68K_D3:			sprintf(info->s = cpuintrf_temp_str(), "D3 :%08X", m68k_get_reg(NULL, M68K_REG_D3)); break;
		case CPUINFO_STR_REGISTER + M68K_D4:			sprintf(info->s = cpuintrf_temp_str(), "D4 :%08X", m68k_get_reg(NULL, M68K_REG_D4)); break;
		case CPUINFO_STR_REGISTER + M68K_D5:			sprintf(info->s = cpuintrf_temp_str(), "D5 :%08X", m68k_get_reg(NULL, M68K_REG_D5)); break;
		case CPUINFO_STR_REGISTER + M68K_D6:			sprintf(info->s = cpuintrf_temp_str(), "D6 :%08X", m68k_get_reg(NULL, M68K_REG_D6)); break;
		case CPUINFO_STR_REGISTER + M68K_D7:			sprintf(info->s = cpuintrf_temp_str(), "D7 :%08X", m68k_get_reg(NULL, M68K_REG_D7)); break;
		case CPUINFO_STR_REGISTER + M68K_A0:			sprintf(info->s = cpuintrf_temp_str(), "A0 :%08X", m68k_get_reg(NULL, M68K_REG_A0)); break;
		case CPUINFO_STR_REGISTER + M68K_A1:			sprintf(info->s = cpuintrf_temp_str(), "A1 :%08X", m68k_get_reg(NULL, M68K_REG_A1)); break;
		case CPUINFO_STR_REGISTER + M68K_A2:			sprintf(info->s = cpuintrf_temp_str(), "A2 :%08X", m68k_get_reg(NULL, M68K_REG_A2)); break;
		case CPUINFO_STR_REGISTER + M68K_A3:			sprintf(info->s = cpuintrf_temp_str(), "A3 :%08X", m68k_get_reg(NULL, M68K_REG_A3)); break;
		case CPUINFO_STR_REGISTER + M68K_A4:			sprintf(info->s = cpuintrf_temp_str(), "A4 :%08X", m68k_get_reg(NULL, M68K_REG_A4)); break;
		case CPUINFO_STR_REGISTER + M68K_A5:			sprintf(info->s = cpuintrf_temp_str(), "A5 :%08X", m68k_get_reg(NULL, M68K_REG_A5)); break;
		case CPUINFO_STR_REGISTER + M68K_A6:			sprintf(info->s = cpuintrf_temp_str(), "A6 :%08X", m68k_get_reg(NULL, M68K_REG_A6)); break;
		case CPUINFO_STR_REGISTER + M68K_A7:			sprintf(info->s = cpuintrf_temp_str(), "A7 :%08X", m68k_get_reg(NULL, M68K_REG_A7)); break;
		case CPUINFO_STR_REGISTER + M68K_PREF_ADDR:		sprintf(info->s = cpuintrf_temp_str(), "PAR:%08X", m68k_get_reg(NULL, M68K_REG_PREF_ADDR)); break;
		case CPUINFO_STR_REGISTER + M68K_PREF_DATA:		sprintf(info->s = cpuintrf_temp_str(), "PDA:%08X", m68k_get_reg(NULL, M68K_REG_PREF_DATA)); break;
	}
}


#if (HAS_M68010)
/**************************************************************************
 * CPU-specific set_info
 **************************************************************************/

static void m68010_set_info(UINT32 state, union cpuinfo *info)
{
	switch (state)
	{
		/* --- the following bits of info are set as 64-bit signed integers --- */
		case CPUINFO_INT_REGISTER + M68K_VBR:  			m68k_set_reg(M68K_REG_VBR, info->i);	break;
		case CPUINFO_INT_REGISTER + M68K_SFC:  			m68k_set_reg(M68K_REG_SFC, info->i);	break;
		case CPUINFO_INT_REGISTER + M68K_DFC:  			m68k_set_reg(M68K_REG_DFC, info->i);	break;

		/* --- the following bits of info are set as pointers to data or functions --- */

		default:
			m68000_set_info(state, info);
			break;
	}
}

void m68010_get_info(UINT32 state, union cpuinfo *info)
{
	switch (state)
	{
		/* --- the following bits of info are returned as 64-bit signed integers --- */
		case CPUINFO_INT_REGISTER + M68K_VBR:  			info->i = m68k_get_reg(NULL, M68K_REG_VBR); break;
		case CPUINFO_INT_REGISTER + M68K_SFC:  			info->i = m68k_get_reg(NULL, M68K_REG_SFC); break;
		case CPUINFO_INT_REGISTER + M68K_DFC:  			info->i = m68k_get_reg(NULL, M68K_REG_DFC); break;

		/* --- the following bits of info are returned as pointers to data or functions --- */
		case CPUINFO_PTR_SET_INFO:						info->setinfo = m68010_set_info;		break;
		case CPUINFO_PTR_INIT:							info->init = m68010_init;				break;
		case CPUINFO_PTR_DISASSEMBLE:					info->disassemble = m68010_dasm;		break;
		case CPUINFO_PTR_REGISTER_LAYOUT:				info->p = m68010_reg_layout;			break;

		/* --- the following bits of info are returned as NULL-terminated strings --- */
		case CPUINFO_STR_NAME:							strcpy(info->s = cpuintrf_temp_str(), "68010"); break;
		case CPUINFO_STR_REGISTER + M68K_SFC:			sprintf(info->s = cpuintrf_temp_str(), "SFC:%X",   m68k_get_reg(NULL, M68K_REG_SFC)); break;
		case CPUINFO_STR_REGISTER + M68K_DFC:			sprintf(info->s = cpuintrf_temp_str(), "DFC:%X",   m68k_get_reg(NULL, M68K_REG_DFC)); break;
		case CPUINFO_STR_REGISTER + M68K_VBR:			sprintf(info->s = cpuintrf_temp_str(), "VBR:%08X", m68k_get_reg(NULL, M68K_REG_VBR)); break;

		default:
			m68000_get_info(state, info);
			break;
	}
}
#endif

#endif


#ifndef A68K2

/**************************************************************************
 * CPU-specific set_info
 **************************************************************************/

static void m68020_set_info(UINT32 state, union cpuinfo *info)
{
	switch (state)
	{
		/* --- the following bits of info are set as 64-bit signed integers --- */
		case CPUINFO_INT_IRQ_STATE + 0:				set_irq_line(0, info->i);					break;
		case CPUINFO_INT_IRQ_STATE + 1:				set_irq_line(1, info->i);					break;
		case CPUINFO_INT_IRQ_STATE + 2:				set_irq_line(2, info->i);					break;
		case CPUINFO_INT_IRQ_STATE + 3:				set_irq_line(3, info->i);					break;
		case CPUINFO_INT_IRQ_STATE + 4:				set_irq_line(4, info->i);					break;
		case CPUINFO_INT_IRQ_STATE + 5:				set_irq_line(5, info->i);					break;
		case CPUINFO_INT_IRQ_STATE + 6:				set_irq_line(6, info->i);					break;
		case CPUINFO_INT_IRQ_STATE + 7:				set_irq_line(7, info->i);					break;

		case CPUINFO_INT_PC:  						m68k_set_reg(M68K_REG_PC, info->i);			break;
		case CPUINFO_INT_REGISTER + M68K_PC:  		m68k_set_reg(M68K_REG_PC, info->i);			break;
		case CPUINFO_INT_SP:
		case CPUINFO_INT_REGISTER + M68K_SP:  		m68k_set_reg(M68K_REG_SP, info->i);			break;
		case CPUINFO_INT_REGISTER + M68K_ISP: 		m68k_set_reg(M68K_REG_ISP, info->i);		break;
		case CPUINFO_INT_REGISTER + M68K_USP: 		m68k_set_reg(M68K_REG_USP, info->i);		break;
		case CPUINFO_INT_REGISTER + M68K_SR:  		m68k_set_reg(M68K_REG_SR, info->i);			break;
		case CPUINFO_INT_REGISTER + M68K_D0:  		m68k_set_reg(M68K_REG_D0, info->i);			break;
		case CPUINFO_INT_REGISTER + M68K_D1:  		m68k_set_reg(M68K_REG_D1, info->i);			break;
		case CPUINFO_INT_REGISTER + M68K_D2:  		m68k_set_reg(M68K_REG_D2, info->i);			break;
		case CPUINFO_INT_REGISTER + M68K_D3:  		m68k_set_reg(M68K_REG_D3, info->i);			break;
		case CPUINFO_INT_REGISTER + M68K_D4:  		m68k_set_reg(M68K_REG_D4, info->i);			break;
		case CPUINFO_INT_REGISTER + M68K_D5:  		m68k_set_reg(M68K_REG_D5, info->i);			break;
		case CPUINFO_INT_REGISTER + M68K_D6:  		m68k_set_reg(M68K_REG_D6, info->i);			break;
		case CPUINFO_INT_REGISTER + M68K_D7:  		m68k_set_reg(M68K_REG_D7, info->i);			break;
		case CPUINFO_INT_REGISTER + M68K_A0:  		m68k_set_reg(M68K_REG_A0, info->i);			break;
		case CPUINFO_INT_REGISTER + M68K_A1:  		m68k_set_reg(M68K_REG_A1, info->i);			break;
		case CPUINFO_INT_REGISTER + M68K_A2:  		m68k_set_reg(M68K_REG_A2, info->i);			break;
		case CPUINFO_INT_REGISTER + M68K_A3:  		m68k_set_reg(M68K_REG_A3, info->i);			break;
		case CPUINFO_INT_REGISTER + M68K_A4:  		m68k_set_reg(M68K_REG_A4, info->i);			break;
		case CPUINFO_INT_REGISTER + M68K_A5:  		m68k_set_reg(M68K_REG_A5, info->i);			break;
		case CPUINFO_INT_REGISTER + M68K_A6:  		m68k_set_reg(M68K_REG_A6, info->i);			break;
		case CPUINFO_INT_REGISTER + M68K_A7:  		m68k_set_reg(M68K_REG_A7, info->i);			break;
		case CPUINFO_INT_REGISTER + M68K_MSP:		m68k_set_reg(M68K_REG_MSP, info->i);		break; /* 68020+ */
		case CPUINFO_INT_REGISTER + M68K_CACR:		m68k_set_reg(M68K_REG_CACR, info->i);		break; /* 68020+ */
		case CPUINFO_INT_REGISTER + M68K_CAAR:		m68k_set_reg(M68K_REG_CAAR, info->i);		break; /* 68020+ */
		case CPUINFO_INT_REGISTER + M68K_VBR:		m68k_set_reg(M68K_REG_VBR, info->i);		break; /* 68010+ */
		case CPUINFO_INT_REGISTER + M68K_SFC:		m68k_set_reg(M68K_REG_SFC, info->i);		break; /* 68010+ */
		case CPUINFO_INT_REGISTER + M68K_DFC:		m68k_set_reg(M68K_REG_DFC, info->i);		break; /* 68010+ */
		
		/* --- the following bits of info are set as pointers to data or functions --- */
		case CPUINFO_PTR_IRQ_CALLBACK:				m68k_set_int_ack_callback(info->irqcallback); break;
	}
}

void m68020_get_info(UINT32 state, union cpuinfo *info)
{
	int sr;

	switch (state)
	{
		/* --- the following bits of info are returned as 64-bit signed integers --- */
		case CPUINFO_INT_CONTEXT_SIZE:					info->i = m68k_get_context(NULL);		break;
		case CPUINFO_INT_IRQ_LINES:						info->i = 8;							break;
		case CPUINFO_INT_DEFAULT_IRQ_VECTOR:			info->i = -1;							break;
		case CPUINFO_INT_ENDIANNESS:					info->i = CPU_IS_BE;					break;
		case CPUINFO_INT_CLOCK_DIVIDER:					info->i = 1;							break;
		case CPUINFO_INT_MIN_INSTRUCTION_BYTES:			info->i = 2;							break;
		case CPUINFO_INT_MAX_INSTRUCTION_BYTES:			info->i = 10;							break;
		case CPUINFO_INT_MIN_CYCLES:					info->i = 4;							break;
		case CPUINFO_INT_MAX_CYCLES:					info->i = 158;							break;
		
		case CPUINFO_INT_DATABUS_WIDTH + ADDRESS_SPACE_PROGRAM:	info->i = 32;					break;
		case CPUINFO_INT_ADDRBUS_WIDTH + ADDRESS_SPACE_PROGRAM: info->i = 32;					break;
		case CPUINFO_INT_ADDRBUS_SHIFT + ADDRESS_SPACE_PROGRAM: info->i = 0;					break;
		case CPUINFO_INT_DATABUS_WIDTH + ADDRESS_SPACE_DATA:	info->i = 0;					break;
		case CPUINFO_INT_ADDRBUS_WIDTH + ADDRESS_SPACE_DATA: 	info->i = 0;					break;
		case CPUINFO_INT_ADDRBUS_SHIFT + ADDRESS_SPACE_DATA: 	info->i = 0;					break;
		case CPUINFO_INT_DATABUS_WIDTH + ADDRESS_SPACE_IO:		info->i = 0;					break;
		case CPUINFO_INT_ADDRBUS_WIDTH + ADDRESS_SPACE_IO: 		info->i = 0;					break;
		case CPUINFO_INT_ADDRBUS_SHIFT + ADDRESS_SPACE_IO: 		info->i = 0;					break;

		case CPUINFO_INT_IRQ_STATE + 0:					info->i = 0;	/* fix me */			break;
		case CPUINFO_INT_IRQ_STATE + 1:					info->i = 1;	/* fix me */			break;
		case CPUINFO_INT_IRQ_STATE + 2:					info->i = 2;	/* fix me */			break;
		case CPUINFO_INT_IRQ_STATE + 3:					info->i = 3;	/* fix me */			break;
		case CPUINFO_INT_IRQ_STATE + 4:					info->i = 4;	/* fix me */			break;
		case CPUINFO_INT_IRQ_STATE + 5:					info->i = 5;	/* fix me */			break;
		case CPUINFO_INT_IRQ_STATE + 6:					info->i = 6;	/* fix me */			break;
		case CPUINFO_INT_IRQ_STATE + 7:					info->i = 7;	/* fix me */			break;

		case CPUINFO_INT_PREVIOUSPC:					info->i = m68k_get_reg(NULL, M68K_REG_PPC); break;

		case CPUINFO_INT_PC:							info->i = m68k_get_reg(NULL, M68K_REG_PC); break;
		case CPUINFO_INT_REGISTER + M68K_PC:			info->i = m68k_get_reg(NULL, M68K_REG_PC); break;
		case CPUINFO_INT_SP:
		case CPUINFO_INT_REGISTER + M68K_SP:			info->i = m68k_get_reg(NULL, M68K_REG_SP); break;
		case CPUINFO_INT_REGISTER + M68K_ISP:			info->i = m68k_get_reg(NULL, M68K_REG_ISP); break;
		case CPUINFO_INT_REGISTER + M68K_USP:			info->i = m68k_get_reg(NULL, M68K_REG_USP); break;
		case CPUINFO_INT_REGISTER + M68K_SR:			info->i = m68k_get_reg(NULL, M68K_REG_SR); break;
		case CPUINFO_INT_REGISTER + M68K_D0:			info->i = m68k_get_reg(NULL, M68K_REG_D0); break;
		case CPUINFO_INT_REGISTER + M68K_D1:			info->i = m68k_get_reg(NULL, M68K_REG_D1); break;
		case CPUINFO_INT_REGISTER + M68K_D2:			info->i = m68k_get_reg(NULL, M68K_REG_D2); break;
		case CPUINFO_INT_REGISTER + M68K_D3:			info->i = m68k_get_reg(NULL, M68K_REG_D3); break;
		case CPUINFO_INT_REGISTER + M68K_D4:			info->i = m68k_get_reg(NULL, M68K_REG_D4); break;
		case CPUINFO_INT_REGISTER + M68K_D5:			info->i = m68k_get_reg(NULL, M68K_REG_D5); break;
		case CPUINFO_INT_REGISTER + M68K_D6:			info->i = m68k_get_reg(NULL, M68K_REG_D6); break;
		case CPUINFO_INT_REGISTER + M68K_D7:			info->i = m68k_get_reg(NULL, M68K_REG_D7); break;
		case CPUINFO_INT_REGISTER + M68K_A0:			info->i = m68k_get_reg(NULL, M68K_REG_A0); break;
		case CPUINFO_INT_REGISTER + M68K_A1:			info->i = m68k_get_reg(NULL, M68K_REG_A1); break;
		case CPUINFO_INT_REGISTER + M68K_A2:			info->i = m68k_get_reg(NULL, M68K_REG_A2); break;
		case CPUINFO_INT_REGISTER + M68K_A3:			info->i = m68k_get_reg(NULL, M68K_REG_A3); break;
		case CPUINFO_INT_REGISTER + M68K_A4:			info->i = m68k_get_reg(NULL, M68K_REG_A4); break;
		case CPUINFO_INT_REGISTER + M68K_A5:			info->i = m68k_get_reg(NULL, M68K_REG_A5); break;
		case CPUINFO_INT_REGISTER + M68K_A6:			info->i = m68k_get_reg(NULL, M68K_REG_A6); break;
		case CPUINFO_INT_REGISTER + M68K_A7:			info->i = m68k_get_reg(NULL, M68K_REG_A7); break;
		case CPUINFO_INT_REGISTER + M68K_PREF_ADDR:		info->i = m68k_get_reg(NULL, M68K_REG_PREF_ADDR); break;
		case CPUINFO_INT_REGISTER + M68K_PREF_DATA:		info->i = m68k_get_reg(NULL, M68K_REG_PREF_DATA); break;
		case CPUINFO_INT_REGISTER + M68K_MSP:			info->i = m68k_get_reg(NULL, M68K_REG_MSP); /* 68020+ */ break;
		case CPUINFO_INT_REGISTER + M68K_CACR: 			info->i = m68k_get_reg(NULL, M68K_REG_CACR); /* 68020+ */ break;
		case CPUINFO_INT_REGISTER + M68K_CAAR: 			info->i = m68k_get_reg(NULL, M68K_REG_CAAR); /* 68020+ */ break;
		case CPUINFO_INT_REGISTER + M68K_VBR:			info->i = m68k_get_reg(NULL, M68K_REG_VBR); /* 68010+ */ break;
		case CPUINFO_INT_REGISTER + M68K_SFC:			info->i = m68k_get_reg(NULL, M68K_REG_SFC); /* 68010" */ break;
		case CPUINFO_INT_REGISTER + M68K_DFC:			info->i = m68k_get_reg(NULL, M68K_REG_DFC); /* 68010+ */ break;

		/* --- the following bits of info are returned as pointers to data or functions --- */
		case CPUINFO_PTR_SET_INFO:						info->setinfo = m68020_set_info;		break;
		case CPUINFO_PTR_GET_CONTEXT:					info->getcontext = m68020_get_context;	break;
		case CPUINFO_PTR_SET_CONTEXT:					info->setcontext = m68020_set_context;	break;
		case CPUINFO_PTR_INIT:							info->init = m68020_init;				break;
		case CPUINFO_PTR_RESET:							info->reset = m68020_reset;				break;
		case CPUINFO_PTR_EXIT:							info->exit = m68020_exit;				break;
		case CPUINFO_PTR_EXECUTE:						info->execute = m68020_execute;			break;
		case CPUINFO_PTR_BURN:							info->burn = NULL;						break;
		case CPUINFO_PTR_DISASSEMBLE:					info->disassemble = m68020_dasm;		break;
		case CPUINFO_PTR_IRQ_CALLBACK:					/* fix me */							break;
		case CPUINFO_PTR_INSTRUCTION_COUNTER:			info->icount = &m68k_ICount;			break;
		case CPUINFO_PTR_REGISTER_LAYOUT:				info->p = m68020_reg_layout;			break;
		case CPUINFO_PTR_WINDOW_LAYOUT:					info->p = m68020_win_layout;			break;

		/* --- the following bits of info are returned as NULL-terminated strings --- */
		case CPUINFO_STR_NAME:							strcpy(info->s = cpuintrf_temp_str(), "68020"); break;
		case CPUINFO_STR_CORE_FAMILY:					strcpy(info->s = cpuintrf_temp_str(), "Motorola 68K"); break;
		case CPUINFO_STR_CORE_VERSION:					strcpy(info->s = cpuintrf_temp_str(), "3.2"); break;
		case CPUINFO_STR_CORE_FILE:						strcpy(info->s = cpuintrf_temp_str(), __FILE__); break;
		case CPUINFO_STR_CORE_CREDITS:					strcpy(info->s = cpuintrf_temp_str(), "Copyright 1999-2000 Karl Stenerud. All rights reserved. (2.1 fixes HJB)"); break;

		case CPUINFO_STR_FLAGS:
			sr = m68k_get_reg(NULL, M68K_REG_SR);
			sprintf(info->s = cpuintrf_temp_str(), "%c%c%c%c%c%c%c%c%c%c%c%c%c%c%c%c",
				sr & 0x8000 ? 'T':'.',
				sr & 0x4000 ? 't':'.',
				sr & 0x2000 ? 'S':'.',
				sr & 0x1000 ? 'M':'.',
				sr & 0x0800 ? '?':'.',
				sr & 0x0400 ? 'I':'.',
				sr & 0x0200 ? 'I':'.',
				sr & 0x0100 ? 'I':'.',
				sr & 0x0080 ? '?':'.',
				sr & 0x0040 ? '?':'.',
				sr & 0x0020 ? '?':'.',
				sr & 0x0010 ? 'X':'.',
				sr & 0x0008 ? 'N':'.',
				sr & 0x0004 ? 'Z':'.',
				sr & 0x0002 ? 'V':'.',
				sr & 0x0001 ? 'C':'.');
			break;

		case CPUINFO_STR_REGISTER + M68K_PC:			sprintf(info->s = cpuintrf_temp_str(), "PC :%08X", m68k_get_reg(NULL, M68K_REG_PC)); break;
		case CPUINFO_STR_REGISTER + M68K_SR:  			sprintf(info->s = cpuintrf_temp_str(), "SR :%04X", m68k_get_reg(NULL, M68K_REG_SR)); break;
		case CPUINFO_STR_REGISTER + M68K_SP:  			sprintf(info->s = cpuintrf_temp_str(), "SP :%08X", m68k_get_reg(NULL, M68K_REG_SP)); break;
		case CPUINFO_STR_REGISTER + M68K_ISP: 			sprintf(info->s = cpuintrf_temp_str(), "ISP:%08X", m68k_get_reg(NULL, M68K_REG_ISP)); break;
		case CPUINFO_STR_REGISTER + M68K_USP: 			sprintf(info->s = cpuintrf_temp_str(), "USP:%08X", m68k_get_reg(NULL, M68K_REG_USP)); break;
		case CPUINFO_STR_REGISTER + M68K_D0:			sprintf(info->s = cpuintrf_temp_str(), "D0 :%08X", m68k_get_reg(NULL, M68K_REG_D0)); break;
		case CPUINFO_STR_REGISTER + M68K_D1:			sprintf(info->s = cpuintrf_temp_str(), "D1 :%08X", m68k_get_reg(NULL, M68K_REG_D1)); break;
		case CPUINFO_STR_REGISTER + M68K_D2:			sprintf(info->s = cpuintrf_temp_str(), "D2 :%08X", m68k_get_reg(NULL, M68K_REG_D2)); break;
		case CPUINFO_STR_REGISTER + M68K_D3:			sprintf(info->s = cpuintrf_temp_str(), "D3 :%08X", m68k_get_reg(NULL, M68K_REG_D3)); break;
		case CPUINFO_STR_REGISTER + M68K_D4:			sprintf(info->s = cpuintrf_temp_str(), "D4 :%08X", m68k_get_reg(NULL, M68K_REG_D4)); break;
		case CPUINFO_STR_REGISTER + M68K_D5:			sprintf(info->s = cpuintrf_temp_str(), "D5 :%08X", m68k_get_reg(NULL, M68K_REG_D5)); break;
		case CPUINFO_STR_REGISTER + M68K_D6:			sprintf(info->s = cpuintrf_temp_str(), "D6 :%08X", m68k_get_reg(NULL, M68K_REG_D6)); break;
		case CPUINFO_STR_REGISTER + M68K_D7:			sprintf(info->s = cpuintrf_temp_str(), "D7 :%08X", m68k_get_reg(NULL, M68K_REG_D7)); break;
		case CPUINFO_STR_REGISTER + M68K_A0:			sprintf(info->s = cpuintrf_temp_str(), "A0 :%08X", m68k_get_reg(NULL, M68K_REG_A0)); break;
		case CPUINFO_STR_REGISTER + M68K_A1:			sprintf(info->s = cpuintrf_temp_str(), "A1 :%08X", m68k_get_reg(NULL, M68K_REG_A1)); break;
		case CPUINFO_STR_REGISTER + M68K_A2:			sprintf(info->s = cpuintrf_temp_str(), "A2 :%08X", m68k_get_reg(NULL, M68K_REG_A2)); break;
		case CPUINFO_STR_REGISTER + M68K_A3:			sprintf(info->s = cpuintrf_temp_str(), "A3 :%08X", m68k_get_reg(NULL, M68K_REG_A3)); break;
		case CPUINFO_STR_REGISTER + M68K_A4:			sprintf(info->s = cpuintrf_temp_str(), "A4 :%08X", m68k_get_reg(NULL, M68K_REG_A4)); break;
		case CPUINFO_STR_REGISTER + M68K_A5:			sprintf(info->s = cpuintrf_temp_str(), "A5 :%08X", m68k_get_reg(NULL, M68K_REG_A5)); break;
		case CPUINFO_STR_REGISTER + M68K_A6:			sprintf(info->s = cpuintrf_temp_str(), "A6 :%08X", m68k_get_reg(NULL, M68K_REG_A6)); break;
		case CPUINFO_STR_REGISTER + M68K_A7:			sprintf(info->s = cpuintrf_temp_str(), "A7 :%08X", m68k_get_reg(NULL, M68K_REG_A7)); break;
		case CPUINFO_STR_REGISTER + M68K_PREF_ADDR:		sprintf(info->s = cpuintrf_temp_str(), "PAR:%08X", m68k_get_reg(NULL, M68K_REG_PREF_ADDR)); break;
		case CPUINFO_STR_REGISTER + M68K_PREF_DATA:		sprintf(info->s = cpuintrf_temp_str(), "PDA:%08X", m68k_get_reg(NULL, M68K_REG_PREF_DATA)); break;
		case CPUINFO_STR_REGISTER + M68K_MSP:			sprintf(info->s = cpuintrf_temp_str(), "MSP:%08X", m68k_get_reg(NULL, M68K_REG_MSP)); break;
		case CPUINFO_STR_REGISTER + M68K_CACR:			sprintf(info->s = cpuintrf_temp_str(), "CCR:%08X", m68k_get_reg(NULL, M68K_REG_CACR)); break;
		case CPUINFO_STR_REGISTER + M68K_CAAR:			sprintf(info->s = cpuintrf_temp_str(), "CAR:%08X", m68k_get_reg(NULL, M68K_REG_CAAR)); break;
		case CPUINFO_STR_REGISTER + M68K_SFC:			sprintf(info->s = cpuintrf_temp_str(), "SFC:%X",   m68k_get_reg(NULL, M68K_REG_SFC)); break;
		case CPUINFO_STR_REGISTER + M68K_DFC:			sprintf(info->s = cpuintrf_temp_str(), "DFC:%X",   m68k_get_reg(NULL, M68K_REG_DFC)); break;
		case CPUINFO_STR_REGISTER + M68K_VBR:			sprintf(info->s = cpuintrf_temp_str(), "VBR:%08X", m68k_get_reg(NULL, M68K_REG_VBR)); break;
	}
}


#if (HAS_M68EC020)
/**************************************************************************
 * CPU-specific set_info
 **************************************************************************/

static void m68ec020_set_info(UINT32 state, union cpuinfo *info)
{
	switch (state)
	{
		/* --- the following bits of info are set as 64-bit signed integers --- */
		case CPUINFO_INT_PC:  							m68k_set_reg(M68K_REG_PC, info->i&0x00ffffff); break;

		/* --- the following bits of info are set as pointers to data or functions --- */

		default:
			m68020_set_info(state, info);
			break;
	}
}

void m68ec020_get_info(UINT32 state, union cpuinfo *info)
{
	switch (state)
	{
		/* --- the following bits of info are returned as 64-bit signed integers --- */
		case CPUINFO_INT_ADDRBUS_WIDTH + ADDRESS_SPACE_PROGRAM: info->i = 24;					break;
		case CPUINFO_INT_REGISTER + REG_PC:				info->i = m68k_get_reg(NULL, M68K_REG_PC)&0x00ffffff; break;

		/* --- the following bits of info are returned as pointers to data or functions --- */
		case CPUINFO_PTR_SET_INFO:						info->setinfo = m68ec020_set_info;		break;
		case CPUINFO_PTR_INIT:							info->init = m68ec020_init;				break;
		case CPUINFO_PTR_DISASSEMBLE:					info->disassemble = m68ec020_dasm;		break;

		/* --- the following bits of info are returned as NULL-terminated strings --- */
		case CPUINFO_STR_NAME:							strcpy(info->s = cpuintrf_temp_str(), "68EC020"); break;

		default:
			m68020_get_info(state, info);
			break;
	}
}
#endif

#endif
