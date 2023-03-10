#include "pch.h"
#include "Renderer.h"
#include "MeshRepresentation.h"
#include "Texture.h"
#include "ShadedEffect.h"
#include "Utils.h"

HANDLE m_hConsole = GetStdHandle(STD_OUTPUT_HANDLE);

using namespace dae;
using namespace std;

Renderer::Renderer(SDL_Window* pWindow) :
	m_pWindow(pWindow)
{
	//Initialize
	SDL_GetWindowSize(pWindow, &m_Width, &m_Height);
	m_Camera.Initialize(45.f,  {0, 0, 0}, float(m_Width) / m_Height );


	//Initialize DirectX pipeline
	const HRESULT result = InitializeDirectX();
	if (result == S_OK)
	{
		m_IsInitialized = true;
		std::cout << "DirectX is initialized and ready!\n";
	}
	else
	{
		std::cout << "DirectX initialization failed!\n";
	}

	ShadedEffect* pShadedEffect{ new ShadedEffect(m_pDevice, L"Resources/PosCol3D.fx") };

	m_pDiffuseTxt = new Texture(m_pDevice, "Resources/vehicle_diffuse.png");
	m_pNormalTxt = new Texture(m_pDevice, "Resources/vehicle_normal.png");
	m_pSpecularTxt = new Texture(m_pDevice, "Resources/vehicle_specular.png");
	m_pGlossTxt = new Texture(m_pDevice, "Resources/vehicle_gloss.png");

	pShadedEffect->SetDiffuseMap(m_pDiffuseTxt);
	pShadedEffect->SetNormalMap(m_pNormalTxt);
	pShadedEffect->SetSpecularMap(m_pSpecularTxt);
	pShadedEffect->SetGlossinessMap(m_pGlossTxt);
	
	m_pMeshRepresentation.push_back(new MeshRepresentation{ m_pDevice, "Resources/vehicle.obj", std::move(pShadedEffect) });

	Effect* pTransparentEffect{ new Effect(m_pDevice, L"Resources/Transparent3D.fx") };

	Texture fireDiffuseTexture{ m_pDevice, "Resources/fireFX_diffuse.png" };
	pTransparentEffect->SetDiffuseMap(&fireDiffuseTexture);
	
	m_pFireMesh = (new MeshRepresentation{ m_pDevice,"Resources/fireFX.obj",std::move(pTransparentEffect) });
	m_pMeshRepresentation.push_back(m_pFireMesh);

	//INITIALIZE RASTERIZER

	//Create Buffers
	m_pFrontBuffer = SDL_GetWindowSurface(pWindow);
	m_pBackBuffer = SDL_CreateRGBSurface(0, m_Width, m_Height, 32, 0, 0, 0, 0);
	m_pBackBufferPixels = (uint32_t*)m_pBackBuffer->pixels;

	m_pDepthBufferPixels = new float[m_Width * m_Height];

	//Mesh
	MeshRasterizer& mesh = m_pMeshesRast.emplace_back(MeshRasterizer{});
	Utils::ParseOBJ("Resources/vehicle.obj", mesh.vertices, mesh.indices);
	mesh.primitiveTopology = PrimitiveTopology::TriangleList;

	PrintText();
}

Renderer::~Renderer()
{
	if (m_pRenderTargetView) m_pRenderTargetView->Release();
	if (m_pRenderTargetBuffer) m_pRenderTargetBuffer->Release();
	if (m_pDepthStencilView) m_pDepthStencilView->Release();
	if (m_pDepthStencilBuffer) m_pDepthStencilBuffer->Release();
	if (m_pSwapChain) m_pSwapChain->Release();
	if (m_pDeviceContext)
	{
		m_pDeviceContext->ClearState();
		m_pDeviceContext->Flush();
		m_pDeviceContext->Release();
	}
	if (m_pDevice) m_pDevice->Release();
	
	for (auto& Mesh : m_pMeshRepresentation)
	{
		delete Mesh;
	}


	//RASTERIZER
	delete m_pDiffuseTxt;
	delete m_pNormalTxt;
	delete m_pSpecularTxt;
	delete m_pGlossTxt;
	delete[] m_pDepthBufferPixels;
}

