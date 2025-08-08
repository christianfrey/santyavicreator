#ifndef AVIFILE_H_INCLUDED
#define AVIFILE_H_INCLUDED

#include <windows.h>
#include <vfw.h>
#pragma comment(lib, "vfw32.lib")

DECLARE_HANDLE(HAVI);

// On définie le format WAVE (http://www.sonicspot.com/guide/wavefiles.html)
#include <pshpack1.h>
typedef struct
{
	DWORD fId;        // Contient 'fmt ' (avec l'espace)
	DWORD fLen;		   // Longueur du Chunck, doit être 16
	WORD  wFormatTag; // Format (WAVE_FORMAT_PCM = 1) // WAVE_FORMAT_MPEGLAYER3 = MP3
	WORD  wChannels;  // Nombre de canaux (1=mono, 2=stéréo)
	DWORD dwSamplesPerSec;  // Fréquence d'échantillonage (en Hz)
	DWORD dwAvgBytesPerSec; // = wChannels * dwSamplesPerSec * (wBitsPerSample / 8)
	WORD  wBlockAlign;      // = wChannels * (wBitsPerSample / 8)
	WORD  wBitsPerSample;   // Longueur d'un échantillon en bits (8, 16, 24 ou 32)
} FmtChunk;

typedef struct
{
	DWORD dId;  // Contient 'data'
	DWORD dLen; // Longueur du chunck dData (en octets)
	unsigned char dData[1]; // Les données du son échantillonné
} DataChunk;

typedef struct
{
	DWORD FichType;	  // Contient 'RIFF'
	DWORD dwFileLength; // Longueur du fichier
	DWORD InfoType;     // Contient 'WAVE'
	FmtChunk fmt;  // Le Format Chunk
	DataChunk dat; // Le WAVE Data Chunck
} WavChunk;
#include <poppack.h>

typedef struct
{
	IAVIFile *pfile;	// Créé par CreateAvi
	WAVEFORMATEX wfx; // Comme donné à CreateAvi (.nChanels = 0 si aucun donné). Utilisé quant le stream audio est créé en premier
	int period;			// Indiqué dans CreateAvi, utilisé quand le stream vidéo est créé en premier
	IAVIStream *as;	// Stream audio, initialisé quant le stream audio est créé en premier
	IAVIStream *ps, *psCompressed; // Stream vidéo, lorsqu'il est créé en premier
	unsigned long nframe, nsamp;   // Quelle frame va être ajoutée juste après, quel sample va être ajouté après?
	bool iserr;			// Si vraie, alors aucune fonction ne fera quelque chose
} TAviUtil;

HAVI CreateAvi(const char *fn, int frameperiod, const WAVEFORMATEX *wfx);
HRESULT CloseAvi(HAVI avi);
HRESULT AddAviFrame(HAVI avi, HBITMAP hbm);
HRESULT AddAviWav(HAVI avi, const char *wav, DWORD flags);
HRESULT SetAviVideoCompression(HAVI avi, HBITMAP hBmp, AVICOMPRESSOPTIONS *opts, bool bShowDialog, HWND hParent);
unsigned int FormatAviMessage(HRESULT code, char *buf, unsigned int len);

#endif	// AVIFILE_H_INCLUDED