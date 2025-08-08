#include "AVIFile.h"

//***************************************************************************************
// CreateAvi : Commence la création du fichier avi.
//
//	=>	Le WAVEFORMATEX peut être nul si vous n'allez pas ajouter du son,
//		ou si nous allez ajouter du son depuis un fichier.
//***************************************************************************************
HAVI CreateAvi(const char *fn, int frameperiod, const WAVEFORMATEX *wfx)
{
	IAVIFile *pfile;
	AVIFileInit();
	HRESULT hr = AVIFileOpen(&pfile, fn, OF_WRITE | OF_CREATE, NULL);
	if(hr != AVIERR_OK) {AVIFileExit(); return NULL;}
	TAviUtil *au = new TAviUtil;
	au->pfile = pfile;
	if(wfx == NULL) ZeroMemory(&au->wfx, sizeof(WAVEFORMATEX)); 
	else CopyMemory(&au->wfx, wfx, sizeof(WAVEFORMATEX));
	au->period = frameperiod;
	au->as = 0; au->ps = 0; au->psCompressed = 0;
	au->nframe = 0; au->nsamp=0;
	au->iserr = false;
	return (HAVI)au;
}

//***************************************************************************************
// CloseAvi : On ferme le fichier avi.
//***************************************************************************************
HRESULT CloseAvi(HAVI avi)
{
	if(avi == NULL) return AVIERR_BADHANDLE;
	TAviUtil *au = (TAviUtil*)avi;
	if(au->as != 0) AVIStreamRelease(au->as); au->as = 0;
	if(au->psCompressed != 0) AVIStreamRelease(au->psCompressed); au->psCompressed = 0;
	if(au->ps != 0) AVIStreamRelease(au->ps); au->ps = 0;
	if(au->pfile != 0) AVIFileRelease(au->pfile); au->pfile = 0;
	AVIFileExit();
	delete au;
	return S_OK;
}

//***************************************************************************************
// SetAviVideoCompression : Permet la compression de la vidéo.
//
// => Cette fonction doit être appelée avant l'ajout des frames.
//		hBmp doit être une DIBSection, simplement pour savoir le format et la taille.
//		Cette fontion peut affichier une boîte de dialogue pour laisser à l'utilisateur
//		le choix du type de compresion. (mettez alors bShowDialog à true)
//***************************************************************************************
HRESULT SetAviVideoCompression(HAVI avi, HBITMAP hBmp, AVICOMPRESSOPTIONS *opts, bool bShowDialog, HWND hParent)
{
	if(avi == NULL) return AVIERR_BADHANDLE;
	if(hBmp == NULL) return AVIERR_BADPARAM;
	DIBSECTION dibs; int sbm = GetObject(hBmp, sizeof(dibs), &dibs);
	if(sbm != sizeof(DIBSECTION)) return AVIERR_BADPARAM;
	TAviUtil *au = (TAviUtil*)avi;
	if(au->iserr) return AVIERR_ERROR;
	if(au->psCompressed != 0) return AVIERR_COMPRESSOR;

	if(au->ps == 0) // Créé le stream, si cela ne l'a pas été auparavant
	{
		AVISTREAMINFO strhdr; ZeroMemory(&strhdr, sizeof(strhdr));
		strhdr.fccType = streamtypeVIDEO; // Type du stream
		strhdr.fccHandler = 0; 
		strhdr.dwScale = au->period;
		strhdr.dwRate = 1000;
		strhdr.dwSuggestedBufferSize  = dibs.dsBmih.biSizeImage;
		SetRect(&strhdr.rcFrame, 0, 0, dibs.dsBmih.biWidth, dibs.dsBmih.biHeight);
		HRESULT hr=AVIFileCreateStream(au->pfile, &au->ps, &strhdr);
		if(hr != AVIERR_OK) {au->iserr = true; return hr;}
	}

	if(au->psCompressed == 0) // Initialise la compression, affiche boîte de dialogue si nécessaire
	{ 
		AVICOMPRESSOPTIONS myopts; ZeroMemory(&myopts, sizeof(myopts));
		AVICOMPRESSOPTIONS *aopts[1];
		if(opts != NULL) aopts[0] = opts; else aopts[0] = &myopts;
		if(bShowDialog)
		{
			BOOL res = (BOOL)AVISaveOptions(hParent, 0, 1, &au->ps, aopts);
			if(!res) {AVISaveOptionsFree(1, aopts); au->iserr = true; return AVIERR_USERABORT;}
		}
		HRESULT hr = AVIMakeCompressedStream(&au->psCompressed, au->ps, aopts[0], NULL);
		AVISaveOptionsFree(1, aopts);
		if(hr != AVIERR_OK) {au->iserr = true; return hr;}
		DIBSECTION dibs; GetObject(hBmp, sizeof(dibs), &dibs);
		hr = AVIStreamSetFormat(au->psCompressed, 0, &dibs.dsBmih, dibs.dsBmih.biSize+dibs.dsBmih.biClrUsed * sizeof(RGBQUAD));
		if(hr != AVIERR_OK) {au->iserr = true; return hr;}
  }

  return AVIERR_OK;
}

