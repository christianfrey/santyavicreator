#define _WIN32_WINNT 0x0501
#define _WIN32_IE 0x0501
#include <windows.h>
#include <commctrl.h>
#include <ocidl.h> // LPPICTURE
#include <olectl.h> // OleLoadPicturePath()
#include "AVIFile.h"
#include "resource.h"
#pragma comment(lib,"comctl32.lib")

#define ugWM_CREATEAVI (WM_USER+1)
#define ugWM_ENDTHRD	  (WM_USER+2)

char g_szAppName[] = "SantyAVICreator";
char g_szErrAvi[] = "Path pour le fichier AVI manquant!";
char g_szErrFrames[] = "Aucune frame sélectionnée!";

HINSTANCE g_hInstance;
HWND g_hList;
HWND g_hDlg;
HCURSOR g_hArrow, g_hWait;
HWND g_hInfo, g_hPg;
long g_xPos, g_yPos;
HANDLE g_hThrd = 0;

HBITMAP ImgLoad(char* pszFilePath);

//***************************************************************************************
// dlgFileOpenMulti : Boîte de dialogue pour ouvrir plusieurs images (bmp, jpg ou gif).
//***************************************************************************************
int __stdcall dlgFileOpenMulti(HWND hOwner, char *pszFiles, DWORD nSize)
{ // Retours : < 0 si pszFiles trop petit, 0 boîte annulée, sinon offset 1er fichier
	OPENFILENAME ofn;
	ZeroMemory(&ofn, sizeof(OPENFILENAME));
	ofn.lStructSize = sizeof(OPENFILENAME);
	ofn.hInstance = g_hInstance; ofn.hwndOwner = hOwner;
	ofn.lpstrFilter = "Images (*.bmp;*.jpg;*.jpeg;*.gif)\0*.bmp;*.jpg;*.jpeg;*.gif\0\0";
	ofn.lpstrFile = pszFiles; ofn.nMaxFile = nSize;
	ofn.lpstrCustomFilter = ofn.lpstrFileTitle = 0;
	ofn.nFileExtension = ofn.nFileOffset = 0;
	ofn.lCustData = ofn.dwReserved = 0;
	ofn.lpTemplateName = ofn.lpstrInitialDir = ofn.lpstrDefExt = 0;
	ofn.lpfnHook = 0; ofn.pvReserved = 0;
	ofn.nFileExtension = ofn.nFileExtension = 0;
	ofn.nMaxCustFilter = ofn.nMaxFileTitle = 0;
	ofn.nFilterIndex = 1;
	ofn.Flags = OFN_PATHMUSTEXIST | OFN_FILEMUSTEXIST | OFN_HIDEREADONLY | OFN_EXPLORER | OFN_ALLOWMULTISELECT;
	pszFiles[0] = 0;
	if(GetOpenFileName(&ofn)) return ofn.nFileOffset;
	// Erreur si buffer passé à OPENFILENAME.lpstrFile trop petit
	if(CommDlgExtendedError() == FNERR_BUFFERTOOSMALL) return -1;
	return 0;
}

//***************************************************************************************
// onSelFiles : Traîtement de la sélection multiple.
//***************************************************************************************
void __stdcall onSelFiles(HWND hDlg)
{
	int r, n;
	char *pmem, *c;
	HANDLE hheap;
	hheap = GetProcessHeap();
	n = 4096; // Si trop peu, on recommence
nextPass:
	pmem = (char*) HeapAlloc(hheap, 0, n);
	if(!pmem) goto errMemory;
	r = dlgFileOpenMulti(hDlg, pmem, n);
	if(!r) goto relMem; // Boîte annulée, on sort
	if(r > 0) goto selOK; // OK, on affiche
	// Buffer trop petit
	n = *((WORD*) pmem); // Taille requise dans les 2 premiers octets
	HeapFree(hheap, 0, pmem); // On libère
	n++;
	goto nextPass; // Nouvelle tentative
selOK:
	SendMessage(g_hList, LB_RESETCONTENT, 0, 0);
	SetDlgItemText(hDlg, IDST_DIR, 0);
	// Chaînes séparées par un zéro, 2 de suite dit fin
	// 1ère chaîne est Directory
	pmem[r-1] = 0; // Si 1 seul fichier est collé au dossier, on sépare
	SetDlgItemText(hDlg, IDST_DIR, pmem);
	c = pmem + r; // Normalement est sur 1er fichier
	n = 0;
nextFile:
	SendMessage(g_hList, LB_ADDSTRING, 0, (long) c);
	n++;
	while(*c) c++; // Pousse au prochain zéro
	c++; // 2 zéros de suite, si OUI est fini
	if(*c) goto nextFile;
relMem: HeapFree(hheap, 0, pmem);
	return;
errMemory:
	MessageBox(hDlg, "DEFAUT MEMOIRE", g_szAppName, 0x30);
}