HRESULT Renderer::InitializeDirectX()
{
		//1. Create Device & DeviceContext
		//=====
		D3D_FEATURE_LEVEL featureLevel = D3D_FEATURE_LEVEL_11_1;
		uint32_t createDeviceFlags = 0;
	#if defined(DEBUG) || defined(_DEBUG)
		createDeviceFlags |= D3D11_CREATE_DEVICE_DEBUG;
	#endif 
		HRESULT result = D3D11CreateDevice(nullptr, D3D_DRIVER_TYPE_HARDWARE, 0, createDeviceFlags, &featureLevel,
			1, D3D11_SDK_VERSION, &m_pDevice, nullptr, &m_pDeviceContext);
		if (FAILED(result))
			return result;

		//Create DXGI Factory
		IDXGIFactory1* pDxgiFactory{};
		result = CreateDXGIFactory1(__uuidof(IDXGIFactory1), reinterpret_cast<void**>(&pDxgiFactory));
		if (FAILED(result))
			return result;

		//2. Create Swapchain
		//=====
		DXGI_SWAP_CHAIN_DESC swapChainDesc{};
		swapChainDesc.BufferDesc.Width = m_Width;
		swapChainDesc.BufferDesc.Height = m_Height;
		swapChainDesc.BufferDesc.RefreshRate.Numerator = 1; 
		swapChainDesc.BufferDesc.RefreshRate.Denominator = 60;
		swapChainDesc.BufferDesc.Format = DXGI_FORMAT_R8G8B8A8_UNORM;
		swapChainDesc.BufferDesc.ScanlineOrdering = DXGI_MODE_SCANLINE_ORDER_UNSPECIFIED; 
		swapChainDesc.BufferDesc.Scaling = DXGI_MODE_SCALING_UNSPECIFIED;
		swapChainDesc.SampleDesc.Count = 1;
		swapChainDesc.SampleDesc.Quality = 0;
		swapChainDesc.BufferUsage = DXGI_USAGE_RENDER_TARGET_OUTPUT;
		swapChainDesc.BufferCount = 1;
		swapChainDesc.Windowed = true;
		swapChainDesc.SwapEffect = DXGI_SWAP_EFFECT_DISCARD;
		swapChainDesc.Flags = 0;

		//Get the handle (HWND) from the SDL Backbuffer
		SDL_SysWMinfo sysWMInfo{}; 
		SDL_VERSION(&sysWMInfo.version)
		SDL_GetWindowWMInfo(m_pWindow, &sysWMInfo);
		swapChainDesc.OutputWindow = sysWMInfo.info.win.window;
		//Create SwapChain
		result = pDxgiFactory->CreateSwapChain(m_pDevice, &swapChainDesc, &m_pSwapChain);
		if (FAILED(result))
			return result;

		//3. Create DepthStencil (DS) & DepthStencilView (DSV)
		//Resource
		D3D11_TEXTURE2D_DESC depthStencilDesc{};
		depthStencilDesc.Width = m_Width;
		depthStencilDesc.Height = m_Height;
		depthStencilDesc.MipLevels = 1;
		depthStencilDesc.ArraySize = 1;
		depthStencilDesc.Format = DXGI_FORMAT_D24_UNORM_S8_UINT;
		depthStencilDesc.SampleDesc.Count = 1;
		depthStencilDesc.SampleDesc.Quality = 0;
		depthStencilDesc.Usage = D3D11_USAGE_DEFAULT;
		depthStencilDesc.BindFlags = D3D11_BIND_DEPTH_STENCIL;
		depthStencilDesc.CPUAccessFlags = 0;
		depthStencilDesc.MiscFlags = 0;

		//View
		D3D11_DEPTH_STENCIL_VIEW_DESC depthStencilViewDesc{}; 
		depthStencilViewDesc.Format = depthStencilDesc.Format;
		depthStencilViewDesc.ViewDimension = D3D11_DSV_DIMENSION_TEXTURE2D;
		depthStencilViewDesc.Texture2D.MipSlice = 0;

		result = m_pDevice->CreateTexture2D(&depthStencilDesc, nullptr, &m_pDepthStencilBuffer); 
		if (FAILED(result)) 
			return result;
		result = m_pDevice->CreateDepthStencilView(m_pDepthStencilBuffer, &depthStencilViewDesc, &m_pDepthStencilView);
		if (FAILED(result))
			return result;

		//4. Create RenderTarget (RT) & RenderTargetView (RTV)
		//===== 
		// 
		//Resource
		result = m_pSwapChain->GetBuffer(0, __uuidof(ID3D11Texture2D), reinterpret_cast<void**>(&m_pRenderTargetBuffer)); if (FAILED(result))
			return result;

		//View
		result = m_pDevice->CreateRenderTargetView(m_pRenderTargetBuffer, nullptr, &m_pRenderTargetView);
		if (FAILED(result))
			return result;

		//5. Bind RTV & DSV to Output Merger Stage
		//=====
		m_pDeviceContext->OMSetRenderTargets(1, &m_pRenderTargetView, m_pDepthStencilView);

		//6. Set Viewport
		//=====
		D3D11_VIEWPORT viewport{};
		viewport.Width = static_cast<float>(m_Width);
		viewport.Height = static_cast<float>(m_Height);
		viewport.TopLeftX = 0.f;
		viewport.TopLeftY = 0.f;
		viewport.MinDepth = 0.f;
		viewport.MaxDepth = 1.f;
		m_pDeviceContext->RSSetViewports(1, &viewport);

		return result;
}

