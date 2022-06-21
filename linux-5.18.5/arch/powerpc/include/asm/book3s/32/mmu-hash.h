/* SPDX-License-Identifier: GPL-2.0 */
#ifndef _ASM_POWERPC_BOOK3S_32_MMU_HASH_H_
#define _ASM_POWERPC_BOOK3S_32_MMU_HASH_H_

/*
 * 32-bit hash table MMU support
 */

/*
 * BATs
 */

/* Block size masks */
#define BL_128K	0x000
#define BL_256K 0x001
#define BL_512K 0x003
#define BL_1M   0x007
#define BL_2M   0x00F
#define BL_4M   0x01F
#define BL_8M   0x03F
#define BL_16M  0x07F
#define BL_32M  0x0FF
#define BL_64M  0x1FF
#define BL_128M 0x3FF
#define BL_256M 0x7FF

/* BAT Access Protection */
#define BPP_XX	0x00		/* No access */
#define BPP_RX	0x01		/* Read only */
#define BPP_RW	0x02		/* Read/write */

#ifndef __ASSEMBLY__
/* Contort a phys_addr_t into the right format/bits for a BAT */
#ifdef CONFIG_PHYS_64BIT
#define BAT_PHYS_ADDR(x) ((u32)((x & 0x00000000fffe0000ULL) | \
				((x & 0x0000000e00000000ULL) >> 24) | \
				((x & 0x0000000100000000ULL) >> 30)))
#define PHYS_BAT_ADDR(x) (((u64)(x) & 0x00000000fffe0000ULL) | \
			  (((u64)(x) << 24) & 0x0000000e00000000ULL) | \
			  (((u64)(x) << 30) & 0x0000000100000000ULL))
#else
#define BAT_PHYS_ADDR(x) (x)
#define PHYS_BAT_ADDR(x) ((x) & 0xfffe0000)
#endif

struct ppc_bat {
	u32 batu;
	u32 batl;
};
#endif /* !__ASSEMBLY__ */

/*
 * Hash table
 */

/* Values for PP (assumes Ks=0, Kp=1) */
#define PP_RWXX	0	/* Supervisor read/write, User none */
#define PP_RWRX 1	/* Supervisor read/write, User read */
#define PP_RWRW 2	/* Supervisor read/write, User read/write */
#define PP_RXRX 3	/* Supervisor read,       User read */

/* Values for Segment Registers */
#define SR_NX	0x10000000	/* No Execute */
#define SR_KP	0x20000000	/* User key */
#define SR_KS	0x40000000	/* Supervisor key */

#ifdef __ASSEMBLY__

#include <asm/asm-offsets.h>

.macro uus_addi sr reg1 reg2 imm
	.if NUM_USER_SEGMENTS > \sr
	addi	\reg1,\reg2,\imm
	.endif
.endm

.macro uus_mtsr sr reg1
	.if NUM_USER_SEGMENTS > \sr
	mtsr	\sr, \reg1
	.endif
.endm

/*
 * This isync() shouldn't be necessary as the kernel is not excepted to run
 * any instruction in userspace soon after the update of segments and 'rfi'
 * instruction is used to return to userspace, but hash based cores
 * (at least G3) seem to exhibit a random behaviour when the 'isync' is not
 * there. 603 cores don't have this behaviour so don't do the 'isync' as it
 * saves several CPU cycles.
 */
.macro uus_isync
#ifdef CONFIG_PPC_BOOK3S_604
BEGIN_MMU_FTR_SECTION
	isync
END_MMU_FTR_SECTION_IFSET(MMU_FTR_HPTE_TABLE)
#endif
.endm