//***************************************************************************************
// AddAviFrame : Ajoute les frames pour le fichier avi.
//***************************************************************************************
HRESULT AddAviFrame(HAVI avi, HBITMAP hBmp)
{
	if(avi == NULL) return AVIERR_BADHANDLE;
	if(hBmp == NULL) return AVIERR_BADPARAM;
	DIBSECTION dibs; int sbm = GetObject(hBmp, sizeof(dibs), &dibs);
	if(sbm != sizeof(DIBSECTION)) return AVIERR_BADPARAM;
	TAviUtil *au = (TAviUtil*)avi;
	if(au->iserr) return AVIERR_ERROR;

	if(au->ps == 0) // Créé le stream, si cela ne l'a pas été auparavant
	{
		AVISTREAMINFO strhdr; ZeroMemory(&strhdr, sizeof(strhdr));
		strhdr.fccType = streamtypeVIDEO; // Type du stream
		strhdr.fccHandler = 0; 
		strhdr.dwScale = au->period;
		strhdr.dwRate = 1000;
		strhdr.dwSuggestedBufferSize = dibs.dsBmih.biSizeImage; 
		SetRect(&strhdr.rcFrame, 0, 0, dibs.dsBmih.biWidth, dibs.dsBmih.biHeight);
		HRESULT hr = AVIFileCreateStream(au->pfile, &au->ps, &strhdr);
		if(hr != AVIERR_OK) {au->iserr = true; return hr;}
	}

	if(au->psCompressed == 0) // Si fichier avi sans compression
	{
		AVICOMPRESSOPTIONS opts; ZeroMemory(&opts, sizeof(opts));
		opts.fccHandler = mmioFOURCC('D','I','B',' '); 
		HRESULT hr = AVIMakeCompressedStream(&au->psCompressed, au->ps, &opts, NULL);
		if(hr != AVIERR_OK) {au->iserr = true; return hr;}
		hr = AVIStreamSetFormat(au->psCompressed, 0, &dibs.dsBmih, dibs.dsBmih.biSize+dibs.dsBmih.biClrUsed * sizeof(RGBQUAD));
		if(hr != AVIERR_OK) {au->iserr = true; return hr;}
	}

	// Maintenant on peut ajouter les frames
	HRESULT hr = AVIStreamWrite(au->psCompressed, au->nframe, 1, dibs.dsBm.bmBits, dibs.dsBmih.biSizeImage, AVIIF_KEYFRAME, NULL, NULL);
	if(hr != AVIERR_OK) {au->iserr = true; return hr;}
	au->nframe++; return S_OK;
}

//***************************************************************************************
// AddAviWav : Ajoute un son wav au fichier avi.
//
//	=> Le fichier wave peut être déjà chargé en mémoire (flags = SND_MEMORY)
//		ou bien dans un fichier sur le disque (flags = SND_FILENAME).
//		Cette fonction exige qu'avant un WAVEFORMATEX nul ait été passé à CreateAvi,
//		ou que le fichier wave en train d'être ajouté a le même format que celui
//		ajouté plus tôt.
//***************************************************************************************
HRESULT AddAviWav(HAVI avi, const char *src, DWORD flags)
{ 
	if(avi == NULL) return AVIERR_BADHANDLE;
	if(flags != SND_MEMORY && flags != SND_FILENAME) return AVIERR_BADFLAGS;
	if(src == 0) return AVIERR_BADPARAM;
	TAviUtil *au = (TAviUtil*)avi;
	if(au->iserr) return AVIERR_ERROR;

	char *buf=0; WavChunk *wav = (WavChunk*)src;
	if(flags == SND_FILENAME) 
	{
		HANDLE hf = CreateFile(src, GENERIC_READ, FILE_SHARE_READ, NULL, OPEN_EXISTING, 0, NULL);
		if(hf == INVALID_HANDLE_VALUE) {au->iserr = true; return AVIERR_FILEOPEN;}
		DWORD size = GetFileSize(hf, NULL);
		buf = new char[size];
		DWORD red; ReadFile(hf, buf, size, &red, NULL);
		CloseHandle(hf);
		wav = (WavChunk*)buf;
	}

	// Vérifie que le format n'est pas mauvais
	bool badformat = false;
	if(au->wfx.nChannels == 0) 
	{
		au->wfx.wFormatTag = wav->fmt.wFormatTag;
		au->wfx.cbSize = 0;
		au->wfx.nAvgBytesPerSec = wav->fmt.dwAvgBytesPerSec;
		au->wfx.nBlockAlign = wav->fmt.wBlockAlign;
		au->wfx.nChannels = wav->fmt.wChannels;
		au->wfx.nSamplesPerSec = wav->fmt.dwSamplesPerSec;
		au->wfx.wBitsPerSample = wav->fmt.wBitsPerSample;
	}
	else 
	{
		if(au->wfx.wFormatTag != wav->fmt.wFormatTag) badformat = true;
		if(au->wfx.nAvgBytesPerSec != wav->fmt.dwAvgBytesPerSec) badformat = true;
		if(au->wfx.nBlockAlign != wav->fmt.wBlockAlign) badformat = true;
		if(au->wfx.nChannels != wav->fmt.wChannels) badformat = true;
		if(au->wfx.nSamplesPerSec != wav->fmt.dwSamplesPerSec) badformat = true;
		if(au->wfx.wBitsPerSample != wav->fmt.wBitsPerSample) badformat = true;
	}
	if(badformat) {if(buf != 0) delete[] buf; return AVIERR_BADFORMAT;}

	if(au->as == 0) // Créé le stream si nécessaire
	{
		AVISTREAMINFO ahdr; ZeroMemory(&ahdr, sizeof(ahdr));
		ahdr.fccType = streamtypeAUDIO;
		ahdr.dwScale = au->wfx.nBlockAlign;
		ahdr.dwRate = au->wfx.nSamplesPerSec*au->wfx.nBlockAlign; 
		ahdr.dwSampleSize = au->wfx.nBlockAlign;
		ahdr.dwQuality = (DWORD)-1;
		HRESULT hr = AVIFileCreateStream(au->pfile, &au->as, &ahdr);
		if(hr != AVIERR_OK) {if(buf != 0) delete[] buf; au->iserr = true; return hr;}
		hr = AVIStreamSetFormat(au->as, 0, &au->wfx, sizeof(WAVEFORMATEX));
		if(hr != AVIERR_OK) {if(buf != 0) delete[] buf; au->iserr = true; return hr;}
	}

	// Maintenant nous pouvons écrire les données
	DWORD numbytes = wav->dat.dLen;
	DWORD numsamps = numbytes * 8 / au->wfx.wBitsPerSample;
	HRESULT hr = AVIStreamWrite(au->as, au->nsamp, numsamps, wav->dat.dData, numbytes, 0, NULL, NULL);
	if(buf != 0) delete[] buf;
	if(hr != AVIERR_OK) {au->iserr = true; return hr;}
	au->nsamp += numsamps; return S_OK;
}