void Renderer::PrintText() const
{
		SetConsoleTextAttribute(m_hConsole, m_Yellow);
		cout << "[Key bindings - SHARED]\n";
		cout << "	[F1]  Toggle Rasterizer Mode (HARDWARE/SOFTWARE)\n";
		cout << "	[F2]  Toggle Vehicle Rotation (ON/OFF)\n";
		cout << "	[F9]  Cycle CullMode (BACK/FRONT/NONE)\n";
		cout << "	[F10] Toggle Uniform ClearColor (ON/OFF)\n";
		cout << "	[F11] Toggle Print FPS (ON/OFF)\n";
		cout << '\n';
		SetConsoleTextAttribute(m_hConsole, m_Green);
		cout << "[Key bindings - HARDWARE]\n";
		cout << "	[F3]  Toggle FireMesh (ON/OFF)\n";
		cout << "	[F4]  Cycle Sample State (POINT/LINEAR/ANISOTROPIC)\n";
		cout << '\n';
		//cout << MAGENTA;
		SetConsoleTextAttribute(m_hConsole, m_Magenta);
		cout << "[Key bindings - SOFTWARE]\n";
		cout << "	[F5]  Cycle Shading Mode (COMBINED/OBSERVED_AREA/DIFFUSE/SPECULAR)\n";
		cout << "	[F6]  Toggle NormalMap (ON/OFF)\n";
		cout << "	[F7]  Toggle DepthBuffer Visualization (ON/OFF)\n";
		cout << "	[F8]  Toggle BoundingBox Visualization (ON/OFF)\n";
		cout << '\n';
		//cout << RED;
		SetConsoleTextAttribute(m_hConsole, m_Red);
		cout << "[I] Get Information List\n";
		cout << '\n';
		SetConsoleTextAttribute(m_hConsole, m_White);
}

void Renderer::Update(const Timer* pTimer)
{
	m_Camera.Update(pTimer);

	
	if(m_RotEnabled)
	{
		const float rotationSpeed{ float(M_PI)/4.f * pTimer->GetElapsed() };
		m_Angle += rotationSpeed;
		
	}
	if (m_DirectXMode)
	{
		UpdateDirectX(pTimer);
	}
	else
	{
		UpdateRasterizer(pTimer);
	}
	
}

