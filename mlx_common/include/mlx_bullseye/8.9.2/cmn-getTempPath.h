// $Revision: 14420 $ $Date: 2014-01-28 17:04:37 -0800 (Tue, 28 Jan 2014) $
// Copyright (c) Bullseye Testing Technology
// This source file contains confidential proprietary information.
//
// Interface to Win32 GetTempPathW. GetTempPathA is not available on Windows Phone 8.1

#if UNICODE
	#define getTempPath GetTempPathW
#else
	static DWORD getTempPath(DWORD size, char* buf)
	{
		wchar_t wbuf[MAX_PATH + 1];
		DWORD length = GetTempPathW(MAX_PATH + 1, wbuf);
		if (length != 0) {
			length = WideCharToMultiByte(CP_ACP, 0, wbuf, length, buf, size, NULL, NULL);
			buf[length] = '\0';
		}
		return length;
	}
#endif
