/* $Revision: 14455 $ $Date: 2014-02-10 17:32:46 -0800 (Mon, 10 Feb 2014) $
 * Copyright (c) Bullseye Testing Technology
 * This source file contains confidential proprietary information.
 */

/* Run-time library probe interface version */
#define Libcov_probe cov_probe_v12
#define Libcov_version 808014u	/* 8.8.14 */

#if PATH_MAX > 256
	/* This greatly reduces usage of BSS */
	#undef PATH_MAX
	#define PATH_MAX 256
#endif

#if __cplusplus
extern "C" {
#endif

/* Data for one translation unit */
/*   Variable data (bss section) */
typedef struct CovVar {
	const struct CovObject* next;
	unsigned events_written;
	char is_linked;
	char is_found;
	unsigned char data[1];
} CovVar;

typedef struct CovObject {
	uint32 signature;
	uint32 id;
	CovVar* var;
	uint32 fileId;
	unsigned data_n;
	char basename[1];
} CovObject;
#define CovObject_signature 0x5a6b7c8dL

/* Interface to covgetkernel */
typedef struct {
	/* Libcov_version */
	unsigned version;
	unsigned arch;
	const CovObject* volatile* array;
} CovKernel;

#if __WATCOMC__
	#define Libcov_cdecl __watcall
#elif defined(_WIN32)
	#define Libcov_cdecl __cdecl
#else
	#define Libcov_cdecl
#endif

/* Might already be defined for DLL export */
#if !defined(Libcov_export)
	#define Libcov_export
#endif

Libcov_export int Libcov_cdecl cov_check(void);
void cov_dumpFile(void);
Libcov_export unsigned Libcov_cdecl cov_eventCount(void);
Libcov_export int Libcov_cdecl cov_file(const char* path);
Libcov_export int Libcov_cdecl cov_reset(void);
void Libcov_cdecl cov_term(void);
Libcov_export int Libcov_cdecl cov_write(void);

#if __cplusplus
}
#endif