void Renderer::UpdateDirectX(const Timer* pTimer)
{
	for (auto& Mesh : m_pMeshRepresentation)
	{
		Mesh->Update(m_Camera.GetWorldViewProjection(), m_Camera.GetInverseViewMatrix(), m_Angle, Pos);
	}
}
void Renderer::UpdateRasterizer(const Timer* pTimer)
{
	for (MeshRasterizer& mesh : m_pMeshesRast)
	{
		Matrix newTrsMatrix = Matrix::CreateRotationY(m_Angle) * Matrix::CreateTranslation(Pos);
		mesh.worldMatrix = newTrsMatrix;
	}
}

void Renderer::Render()
{
	if (m_DirectXMode)
	{
		RenderDirectX();
	}
	else
	{
		RenderRasterizer();
	}

}

void Renderer::RenderDirectX() const
{
	if (!m_IsInitialized)
		return;
	ColorRGB clearColor{};
	//1. CLEAR RTV & DSV
	if (!m_UniformBackGround)
	{
		clearColor = { .39f, .59f, .93f };
	}
	else
	{
		clearColor = { .1f, .1f, .1f };
	}
	m_pDeviceContext->ClearRenderTargetView(m_pRenderTargetView,&clearColor.r);
	m_pDeviceContext->ClearDepthStencilView(m_pDepthStencilView, D3D11_CLEAR_DEPTH | D3D11_CLEAR_STENCIL, 1.f, 0);
	//2. SET PIPELINE + INVOKE DRAWCALLS (= RENDER)
	for (auto& Mesh : m_pMeshRepresentation)
	{
		
		if(Mesh == m_pFireMesh && !m_FireMeshEnabled)
			break;
		
		Mesh->Render(m_pDeviceContext);
	}

	//3. PRESENT BACKBUFFER (SWAP)
	m_pSwapChain->Present( 1, 0);

}
void Renderer::RenderRasterizer()
{
	SDL_LockSurface(m_pBackBuffer);
	ColorRGB clearColor{};
	//Clear backBuffer
	if (!m_UniformBackGround)
	{
		clearColor = { .39f, .39f, .39f };
	}
	else
	{
		clearColor = { .1f, .1f, .1f };
	}
	clearColor *= 255;
	uint32_t hexColor = 0xFF000000 | (uint32_t)clearColor.b << 8 | (uint32_t)clearColor.g << 16 | (uint32_t)clearColor.r;
	SDL_FillRect(m_pBackBuffer, NULL, hexColor);

	std::fill_n(m_pDepthBufferPixels, m_Width * m_Height, FLT_MAX);
	VertexTransformationFunctionW4(m_pMeshesRast);

	for (const auto& mesh : m_pMeshesRast)
	{
		int incrementAmount{ 1 };
		if (mesh.primitiveTopology == PrimitiveTopology::TriangleList)
		{
			incrementAmount = 3;
		}

		for (int i{}; i < mesh.indices.size() - 2; i += incrementAmount)
		{
			//Points of the Triangle
			const uint32_t indexA{ mesh.indices[i] };
			uint32_t indexB{ mesh.indices[i + 1] };
			uint32_t indexC{ mesh.indices[i + 2] };

			if (mesh.primitiveTopology == PrimitiveTopology::TriangleStrip)
			{
				if (i % 2 != 0)
				{
					std::swap(indexB, indexC);
				}

				if (indexA == indexB)
					continue;

				if (indexB == indexC)
					continue;

				if (indexC == indexA)
					continue;
			}

			Vertex_Out A{ mesh.vertices_out[indexA] };
			Vertex_Out B{ mesh.vertices_out[indexB] };
			Vertex_Out C{ mesh.vertices_out[indexC] };

			// Do frustum culling
			if ((A.position.x < -1.0f || A.position.x > 1.0f) &&
				(B.position.x < -1.0f || B.position.x > 1.0f) &&
				(C.position.x < -1.0f || C.position.x > 1.0f))
				continue;

			if ((A.position.y < -1.0f || A.position.y > 1.0f) &&
				(B.position.y < -1.0f || B.position.y > 1.0f) &&
				(C.position.y < -1.0f || C.position.y > 1.0f))
				continue;

			if (A.position.z < 0.0f || A.position.z > 1.0f ||
				B.position.z < 0.0f || B.position.z > 1.0f ||
				C.position.z < 0.0f || C.position.z > 1.0f)
				continue;

			// Convert from NDC to ScreenSpace
			A.position.x = (A.position.x + 1) / 2.0f * m_Width;
			A.position.y = (1 - A.position.y) / 2.0f * m_Height;
			B.position.x = (B.position.x + 1) / 2.0f * m_Width;
			B.position.y = (1 - B.position.y) / 2.0f * m_Height;
			C.position.x = (C.position.x + 1) / 2.0f * m_Width;
			C.position.y = (1 - C.position.y) / 2.0f * m_Height;

			float topLeftX = std::min(A.position.x, std::min(B.position.x, C.position.x));
			float topLeftY = std::max(A.position.y, std::max(B.position.y, C.position.y));
			float bottomRightX = std::max(A.position.x, std::max(B.position.x, C.position.x));
			float bottomRightY = std::min(A.position.y, std::min(B.position.y, C.position.y));

			topLeftX = Clamp(topLeftX, 0.f, float(m_Width));
			topLeftY = Clamp(topLeftY, 0.f, float(m_Height));
			bottomRightX = Clamp(bottomRightX, 0.f, float(m_Width));
			bottomRightY = Clamp(bottomRightY, 0.f, float(m_Height));

			//RENDER LOGIC
			for (int px{ int(topLeftX) }; px < bottomRightX; ++px)
			{
				for (int py{ int(bottomRightY) }; py < topLeftY; ++py)
				{
					if (m_VisBox)
					{
						ColorRGB finalColor{ 1.f,1.f,1.f };

						m_pBackBufferPixels[px + (py * m_Width)] = SDL_MapRGB(m_pBackBuffer->format,
							static_cast<uint8_t>(finalColor.r * 255),
							static_cast<uint8_t>(finalColor.g * 255),
							static_cast<uint8_t>(finalColor.b * 255));

						continue;
					}

					dae::Vector2 pixel{ float(px + 0.5f), float(py + 0.5f) };
					ColorRGB finalColor{ 0.0f, 0.0f, 0.0f };

					// Define the edges of the screen triangle
					const dae::Vector2 AB{ A.position.GetXY(), B.position.GetXY() };
					const dae::Vector2 BC{ B.position.GetXY(), C.position.GetXY() };
					const dae::Vector2 CA{ C.position.GetXY(), A.position.GetXY() };

					const float signedAreaAB{ dae::Vector2::Cross(AB, dae::Vector2{ A.position.GetXY(), pixel}) };
					const float signedAreaBC{ dae::Vector2::Cross(BC, dae::Vector2{ B.position.GetXY(), pixel}) };
					const float signedAreaCA{ dae::Vector2::Cross(CA, dae::Vector2{ C.position.GetXY(), pixel}) };
					const float triangleArea = dae::Vector2::Cross(AB, -CA);

					if (signedAreaAB >= 0 && signedAreaBC >= 0 && signedAreaCA >= 0)
					{
						const float wA{ signedAreaBC / triangleArea };
						const float wB{ signedAreaCA / triangleArea };
						const float wC{ signedAreaAB / triangleArea };

						const float bufferValueZ{ 1 / ((1 / A.position.z) * wA + (1 / B.position.z) * wB + (1 / C.position.z) * wC) }; //interpolated depth (non linear)

						if (bufferValueZ > m_pDepthBufferPixels[px + (py * m_Width)])
							continue;

						m_pDepthBufferPixels[px + (py * m_Width)] = bufferValueZ;

						float interpolatedW{ 1 / ((1 / A.position.w) * wA + (1 / B.position.w) * wB + (1 / C.position.w) * wC) }; // interpolated depth (linear)

						dae::Vector2 uvInterpolated{
							(A.uv / A.position.w) * wA +
							(B.uv / B.position.w) * wB +
							(C.uv / C.position.w) * wC
						};
						uvInterpolated *= interpolatedW;

						Vector3 normalInterpolated{
							(A.normal / A.position.w) * wA +
							(B.normal / B.position.w) * wB +
							(C.normal / C.position.w) * wC
						};
						normalInterpolated *= interpolatedW;
						normalInterpolated.Normalize();

						Vector3 tangentInterpolated{
							(A.tangent / A.position.w) * wA +
							(B.tangent / B.position.w) * wB +
							(C.tangent / C.position.w) * wC
						};
						tangentInterpolated *= interpolatedW;
						tangentInterpolated.Normalize();

						Vector3 viewDirectionInterpolated{
							(A.viewDirection / A.position.w) * wA +
							(B.viewDirection / B.position.w) * wB +
							(C.viewDirection / C.position.w) * wC
						};
						viewDirectionInterpolated *= interpolatedW;
						viewDirectionInterpolated.Normalize();

						Vertex_Out vertexOut{};
						vertexOut.uv = uvInterpolated;
						vertexOut.normal = normalInterpolated;
						vertexOut.tangent = tangentInterpolated;
						vertexOut.viewDirection = viewDirectionInterpolated;


						if (m_VisBuffer)
						{
							const float min{ 0.995f };
							const float max{ 1.0f };
							float depthColor = (Clamp(bufferValueZ, min, max) - min) * (1.0f / (max - min));
							finalColor = { depthColor, depthColor, depthColor };
						}
						else
						{
							finalColor = PixelShading(vertexOut);
						}

						//Update Color in Buffer
						finalColor.MaxToOne();


						m_pBackBufferPixels[px + (py * m_Width)] = SDL_MapRGB(m_pBackBuffer->format,
							static_cast<uint8_t>(finalColor.r * 255),
							static_cast<uint8_t>(finalColor.g * 255),
							static_cast<uint8_t>(finalColor.b * 255));

					}
				}
			}
		}
	}

	SDL_UnlockSurface(m_pBackBuffer);
	SDL_BlitSurface(m_pBackBuffer, 0, m_pFrontBuffer, 0);
	SDL_UpdateWindowSurface(m_pWindow);
}

