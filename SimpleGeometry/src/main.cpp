#define WIN32_LEAN_AND_MEAN
#include <Windows.h>

#include <SimpleGeoApp.h>

int CALLBACK wWinMain(HINSTANCE hInstance, HINSTANCE hPrevInstance, PWSTR lpCmdLine, int nCmdSho) {
	SimpleGeoApp app(hInstance);

	if (!app.Initialize()) {
		return 0;
	}

	app.Run();

	return 0;
}