//***************************************************************************************
// dlgWavOpen : Boîte de dialogue pour ouvrir le fichier wave.
//***************************************************************************************
void __stdcall dlgWavOpen(HWND hOwner)
{
	OPENFILENAME ofn;
	char szPath[256];
	szPath[0] = '\0';
	ZeroMemory(&ofn, sizeof(OPENFILENAME));
	ofn.lStructSize = sizeof(OPENFILENAME);
	ofn.hInstance = g_hInstance; ofn.hwndOwner = hOwner;
	ofn.lpstrFilter = "Fichier Audio (*.wav)\0*.wav\0\0";
	ofn.lpstrFile = szPath; ofn.nMaxFile = 256;
	ofn.Flags = OFN_PATHMUSTEXIST | OFN_FILEMUSTEXIST | OFN_HIDEREADONLY | OFN_LONGNAMES;
	if(GetOpenFileName(&ofn)) SetDlgItemText(hOwner, IDED_DIR, szPath);
}

//***************************************************************************************
// dlgAviSave : Boîte de dialogue pour enregistrer le fichier avi.
//***************************************************************************************
void __stdcall dlgAviSave(HWND hOwner)
{
	OPENFILENAME ofn;
	char szAviName[256] = "test.avi";
	ZeroMemory(&ofn, sizeof(OPENFILENAME));
	ofn.lStructSize = sizeof(OPENFILENAME);
	ofn.hInstance = g_hInstance; ofn.hwndOwner = hOwner;
	ofn.lpstrFilter = "Fichier Vidéo (*.avi)\0*.avi\0\0";
	ofn.lpstrFile = szAviName; ofn.nMaxFile = sizeof(szAviName);
	ofn.lpstrDefExt = ".avi";
	ofn.nFilterIndex = 1;
	ofn.Flags = OFN_PATHMUSTEXIST | OFN_HIDEREADONLY | OFN_OVERWRITEPROMPT | OFN_LONGNAMES | OFN_EXTENSIONDIFFERENT;
	if(GetSaveFileName(&ofn)) SetDlgItemText(hOwner, IDED_DIR2, szAviName);
}

//***************************************************************************************
// ImgLoad : Retourne le HBITMAP d'une image au format bmp, jpg, ou gif.
//***************************************************************************************
HBITMAP ImgLoad(char *pszFilePath)
{
   WCHAR wPath[256];
	LPPICTURE pPic = NULL;
	HBITMAP hPic = NULL;
	MultiByteToWideChar(CP_ACP, 0, pszFilePath, -1, wPath, 256); // ANSI vers Unicode
   if(OleLoadPicturePath(wPath, 0, 0, 0, IID_IPicture, (LPVOID*)&pPic)) return 0;
	if(pPic->get_Handle((UINT*)&hPic)) return 0;
	HBITMAP hPicRet = (HBITMAP)CopyImage(hPic, IMAGE_BITMAP, 0, 0, LR_COPYRETURNORG | LR_CREATEDIBSECTION);
	pPic->Release();
	return hPicRet;
}

//***************************************************************************************
// CreateAviThrd : Point d'entrée du thread pour créer le fichier avi.
//***************************************************************************************
DWORD WINAPI CreateAviThrd(void *pprm)
{
	char szInfo[64], szPath[256], szPicName[256], szFilePath[256];
	SetClassLong((HWND) pprm, GCL_HCURSOR, (long)g_hWait);
	GetDlgItemText(g_hDlg, IDED_DIR2, szFilePath, 256);
	HAVI avi = CreateAvi(szFilePath, 500, NULL);
	int nFrames = SendMessage(g_hList, LB_GETCOUNT, 0, 0);
	wsprintf(szInfo, "Fichier n°0 de %d (0 %%)", nFrames);
	SetWindowText(g_hInfo, szInfo);
	SendMessage(g_hPg, PBM_SETRANGE32, 0, nFrames);
	for(int i = 0; i < nFrames; i++) {
		SendMessage(g_hList, LB_GETTEXT, i, (LPARAM) szPicName);
		GetDlgItemText(g_hDlg, IDST_DIR, szPath, 256);
		lstrcat(szPath, "\\"); lstrcat(szPath, szPicName);
		HBITMAP hBmp = ImgLoad(szPath);
		if((i == 0) && (IsDlgButtonChecked(g_hDlg, IDCK_COMPRESSION)))
		{ // Initialise la compression juste avant la première frame
			AVICOMPRESSOPTIONS opts; ZeroMemory(&opts, sizeof(opts));
			SetAviVideoCompression(avi, hBmp, &opts, true, g_hDlg);
		}
		AddAviFrame(avi, hBmp);
		DeleteObject(hBmp);
		wsprintf(szInfo, "Fichier n°%d de %d (%d %%)", i+1, nFrames, ((i+1)*100)/nFrames);
		SetWindowText(g_hInfo, szInfo);
		SendMessage(g_hPg, PBM_STEPIT, 0, 0) ;
	}
	if(GetDlgItemText(g_hDlg, IDED_DIR, szFilePath, 256) != NULL)
		AddAviWav(avi, szFilePath, SND_FILENAME);
	CloseAvi(avi);
	PostMessage((HWND)pprm, ugWM_ENDTHRD, 0, 0);
	return 0;
}