//Shared
void Renderer::ToggleMode()
{
	m_DirectXMode = !m_DirectXMode;

	SetConsoleTextAttribute(m_hConsole, m_Yellow);

	if (m_DirectXMode)
	{
		std::cout << "Hardware\n";
	}
	else
	{
		std::cout << "Software\n";
	}
	SetConsoleTextAttribute(m_hConsole, m_White);
}
void Renderer::ToggleRot()
{
	m_RotEnabled = !m_RotEnabled;

	SetConsoleTextAttribute(m_hConsole, m_Yellow);

	if(m_RotEnabled)
	{
		std::cout << "Rotation Enabled\n";
	}
	else
	{
		std::cout << "Rotation Disabled\n";
	}
	SetConsoleTextAttribute(m_hConsole, m_White);
}
void Renderer::ToggleCullMode() const
{
	if (m_DirectXMode)
	{
		SetConsoleTextAttribute(m_hConsole, m_Yellow);

		m_pMeshRepresentation[0]->ToggleCullMode();

		if (m_pMeshRepresentation[0]->GetCullMode() == 0)
		{
			std::cout << "Front Culling\n";
		}
		if (m_pMeshRepresentation[0]->GetCullMode() == 1)
		{
			std::cout << "Back Culling\n";
		}
		if (m_pMeshRepresentation[0]->GetCullMode() == 2)
		{
			std::cout << "No Culling\n";
		}
		SetConsoleTextAttribute(m_hConsole, m_White);
	}
}
void Renderer::ToggleBackGround()
{
	m_UniformBackGround = !m_UniformBackGround;

	SetConsoleTextAttribute(m_hConsole, m_Yellow);

	if(m_UniformBackGround)
	{
		std::cout << "Uniformed BackGround\n";
	}
	else
	{
		std::cout << "Unique BackGround\n";
	}
	SetConsoleTextAttribute(m_hConsole, m_White);
}
void Renderer::ToggleFPS(bool FpsOnOff) const
{
	SetConsoleTextAttribute(m_hConsole, m_Yellow);
	if (FpsOnOff)
	{
		std::cout << "Print FPS ON\n";
	}
	else
	{
		std::cout << "Print FPS OFF\n";
	}
	SetConsoleTextAttribute(m_hConsole, m_White);
}

