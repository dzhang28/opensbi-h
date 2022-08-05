#include <sbi/sbi_page_fault.h>
#include <sbi/riscv_encoding.h>
#include <sbi/sbi_error.h>
#include <sbi/sbi_trap.h>
#include <sbi/sbi_ptw.h>
#include <sbi/sbi_hext.h>
#include <sbi/sbi_console.h>

static inline sbi_pte_t cause_to_access(unsigned long cause)
{
	switch (cause) {
	case CAUSE_LOAD_PAGE_FAULT:
		return PTE_R;
	case CAUSE_STORE_PAGE_FAULT:
		return PTE_W;
	case CAUSE_FETCH_PAGE_FAULT:
		return PTE_X;
	default:
		return 0;
	}
}

int sbi_page_fault_handler(ulong tval, ulong cause, struct sbi_trap_regs *regs)
{
	int ret;
	struct hext_state *hext = sbi_hext_current_state();
	struct sbi_ptw_csr csr = { .hgatp = hext->hgatp, .vsatp = hext->vsatp };
	struct sbi_ptw_out out;
	struct sbi_trap_info trap;
	sbi_pte_t access = cause_to_access(cause);

	sbi_printf("%s: page fault 0x%lx cause %d at pc=0x%lx\n", __func__,
		   tval, (int)cause, regs->mepc);
	ret = sbi_ptw_translate(tval, &csr, &out, &trap);

	if (ret) {
		trap.cause = convert_access_type(trap.cause, cause);
		return sbi_trap_redirect(regs, &trap);
	}

	if ((access & out.prot) != access) {
		trap.cause = cause;
		trap.tval  = tval;
		trap.tval2 = 0;
		trap.tinst = 0;
		return sbi_trap_redirect(regs, &trap);
	}

	sbi_pt_map(tval, &out, &hext->pt_area);
	asm volatile("sfence.vma" ::: "memory");

	return SBI_OK;
}
