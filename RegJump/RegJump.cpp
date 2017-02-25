#include "stdafx.h"
#include "resource.h"
#include <strsafe.h>
#pragma comment(lib, "strsafe.lib")
#include <Shlwapi.h>
#pragma comment(lib, "Shlwapi.lib")
#include <Commctrl.h>
#pragma comment(lib, "Comctl32.lib")


HRESULT WriteJumpKey(LPWSTR wszKey)
{
	if (wszKey==NULL) return E_INVALIDARG;

	//platform switch
	OSVERSIONINFO osvi;
	osvi.dwOSVersionInfoSize=sizeof(OSVERSIONINFO);
	if (!GetVersionEx(&osvi)) return E_FAIL;
	int bVista=(osvi.dwMajorVersion > 5) ? 3 : 0;//switch between "C[3]omputer" and "M[0]y Computer" root name

	size_t cch=0;
	HRESULT hr=StringCchLength(wszKey, STRSAFE_MAX_CCH, &cch);
	if FAILED(hr) return hr;
	if (cch<3) return E_INVALIDARG;//needs at least root key short-syntax name (HKU, etc.)

	//skip to valid first char (hk.., hkey...)
	WCHAR* pend=wszKey+cch;
	while ((wszKey<pend) && ((*wszKey!=L'h') && (*wszKey!=L'H'))) {wszKey++;cch--;}
	if (cch==0) return E_INVALIDARG;

	//check key name component limit (255)
	int knlen=0;
	WCHAR *psrc=wszKey;
	while(*psrc && (psrc < pend))//walk the string
	{
		if (*psrc==L'\\') 
			knlen=0;
		else knlen++;
		if (knlen>255)
			return E_INVALIDARG;
		psrc++;
	}

	//expand abbreviations
	LPCWSTR pwszRootKey=NULL;
	LPCWSTR pwszSubKey=NULL;
	int rklen=0;
	if (wszKey[4]==L'_') //quick branch
	{
		if (StrIsIntlEqual(FALSE, wszKey, L"HKEY_CLASSES_ROOT", 17))   rklen=12;
		if (StrIsIntlEqual(FALSE, wszKey, L"HKEY_CURRENT_USER", 17))   rklen=12;
		if (StrIsIntlEqual(FALSE, wszKey, L"HKEY_LOCAL_MACHINE", 18))  rklen=12;
		if (StrIsIntlEqual(FALSE, wszKey, L"HKEY_USERS", 10))          rklen=12;
		if (StrIsIntlEqual(FALSE, wszKey, L"HKEY_CURRENT_CONFIG", 19)) rklen=12;
		if (rklen) { pwszRootKey=L"My Computer\\"; pwszSubKey=wszKey; } 
	}
	else
	{
		if (StrIsIntlEqual(FALSE, wszKey, L"HKCR", 4)) { pwszRootKey=L"My Computer\\HKEY_CLASSES_ROOT"; rklen=29; pwszSubKey=wszKey+4; }
		if (StrIsIntlEqual(FALSE, wszKey, L"HKCU", 4)) { pwszRootKey=L"My Computer\\HKEY_CURRENT_USER"; rklen=29; pwszSubKey=wszKey+4; }
		if (StrIsIntlEqual(FALSE, wszKey, L"HKLM", 4)) { pwszRootKey=L"My Computer\\HKEY_LOCAL_MACHINE"; rklen=30; pwszSubKey=wszKey+4; }
		if (StrIsIntlEqual(FALSE, wszKey, L"HKU",  3)) { pwszRootKey=L"My Computer\\HKEY_USERS"; rklen=22; pwszSubKey=wszKey+3; }
		if (StrIsIntlEqual(FALSE, wszKey, L"HKCC", 4)) { pwszRootKey=L"My Computer\\HKEY_CURRENT_CONFIG"; rklen=31; pwszSubKey=wszKey+4; }
	}
	if (rklen==0) return E_INVALIDARG;


	size_t chKeyLen=cch+rklen;
	WCHAR* pKey=(WCHAR*)HeapAlloc(GetProcessHeap(), HEAP_ZERO_MEMORY, sizeof(WCHAR)*(chKeyLen+1));
	if (pKey)
	{
		if (S_OK==StringCchCopy(pKey, rklen+1-bVista, pwszRootKey+bVista))
		{
			//copy string
			PWSTR pdest=pKey+rklen-bVista;
			PCWSTR pend=wszKey+cch;

			while (*(pend-1)==L' ') pend--;//remove trailing spaces

			PCWSTR psrc=pwszSubKey;
			while ((*psrc && (psrc<pend)) && (pdest<(pKey+chKeyLen)))
			{
				while(*psrc==L'\\' && *(psrc+1)==L'\\') psrc++;//remove multiple backslashes (eg. from reg file)
				if (*psrc==L'\\' && *(psrc+1)==L'\0') break;//remove trailing backslashes
				*pdest=*psrc;
				pdest++;
				psrc++;
			}
		}


#ifdef _DEBUG
MessageBox(NULL, pKey, L"regjump__LastKey", MB_OK);
#endif
		//write jump key
		if (S_OK==hr)
		{
			LSTATUS ls=SHSetValue(HKEY_CURRENT_USER,
									L"Software\\Microsoft\\Windows\\CurrentVersion\\Applets\\Regedit",
									L"Lastkey", REG_SZ, (LPCVOID)pKey, (DWORD)(chKeyLen+32-bVista)*sizeof(WCHAR));
			hr=HRESULT_FROM_WIN32(ls);
		}


		HeapFree(GetProcessHeap(), 0, pKey);
	}
	else return E_OUTOFMEMORY;


return hr;
}





//main
#ifdef _ATL_MIN_CRT //release build
int WINAPI WinMainCRTStartup(void)//no crt
#else
int APIENTRY _tWinMain(HINSTANCE hInstance, HINSTANCE hPrevInstance, LPTSTR lpCmdLine, int nCmdShow)
#endif
{
	HRESULT hret=E_FAIL;
	int argc=0;
	LPWSTR *szArglist=CommandLineToArgvW(GetCommandLineW(), &argc);
	if (szArglist==NULL) return 0;
	if (argc<2)
	{
		//get clipboard text
		if (OpenClipboard(NULL))
		{
			HANDLE hclStr=::GetClipboardData(CF_UNICODETEXT);//clipboard owns this handle
			if (hclStr)
			{
				LPWSTR pwStr=(WCHAR*)GlobalLock(hclStr);
				if (pwStr) hret=WriteJumpKey(pwStr);
				GlobalUnlock(hclStr);//never forget
			}
			::CloseClipboard();
		}

	}
	else
	{
		hret=WriteJumpKey(szArglist[1]);
	}
	LocalFree(szArglist);


	if (S_OK==hret) ShellExecute(NULL, L"open", L"regedit.exe", NULL, NULL, SW_SHOWNORMAL);
	else MessageBox(NULL, L"No valid registry key specified.", L"RegJump",MB_OK | MB_ICONWARNING);


#ifdef _ATL_MIN_CRT
	ExitProcess(0);
#endif
return 0;
}

