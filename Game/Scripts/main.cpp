#include <windows.h>

#include <memory>

#include "Scripts/GJ26.h"

int WINAPI WinMain(HINSTANCE, HINSTANCE, LPSTR, int) {
	std::unique_ptr<szg::Framework> framework =
		std::make_unique<GJ26>();

	framework->run();

	framework.reset();

	return 0;
}


