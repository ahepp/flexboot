/* $Revision: 15176 $ $Date: 2014-10-30 16:21:48 -0700 (Thu, 30 Oct 2014) $
// Copyright (c) Bullseye Testing Technology
// This source file contains confidential proprietary information.
//
// BullseyeCoverage small footprint run-time for embedded systems core definitions
*/

#if __cplusplus
extern "C" {
#endif

#if !defined(Libcov_prefix)
	#define Libcov_prefix 
#endif

static unsigned short constructorCount;
static short error;
static const char title[] = VERSION_title ": ";
static const CovObject* volatile cov_list;
static cov_atomic_t cov_list_lock = cov_atomic_initializer;

static void stringCopy(char* buf, unsigned* index, const char* string)
{
	unsigned i;
	unsigned j = *index;
	for (i = 0; string[i] != '\0'; i++) {
		buf[j++] = string[i];
	}
	*index = j;
}

static void error_report(short errorNumber, const char* arg)
{
	char buf[128];
	unsigned i = 0;
	const char* string;
	error = errorNumber;
	switch (error) {
	case 5:  string = "error 5: i/o error"; break;
	case 23: string = "error 23: memory corrupt in .bss"; break;
	case 24: string = "error 24: memory corrupt in .const"; break;
	case 25: string = "error 25: internal error"; break;
	case 26: string = "error 26: cannot open file"; break;
	default: string = "error unknown"; break;
	}
	stringCopy(buf, &i, title);
	stringCopy(buf, &i, string);
	if (arg != NULL) {
		stringCopy(buf, &i, ", ");
		stringCopy(buf, &i, arg);
	}
	stringCopy(buf, &i, "\n");
	buf[i] = '\0';
	mlx_bullseye_write(2, buf, i);
}

int cov_check(void)
{
	int status;
	unsigned count = 0;
	const CovObject* p;
	/* Do not lock cov_list_lock because this function does not modify cov_list */
	for (p = cov_list; error == 0 && p != NULL; p = p->var->next) {
		if (p->signature != CovObject_signature || p->data_n == 0) {
			error_report(24, "check");
		} else {
			unsigned i;
			const CovVar* var = p->var;
			for (i = 0; i < p->data_n && var->data[i] <= 1; i++)
				;
			if (i < p->data_n || var->is_linked > 1 || var->is_found != 0) {
				error_report(23, "check");
			}
		}
		if (count == 0xffff) {
			error_report(25, NULL);
		}
		count++;
	}
	status = error;
	if (status == 0) {
		status = -(int)count;
	}
	return status;
}

int cov_reset(void)
{
	const int status = cov_check();
	if (status < 0) {
		/* Wait for the cov_list lock */
		while (!cov_atomic_tryLock(&cov_list_lock))
			;
		{
			const CovObject* p = cov_list;
			cov_list = NULL;
			while (p != NULL) {
				CovVar* var = p->var;
				const CovObject* next = var->next;
				unsigned i;
				for (i = 0; i < p->data_n; i++) {
					var->data[i] = 0;
				}
				var->is_linked = 0;
				var->next = NULL;
				p = next;
			}
		}
		cov_atomic_unlock(&cov_list_lock);
	}
	return status;
}

void Libcov_probe(const void* p_void, int probe)
{
	const CovObject* p = (const CovObject*)p_void;
	if (p->signature != CovObject_signature || (unsigned)probe >= p->data_n) {
		error_report(24, "probe");
	} else {
		CovVar* var = p->var;
		var->data[probe] = 1;
		if (!var->is_linked && cov_atomic_tryLock(&cov_list_lock)) {
			if (!var->is_linked) {
				var->is_linked = 1;
				var->next = cov_list;
				cov_list = p;
			} else if (var->is_linked != 1) {
				/* The likely cause of this error is the .bss section has not been reset yet */
				error_report(23, "probe");
			}
			cov_atomic_unlock(&cov_list_lock);
		}
	}
}

static void write_errorCheck(int fildes, const char* buf, unsigned nbyte)
{
	if (error == 0 && mlx_bullseye_write(fildes, buf, nbyte) != (int)nbyte) {
		error_report(5, NULL);
	}
}

/* Format a number as decimal. Result is not NUL terminated */
static unsigned formatNumber(unsigned long n, char* buf, unsigned size)
{
	unsigned length;
	/* Go backwards with rightmost digit in rightmost buffer position */
	unsigned i = size;
	unsigned j;
	do {
		i--;
		buf[i] = (char)('0' + n % 10);
		n /= 10;
	} while (n != 0);
	length = size - i;
	/* Slide results left */
	for (j = 0; j < length; j++) {
		buf[j] = buf[i++];
	}
	return length;
}

/* Write a decimal number followed by a new-line */
static void writeNumber(int fildes, unsigned long n)
{
	char buf[sizeof("18446744073709551615") + 1];
	const unsigned length = formatNumber(n, buf, sizeof(buf));
	buf[length] = '\n';
	buf[length + 1] = '\0';
	write_errorCheck(fildes, buf, length + 1);
}

/* Write all coverage data into a file to be processed by covpost */
static int dumpData(void)
{
	int status;
	if (error == 0) {
		cov_check();
	}
	if (error == 0 && cov_list != NULL) {
		/* Open output file */
		int fildes;
		static char path[sizeof(Libcov_prefix "BullseyeCoverage.data-18446744073709551615")] = Libcov_prefix "BullseyeCoverage.data-";
		unsigned i = sizeof(Libcov_prefix "BullseyeCoverage.data-") - 1;
		const unsigned pidLength = formatNumber((unsigned long)getpid(), path + i, sizeof(path) - i);
		i += pidLength;
		path[i] = '\0';
		fildes = mlx_bullseye_open(path, O_CREAT | O_TRUNC | O_WRONLY, S_IRUSR | S_IWUSR);
		if (fildes == -1) {
			error_report(26, path);
		} else {
			const CovObject* p;
			/* Write file header */
			static const char s[] = "BullseyeCoverage S2\n";
			write_errorCheck(fildes, s, sizeof(s) - 1);
			/* Write address of cov_list, to aid in diagnosing multiple data address spaces */
			write_errorCheck(fildes, "#", 1);
			#if _MSC_VER >= 1300
				/* Microsoft C++ is unusual for 64-bit CPUs. Size of long is smaller than size of pointer */
				writeNumber(fildes, (unsigned long)(unsigned long long)&cov_list);
			#else
				writeNumber(fildes, (unsigned long)&cov_list);
			#endif
			/* For each object */
			for (p = cov_list; p != NULL; p = p->var->next) {
				unsigned basenameLength;
				enum { bufSize = 64 };
				char buf[bufSize + 2];
				unsigned j;
				const CovVar* var = p->var;
				for (basenameLength = 0; p->basename[basenameLength] != '\0'; basenameLength++)
					;
				write_errorCheck(fildes, p->basename, basenameLength);
				write_errorCheck(fildes, "\n", 1);
				writeNumber(fildes, p->fileId);
				writeNumber(fildes, p->id);
				writeNumber(fildes, p->data_n);
				/* Write event data, bufSize bytes at a time then new-line */
				for (j = 0; j < p->data_n; j += bufSize) {
					const unsigned char* data = var->data + j;
					unsigned k;
					unsigned n = bufSize;
					if (n > p->data_n - j) {
						n = p->data_n - j;
					}
					for (k = 0; k < n; k++) {
						buf[k] = '0' + data[k];
					}
					buf[n] = '\n';
					buf[n + 1] = '\0';
					write_errorCheck(fildes, buf, n + 1);
				}
			}
			write_errorCheck(fildes, "end\n", 4);
			mlx_bullseye_close(fildes);
		}
	}
	status = error;
	if (error == 0 && cov_list == NULL) {
		status = 20;
	}
	return status;
}

/* Synchronize calls to dumpData by multiple threads */
int cov_dumpData(void)
{
	int status;
	static cov_atomic_t lock = cov_atomic_initializer;
	static int isMore;
	if (cov_atomic_tryLock(&lock)) {
		do {
			isMore = 0;
			status = dumpData();
		} while (status == 0 && isMore);
		cov_atomic_unlock(&lock);
	} else {
		isMore = 1;
		status = 0;
	}
	return status;
}

void cov_countDown(void)
{
	constructorCount--;
	if (constructorCount == 0) {
		cov_dumpData();
	}
}

void cov_countUp(void)
{
	constructorCount++;
}

void cov_term(void)
{
	cov_dumpData();
}

#if __cplusplus
}
#endif
