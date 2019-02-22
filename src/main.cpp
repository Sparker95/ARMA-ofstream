#include <intercept.hpp>
#include "Windows.h"
#include <stdio.h>
#include "FileInterface.h"

#ifdef _DEBUG
#include <conio.h>
#endif

extern FileInterface g_FileInterface;

int intercept::api_version() { //This is required for the plugin to work.
    return 1;
}

void intercept::register_interfaces() {
    
}

void intercept::pre_start() {

	g_FileInterface.preStart();
}

void intercept::pre_init() {
    intercept::sqf::system_chat("ARMA ofstream plugin is running!");
}

using namespace intercept;

// This function is exported and is called by the host at the end of mission initialization.
void intercept::post_init() {

}

// Normal Windows DLL junk...
BOOL APIENTRY DllMain(HMODULE hModule,
	DWORD  ul_reason_for_call,
	LPVOID lpReserved
)
{
	switch (ul_reason_for_call)
	{
	case DLL_PROCESS_ATTACH: {
#ifdef _DEBUG
		AllocConsole();
		_cprintf("DLL ATTACH\n");
		_cprintf("size of pointer: %i, size of int: %i\n", sizeof(void*), sizeof(unsigned int));
		_cprintf("test unsinged int: %0llX\n", 0x0011223344556677);

		void* testPtr = 0;
		sscanf("0011223344556677", "%llX", &testPtr);
		_cprintf("Restored pointer from string: %llX\n", testPtr);
#endif
		//_cprintf("Enter anything when concurrency visualizer is attached...\n");
		//int a;
		//_cscanf("%i", &a);
		//FreeConsole();

		// Create a new marker series
		//g_mrkSeries = new marker_series(_T("Intercept marker series"));

		break;
	}
	case DLL_THREAD_ATTACH:
	case DLL_THREAD_DETACH:
	case DLL_PROCESS_DETACH:
		break;
	}
	return TRUE;
}