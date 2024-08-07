/* $Revision: 14420 $ $Date: 2014-01-28 17:04:37 -0800 (Tue, 28 Jan 2014) $
 * Copyright (c) Bullseye Testing Technology
 * This source file contains confidential proprietary information.
 *
 * String functions.
 */

static TCHAR* localStrcpy(TCHAR* s1, const TCHAR* s2)
{
	unsigned i = 0;
	do {
		s1[i] = s2[i];
	} while (s2[i++] != TEXT('\0'));
	return s1;
}

static unsigned localStrlen(const TCHAR* s)
{
	unsigned n;
	for (n = 0; s[n] != TEXT('\0'); n++)
		;
	return n;
}

static TCHAR* localStrcat(TCHAR* s1, const TCHAR* s2)
{
	localStrcpy(s1 + localStrlen(s1), s2);
	return s1;
}