//***************************************************************************************
// FormatAviMessage : Traduit les messages d'erreurs.
//***************************************************************************************
unsigned int FormatAviMessage(HRESULT code, char *buf, unsigned int len)
{ 
	const char *msg = "Unknown avi result code";
	switch (code)
	{
		case S_OK:                  msg = "Success"; break;
		case AVIERR_BADFORMAT:      msg = "AVIERR_BADFORMAT: corrupt file or unrecognized format"; break;
		case AVIERR_MEMORY:         msg = "AVIERR_MEMORY: insufficient memory"; break;
		case AVIERR_FILEREAD:       msg = "AVIERR_FILEREAD: disk error while reading file"; break;
		case AVIERR_FILEOPEN:       msg = "AVIERR_FILEOPEN: disk error while opening file"; break;
		case REGDB_E_CLASSNOTREG:   msg = "REGDB_E_CLASSNOTREG: file type not recognised"; break;
		case AVIERR_READONLY:       msg = "AVIERR_READONLY: file is read-only"; break;
		case AVIERR_NOCOMPRESSOR:   msg = "AVIERR_NOCOMPRESSOR: a suitable compressor could not be found"; break;
		case AVIERR_UNSUPPORTED:    msg = "AVIERR_UNSUPPORTED: compression is not supported for this type of data"; break;
		case AVIERR_INTERNAL:       msg = "AVIERR_INTERNAL: internal error"; break;
		case AVIERR_BADFLAGS:       msg = "AVIERR_BADFLAGS"; break;
		case AVIERR_BADPARAM:       msg = "AVIERR_BADPARAM"; break;
		case AVIERR_BADSIZE:        msg = "AVIERR_BADSIZE"; break;
		case AVIERR_BADHANDLE:      msg = "AVIERR_BADHANDLE"; break;
		case AVIERR_FILEWRITE:      msg = "AVIERR_FILEWRITE: disk error while writing file"; break;
		case AVIERR_COMPRESSOR:     msg = "AVIERR_COMPRESSOR"; break;
		case AVIERR_NODATA:         msg = "AVIERR_READONLY"; break;
		case AVIERR_BUFFERTOOSMALL: msg = "AVIERR_BUFFERTOOSMALL"; break;
		case AVIERR_CANTCOMPRESS:   msg = "AVIERR_CANTCOMPRESS"; break;
		case AVIERR_USERABORT:      msg = "AVIERR_USERABORT"; break;
		case AVIERR_ERROR:          msg = "AVIERR_ERROR"; break;
	}
	unsigned int mlen = (unsigned int)strlen(msg);
	if (buf == 0 || len == 0) return mlen;
	unsigned int n = mlen; if (n+1 > len) n = len-1;
	strncpy(buf, msg, n); buf[n] = 0;
	return mlen; // Retourne longueur du msg d'erreur
}