.macro update_user_segments_by_4 tmp1 tmp2 tmp3 tmp4
	uus_addi	1, \tmp2, \tmp1, 0x111
	uus_addi	2, \tmp3, \tmp1, 0x222
	uus_addi	3, \tmp4, \tmp1, 0x333

	uus_mtsr	0, \tmp1
	uus_mtsr	1, \tmp2
	uus_mtsr	2, \tmp3
	uus_mtsr	3, \tmp4

	uus_addi	4, \tmp1, \tmp1, 0x444
	uus_addi	5, \tmp2, \tmp2, 0x444
	uus_addi	6, \tmp3, \tmp3, 0x444
	uus_addi	7, \tmp4, \tmp4, 0x444

	uus_mtsr	4, \tmp1
	uus_mtsr	5, \tmp2
	uus_mtsr	6, \tmp3
	uus_mtsr	7, \tmp4

	uus_addi	8, \tmp1, \tmp1, 0x444
	uus_addi	9, \tmp2, \tmp2, 0x444
	uus_addi	10, \tmp3, \tmp3, 0x444
	uus_addi	11, \tmp4, \tmp4, 0x444

	uus_mtsr	8, \tmp1
	uus_mtsr	9, \tmp2
	uus_mtsr	10, \tmp3
	uus_mtsr	11, \tmp4

	uus_addi	12, \tmp1, \tmp1, 0x444
	uus_addi	13, \tmp2, \tmp2, 0x444
	uus_addi	14, \tmp3, \tmp3, 0x444
	uus_addi	15, \tmp4, \tmp4, 0x444

	uus_mtsr	12, \tmp1
	uus_mtsr	13, \tmp2
	uus_mtsr	14, \tmp3
	uus_mtsr	15, \tmp4

	uus_isync
.endm

#else

/*
 * This macro defines the mapping from contexts to VSIDs (virtual
 * segment IDs).  We use a skew on both the context and the high 4 bits
 * of the 32-bit virtual address (the "effective segment ID") in order
 * to spread out the entries in the MMU hash table.  Note, if this
 * function is changed then hash functions will have to be
 * changed to correspond.
 */
#define CTX_TO_VSID(c, id)	((((c) * (897 * 16)) + (id * 0x111)) & 0xffffff)

/*
 * Hardware Page Table Entry
 * Note that the xpn and x bitfields are used only by processors that
 * support extended addressing; otherwise, those bits are reserved.
 */
struct hash_pte {
	unsigned long v:1;	/* Entry is valid */
	unsigned long vsid:24;	/* Virtual segment identifier */
	unsigned long h:1;	/* Hash algorithm indicator */
	unsigned long api:6;	/* Abbreviated page index */
	unsigned long rpn:20;	/* Real (physical) page number */
	unsigned long xpn:3;	/* Real page number bits 0-2, optional */
	unsigned long r:1;	/* Referenced */
	unsigned long c:1;	/* Changed */
	unsigned long w:1;	/* Write-thru cache mode */
	unsigned long i:1;	/* Cache inhibited */
	unsigned long m:1;	/* Memory coherence */
	unsigned long g:1;	/* Guarded */
	unsigned long x:1;	/* Real page number bit 3, optional */
	unsigned long pp:2;	/* Page protection */
};

typedef struct {
	unsigned long id;
	unsigned long sr0;
	void __user *vdso;
} mm_context_t;

#ifdef CONFIG_PPC_KUEP
#define INIT_MM_CONTEXT(mm) .context.sr0 = SR_NX
#endif

void update_bats(void);
static inline void cleanup_cpu_mmu_context(void) { }

/* patch sites */
extern s32 patch__hash_page_A0, patch__hash_page_A1, patch__hash_page_A2;
extern s32 patch__hash_page_B, patch__hash_page_C;
extern s32 patch__flush_hash_A0, patch__flush_hash_A1, patch__flush_hash_A2;
extern s32 patch__flush_hash_B;

#include <asm/reg.h>
#include <asm/task_size_32.h>

static __always_inline void update_user_segment(u32 n, u32 val)
{
	if (n << 28 < TASK_SIZE)
		mtsr(val + n * 0x111, n << 28);
}

static __always_inline void update_user_segments(u32 val)
{
	val &= 0xf0ffffff;

	update_user_segment(0, val);
	update_user_segment(1, val);
	update_user_segment(2, val);
	update_user_segment(3, val);
	update_user_segment(4, val);
	update_user_segment(5, val);
	update_user_segment(6, val);
	update_user_segment(7, val);
	update_user_segment(8, val);
	update_user_segment(9, val);
	update_user_segment(10, val);
	update_user_segment(11, val);
	update_user_segment(12, val);
	update_user_segment(13, val);
	update_user_segment(14, val);
	update_user_segment(15, val);
}

int __init find_free_bat(void);
unsigned int bat_block_size(unsigned long base, unsigned long top);
#endif /* !__ASSEMBLY__ */

/* We happily ignore the smaller BATs on 601, we don't actually use
 * those definitions on hash32 at the moment anyway
 */
#define mmu_virtual_psize	MMU_PAGE_4K
#define mmu_linear_psize	MMU_PAGE_256M

#endif /* _ASM_POWERPC_BOOK3S_32_MMU_HASH_H_ */