//Hardware
void Renderer::ToggleFireMesh()
{
	if (m_DirectXMode)
	{
		m_FireMeshEnabled = !m_FireMeshEnabled;

		SetConsoleTextAttribute(m_hConsole, m_Green);
		if (m_FireMeshEnabled)
		{
			std::cout << "FireMesh Enabled\n";
		}
		else
		{
			std::cout << "FireMesh Disabled\n";
		}
		SetConsoleTextAttribute(m_hConsole, m_White);
	}
	
}
void Renderer::ToggleSampling() const
{
	if (m_DirectXMode)
	{
		SetConsoleTextAttribute(m_hConsole, m_Green);

		for (auto& Mesh : m_pMeshRepresentation)
		{
			Mesh->ToggleSampling();
		}
		if (m_pMeshRepresentation[0]->GetSampleState() == 0)
		{
			std::cout << "Point\n";
		}
		if (m_pMeshRepresentation[0]->GetSampleState() == 1)
		{
			std::cout << "Linear\n";
		}
		if (m_pMeshRepresentation[0]->GetSampleState() == 2)
		{
			std::cout << "Anisotropic\n";
		}
		SetConsoleTextAttribute(m_hConsole, m_White);
	}
	
}

//Software
void Renderer::ToggleNor()
{
	if (!m_DirectXMode)
	{
		m_NorEnabled = !m_NorEnabled;

		SetConsoleTextAttribute(m_hConsole, m_Magenta);

		if(m_NorEnabled)
		{
			std::cout << "Normals Enabled\n";
		}
		else
		{
			std::cout << "Normals Disabled\n";
		}

		SetConsoleTextAttribute(m_hConsole, m_White);
	}
}
void Renderer::ToggleBuffer()
{
	if (!m_DirectXMode)
	{
		m_VisBuffer = !m_VisBuffer;

		SetConsoleTextAttribute(m_hConsole, m_Magenta);

		if (m_VisBuffer)
		{
			std::cout << "Visual Buffer Enabled\n";
		}
		else
		{
			std::cout << "visual Buffer Disabled\n";
		}
		SetConsoleTextAttribute(m_hConsole, m_White);
	}
	
}
void Renderer::ToggleBoxVisual()
{
	if (!m_DirectXMode)
	{
		m_VisBox = !m_VisBox;

		SetConsoleTextAttribute(m_hConsole, m_Magenta);

		if (m_VisBox)
		{
			std::cout << "Visual Box Enabled\n";
		}
		else
		{
			std::cout << "Visual Box Disabled\n";
		}

		SetConsoleTextAttribute(m_hConsole, m_White);

	}
	
}
void Renderer::ToggleLightMode()
{
	if (!m_DirectXMode)
	{
		SetConsoleTextAttribute(m_hConsole, m_Magenta);

		switch (m_CurrentLightmode)
		{
		case LightMode::Combined:
			m_CurrentLightmode = LightMode::Diffuse;
			std::cout << "Diffuse\n";
			break;
		case LightMode::Diffuse:
			m_CurrentLightmode = LightMode::Specular;
			std::cout << "Specular\n";
			break;
		case LightMode::Specular:
			m_CurrentLightmode = LightMode::ObservedArea;
			std::cout << "ObservedArea\n";
			break;
		case LightMode::ObservedArea:
			m_CurrentLightmode = LightMode::Combined;
			std::cout << "Combined\n";
			break;
		default:
			break;
		}

		SetConsoleTextAttribute(m_hConsole, m_White);
	}
}

