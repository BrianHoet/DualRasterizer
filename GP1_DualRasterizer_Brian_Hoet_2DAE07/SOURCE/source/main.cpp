#include "pch.h"

#if defined(_DEBUG)
#include "vld.h"
#endif

#undef main
#include "Renderer.h"

using namespace dae;

bool g_PrintFPS{ true };

void ShutDown(SDL_Window* pWindow)
{
	SDL_DestroyWindow(pWindow);
	SDL_Quit();
}

int main(int argc, char* args[])
{
	//Unreferenced parameters
	(void)argc;
	(void)args;

	//Create window + surfaces
	SDL_Init(SDL_INIT_VIDEO);

	const uint32_t width = 640;
	const uint32_t height = 480;

	SDL_Window* pWindow = SDL_CreateWindow(
		"DualRasterizer - ***Hoet Brian, 2DAE07***",
		SDL_WINDOWPOS_UNDEFINED,
		SDL_WINDOWPOS_UNDEFINED,
		width, height, 0);
	//Github
	if (!pWindow)
		return 1;

	//Initialize "framework"
	const auto pTimer = new Timer();
	const auto pRenderer = new Renderer(pWindow);

	//Start loop
	pTimer->Start();
	float printTimer = 0.f;
	bool isLooping = true;
	while (isLooping)
	{
		//--------- Get input events ---------
		SDL_Event e;
		while (SDL_PollEvent(&e))
		{
			switch (e.type)
			{
			case SDL_QUIT:
				isLooping = false;
				break;
			case SDL_KEYDOWN:
				switch(e.key.keysym.scancode)
				{
					case SDL_SCANCODE_F1:
					pRenderer->ToggleMode();
					break;

					case SDL_SCANCODE_F2:
					pRenderer->ToggleRot();
					break;

					case SDL_SCANCODE_F3:
					pRenderer->ToggleFireMesh();
					break;

					case SDL_SCANCODE_F4:
					pRenderer->ToggleSampling();
					break;

					case SDL_SCANCODE_F5:
					pRenderer->ToggleLightMode();
					break;

					case SDL_SCANCODE_F6:
					pRenderer->ToggleNor();
					break;

					case SDL_SCANCODE_F7:
					pRenderer->ToggleBuffer();
					break;

					case SDL_SCANCODE_F8:
					pRenderer->ToggleBoxVisual();
					break;

					case SDL_SCANCODE_F9:
					pRenderer->ToggleCullMode();
					break;

					case SDL_SCANCODE_F10:
					pRenderer->ToggleBackGround();
					break;

					case SDL_SCANCODE_F11:
					g_PrintFPS = !g_PrintFPS;
					pRenderer->ToggleFPS(g_PrintFPS);
					break;

					case SDL_SCANCODE_I:
					pRenderer->PrintText();
					break;

				}
				break;

			default: ;
			}
		}

		//--------- Update ---------
		pRenderer->Update(pTimer);

		//--------- Render ---------
		pRenderer->Render();

		//--------- Timer ---------
		pTimer->Update();
		if (g_PrintFPS)
		{
			printTimer += pTimer->GetElapsed();
			if (printTimer >= 1.f)
			{
				printTimer = 0.f;
				std::cout << "dFPS: " << pTimer->GetdFPS() << std::endl;
			}
		}
	}
	pTimer->Stop();

	//Shutdown "framework"
	delete pRenderer;
	delete pTimer;

	ShutDown(pWindow);
	return 0;
}