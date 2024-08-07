/* $Revision: 14455 $ $Date: 2014-02-10 17:32:46 -0800 (Mon, 10 Feb 2014) $
// Copyright (c) Bullseye Testing Technology
// This source file contains confidential proprietary information.
//
// BullseyeCoverage run-time for Unix/Linux kernel.
// This configuration requires a covgetkernel program to save measurements to file.
*/

#include "../mlx_bullseye/version.h"
static const char whatString[] = "@(#)" VERSION_title;
static const char Copyright[] = VERSION_copyright;

/* One CovObject structure per entry */
/*   HP-UX kernel object count 2650, Apr 2004 */
/*   Redefine cov_array so FreeBSD puller does not find it */
#define cov_array cov_array1
static const CovObject* volatile cov_array[4096];
const CovKernel cov_kernel = { Libcov_version, sizeof(CovObject*), cov_array };
static cov_atomic_t cov_list_lock = cov_atomic_initializer;
static short error;

static void error_report(short errorNumber)
{
	if (error == 0) {
		error = errorNumber;
	}
}

void Libcov_cdecl cov_countDown(void)
{
}

void Libcov_cdecl cov_countUp(void)
{
}

/* The FreeBSD 7.1 kernel calls execve */
void Libcov_cdecl cov_term(void)
{
}

static void cov_probe_init(const CovObject* p)
{
	/* Search for empty slot, or one already pointing to this object
	 *   (due to module unload/reload at same address) */
	unsigned i = sizeof(cov_array) / sizeof(cov_array[0]);
	do {
		i--;
	} while (i != 0 && cov_array[i] != NULL && cov_array[i] != p);
	if (i != 0) {
		if (cov_atomic_tryLock(&cov_list_lock)) {
			if ((cov_array[i] == NULL || cov_array[i] == p) && !p->var->is_linked) {
				p->var->is_linked = 1;
				cov_array[i] = p;
			}
			cov_atomic_unlock(&cov_list_lock);
		}
	}
}

void Libcov_cdecl Libcov_probe(const void* p_void, int probe)
{
	const CovObject* p = (const CovObject*)p_void;
	if (p->signature == CovObject_signature && (unsigned)probe < p->data_n) {
		CovVar* var = p->var;
		var->data[probe] = 1;
		if (var->is_linked == 0) {
			cov_probe_init(p);
		} else if (var->is_linked > 1) {
			/* Memory corrupt in .bss */
			error_report(23);
		}
	} else {
		/* Memory corrupt in .const */
		error_report(24);
	}
}

static void cov_reset_1(const CovObject* p)
{
	unsigned i;
	for (i = 0; i < p->data_n; i++) {
		p->var->data[i] = 0;
	}
	p->var->events_written = 0;
}

Libcov_export int Libcov_cdecl cov_reset(void)
{
	const int status = cov_check();
	if (status < 0) {
		unsigned i;
		/* Spin on the cov_list lock.  We cannot skip the operation if
		 * cov_list is busy, like we can elsewhere.
		 */
		while (!cov_atomic_tryLock(&cov_list_lock))
			;
		for (i = sizeof(cov_array) / sizeof(cov_array[0]) - 1; i != 0; i--) {
			const CovObject* p = cov_array[i];
			if (p != NULL) {
				cov_reset_1(p);
				/* Do not bother resetting cov_array[i], there is no point */
			}
		}
		cov_atomic_unlock(&cov_list_lock);
	}
	return status;
}

static int cov_check_1(const CovObject* p)
{
	int success = 1;
	if (p->signature != CovObject_signature || p->data_n == 0) {
		/* Memory corrupt in .const */
		error_report(24);
		success = 0;
	} else {
		unsigned i;
		const CovVar* var = p->var;
		for (i = 0; i < p->data_n && var->data[i] <= 1; i++) {
		}
		if (i < p->data_n || var->is_linked > 1 || var->is_found > 1) {
			/* Memory corrupt in .bss */
			error_report(23);
			success = 0;
		}
	}
	return success;
}

Libcov_export int Libcov_cdecl cov_check(void)
{
	/* Do not lock cov_list_lock because this function does not modify cov_list */
	int status;
	unsigned count = 0;
	unsigned i;
	for (i = sizeof(cov_array) / sizeof(cov_array[0]) - 1; i != 0; i--) {
		const CovObject* p = cov_array[i];
		if (p != NULL) {
			if (!cov_check_1(p)) {
				break;
			}
			count++;
		}
	}
	status = error;
	if (status == 0) {
		status = -(int)count;
	}
	return status;
}