void Renderer::VertexTransformationFunctionW4(std::vector<MeshRasterizer>& meshes) const
{
	for (MeshRasterizer& mesh : meshes)
	{
		const Matrix matrix = mesh.worldMatrix * (m_Camera.viewMatrix * m_Camera.projectionMatrix);

		mesh.vertices_out.clear();
		mesh.vertices_out.reserve(mesh.vertices.size());
		for (const Vertex& vertex : mesh.vertices)
		{
			//Projection stage
			Vector4 projectionVertex = matrix.TransformPoint({ vertex.position, 1.0f });

			//Conversion to NDC - Perspective Divide (perspective distortion)
			projectionVertex.x /= projectionVertex.w;
			projectionVertex.y /= projectionVertex.w;
			projectionVertex.z /= projectionVertex.w;

			//convert normal and tangent to worldspace, for rotation -> normalize them after
			const Vector3 normal{ mesh.worldMatrix.TransformVector(vertex.normal).Normalized() };
			const Vector3 tangent{ mesh.worldMatrix.TransformVector(vertex.tangent).Normalized() };

			// Calculate vert world position, for viewDirection
			const Vector3 vertPosition{ mesh.worldMatrix.TransformPoint(vertex.position) };
			const Vector3 viewDir{ m_Camera.origin - vertPosition };

			Vertex_Out vertexOut{ projectionVertex, {}, vertex.uv,
				normal, tangent, viewDir };

			mesh.vertices_out.emplace_back(vertexOut);


		}
	}
}
ColorRGB Renderer::PixelShading(const Vertex_Out& v) const
{
	const Vector3 lightDirection{ .577f, -.577f, .577f };
	const float lightIntensity{ 7.f };
	ColorRGB finalColor{};

	//Base color
	const ColorRGB diffuse{ m_pDiffuseTxt->Sample(v.uv) };
	const ColorRGB lambert{ (lightIntensity * diffuse) / PI };

	//Normals
	const Vector3 binormal{ Vector3::Cross(v.normal, v.tangent) };
	const Matrix tangentSpaceAxis{ v.tangent, binormal, v.normal, Vector3::Zero };
	Vector3 sampledNormal{ m_pNormalTxt->Sample(v.uv).r, m_pNormalTxt->Sample(v.uv).g, m_pNormalTxt->Sample(v.uv).b };
	//sampledNormal /= 255.f; // [0, 255] -> [0,1] //doesnt work with this but is in ppt, already done in sample function
	sampledNormal = 2.f * sampledNormal - Vector3{ 1.f, 1.f, 1.f }; // [0,1] -> [-1, 1]
	const Vector3 normalTangentSpace{ tangentSpaceAxis.TransformVector(sampledNormal) };

	//Change Normals
	Vector3 typeOfNormals{};
	if (m_NorEnabled)
	{
		typeOfNormals = normalTangentSpace;
	}
	else
	{
		typeOfNormals = v.normal;
	}
	const float observedArea{ Vector3::Dot(typeOfNormals, -lightDirection) };

	if (observedArea < 0.0f)
		return {};

	//Phong specular
	const ColorRGB specular{ m_pSpecularTxt->Sample(v.uv) };
	const ColorRGB gloss{ m_pGlossTxt->Sample(v.uv) };
	const float shininess{ 25.f };
	const ColorRGB ambient{ .025f, .025f, .025f };

	const Vector3 reflection{ lightDirection - (2.0f * Vector3::Dot(typeOfNormals, lightDirection) * typeOfNormals) };
	float dotReflectionViewDir{ std::max(0.f, Vector3::Dot(reflection, v.viewDirection)) }; // so dot is never negative
	const ColorRGB phong{ specular * powf(dotReflectionViewDir, gloss.r * shininess) }; //r, g, b are the same so we can just use r (greyscale map)


	switch (m_CurrentLightmode)
	{
	case Renderer::LightMode::Combined:
		finalColor = (lambert + phong + ambient) * observedArea;
		break;
	case Renderer::LightMode::Diffuse:
		finalColor = lambert * observedArea;
		break;
	case Renderer::LightMode::Specular:
		finalColor = phong;
		break;
	case Renderer::LightMode::ObservedArea:
		finalColor = { observedArea, observedArea, observedArea };
		break;
	}
	
	return finalColor;
}