//***************************************************************************************
// BarreDlgProc : Procédure de traitement des messages de la boîte de dialogue qui
//						affiche des informations pendant la création du fichier AVI.
//***************************************************************************************
BOOL CALLBACK BarreDlgProc(HWND hDlg, UINT uMsg, WPARAM wParam, LPARAM lParam)
{
	switch(uMsg) {
		case WM_INITDIALOG:
			g_hInfo = GetDlgItem(hDlg, IDST_INFO);
			g_hPg = GetDlgItem(hDlg, ID_PRGRSS);
			SendMessage(g_hPg, PBM_SETPOS, 0, 0);
			SendMessage(g_hPg, PBM_SETSTEP, 1, 0);
			SetClassLong(g_hPg, GCL_HCURSOR, (long)g_hWait);
			SetWindowPos(hDlg, 0, g_xPos, g_yPos, 0, 0, SWP_NOSIZE | SWP_NOZORDER);
			PostMessage(hDlg, ugWM_CREATEAVI, 0, 0);
			return 1;
		case ugWM_CREATEAVI:
			SetCursor(LoadCursor(0, IDC_WAIT));
			g_hThrd = CreateThread(0, 0, CreateAviThrd, (void*)hDlg, 0, 0);
			return 0;
		case ugWM_ENDTHRD:
			CloseHandle(g_hThrd);
			SetClassLong(hDlg, GCL_HCURSOR, (long)g_hArrow);
			EndDialog(hDlg, 0);
	}
	return 0;
}

//***************************************************************************************
// VerifDirs : Vérifie que les paths pour les frames et fichier AVI sont existants.
//***************************************************************************************
DWORD __stdcall VerifPaths(HWND hDlg)
{
	char *c, szFilePath[256];
	int v;
	int nFrames = SendMessage(g_hList, LB_GETCOUNT, 0, 0);
	if(nFrames == 0) {
		c = g_szErrFrames; v = IDBT_BROWSEIMG; goto noGood;
	}
	GetDlgItemText(hDlg, IDED_DIR2, szFilePath, 256);
	if(!szFilePath[0]) {
		c = g_szErrAvi; v = IDBT_BROWSEAVI; goto noGood;
	}
	return 1;
noGood:
	MessageBox(hDlg, c, g_szAppName, 0x30);
	PostMessage(hDlg, WM_NEXTDLGCTL, (long) GetDlgItem(hDlg, v), 1);
	return 0;
}

//***************************************************************************************
// MaPosition : Place la 2ème boîte de dialogue au même endroit que la 1ère.
//***************************************************************************************
void __stdcall MaPosition(HWND hDlg)
{
	RECT r; GetWindowRect(hDlg, &r);
	g_xPos = r.left; g_yPos = r.top;
}

//***************************************************************************************
// AppDlgProc : Procédure de traitement des messages de la boîte de dialogue principale.
//***************************************************************************************
BOOL CALLBACK AppDlgProc(HWND hDlg, UINT uMsg, WPARAM wParam, LPARAM lParam)
{
	switch(uMsg) {
		case WM_INITDIALOG:
			SetClassLong(hDlg, GCL_HICON, (long)LoadIcon(0, IDI_APPLICATION));
			g_hList = GetDlgItem(hDlg, ID_LST);
			g_hArrow = LoadCursor(0, IDC_ARROW);
			g_hWait = LoadCursor(0, IDC_WAIT);
			g_hDlg = hDlg;
			return 1;
		case WM_COMMAND:
			switch(wParam) {
				case IDBT_BROWSEIMG:
					onSelFiles(hDlg);
					return 0;
				case IDBT_BROWSEWAV:
					dlgWavOpen(hDlg);
					return 0;
				case IDBT_BROWSEAVI:
					dlgAviSave(hDlg);
					return 0;
				case IDBT_ABOUT:
					MessageBox(hDlg, "Version 1.0.2\nCopyright © 2005 Urgo", g_szAppName, 0x40);
					return 0;
				case IDBT_CREATE:
					if(!VerifPaths(hDlg)) break;
					MaPosition(hDlg);
					DialogBoxParam(g_hInstance, (LPCTSTR)IDD_BARRE, hDlg, BarreDlgProc, 0);
					return 0;
				case IDCANCEL: EndDialog(hDlg, 0);
			}
	}
	return 0;
}

//***************************************************************************************
// InitProgressControl : Force le chargement des Dlls pour le contrôle 'Progress Bar'.
//***************************************************************************************
void InitProgressControl()
{
	INITCOMMONCONTROLSEX icex;
	icex.dwSize = sizeof(icex); icex.dwICC = ICC_PROGRESS_CLASS;
	InitCommonControlsEx(&icex);
}

//***************************************************************************************
// WinMain : Point d'entrée de l'application.
//***************************************************************************************
int WINAPI WinMain(HINSTANCE hInstance, HINSTANCE, PSTR, int)
{
	CoInitialize(0);
	g_hInstance = hInstance;
	InitProgressControl();
	DialogBoxParam(hInstance, (LPCTSTR)IDD_APP, 0, AppDlgProc, 0);
	return 0;
}