//
// Created by eli on 4/22/2022.
//

#include <graphics/graphics.h>
#include <runtime/runtime.h>
#include <runtime/module.h>
#include "assets/assetManager.h"
#include "fileManager/fileManager.h"
#include "networking/networking.h"
#include "ui/editorUI.h"



int main(int argc, char** argv)
{
	Config::loadConfig();

	Runtime::init();
	Timeline& tl = Runtime::timeline();
	tl.addBlock("asset management");
	tl.addBlock("networking");
	tl.addBlock("before main");
	tl.addBlock("main");
	tl.addBlock("draw");
	Runtime::addModule<NetworkManager>();
	Runtime::addModule<FileManager>();
	Runtime::addModule<AssetManager>();
	Runtime::addModule<EntityManager>();
	Runtime::addModule<graphics::VulkanRuntime>();
	Runtime::addModule<EditorUI>();

	Runtime::run();
	Runtime::cleanup();
}

#ifdef _WIN32
int APIENTRY WinMain(HINSTANCE hInstance,
                     HINSTANCE hPrevInstance,
                     LPSTR lpCmdLine, int nCmdShow)
{
	return main(__argc, __argv);
}
#endif