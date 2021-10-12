// dllmain.cpp : Defines the entry point for the DLL application.
#include "pch.h"

#include "MemoryItems.hpp"
#include <iomanip>

#include "Helper.h"

// D3X HOOK DEFINITIONS
typedef HRESULT(__fastcall* IDXGISwapChainPresent)(IDXGISwapChain* pSwapChain, UINT SyncInterval, UINT Flags);
typedef void(__stdcall* ID3D11DrawIndexed)(ID3D11DeviceContext* pContext, UINT IndexCount, UINT StartIndexLocation, INT BaseVertexLocation);

//D3D11 Global Objects
ID3D11DeviceContext* pContext = nullptr;
ID3D11Device* pDevice = nullptr;
ID3D11RenderTargetView* pRenderTargetView;
static IDXGISwapChain* pSwapChain = nullptr;
static WNDPROC OriginalWindowProcHandler = nullptr;
HWND window = nullptr;
IDXGISwapChainPresent fnIDXGISwapChainPresent;
DWORD_PTR* pDeviceContextVTable = nullptr;

//Global Objects
InventoryHeader header;
MemItem* memItems;

#pragma warning(disable:4996)
const void SetupConsole()
{
	AllocConsole();
	SetConsoleTitleA("Ds3InventoryDumper Screen Overlay");
	freopen("CONOUT$", "w", stdout);
	freopen("CONOUT$", "w", stderr);
	freopen("CONIN$", "r", stdin);
}

LRESULT CALLBACK DXGIMsgProc(HWND hwnd, UINT uMsg, WPARAM wParam, LPARAM lParam) { return DefWindowProc(hwnd, uMsg, wParam, lParam); }
extern LRESULT ImGui_ImplWin32_WndProcHandler(HWND hWnd, UINT msg, WPARAM wParam, LPARAM lParam);

const void GetPresent()
{
	WNDCLASSEXA wc = { sizeof(WNDCLASSEX), CS_CLASSDC, DXGIMsgProc, 0, 0, GetModuleHandleA(NULL), nullptr, nullptr, nullptr, nullptr, "DX", nullptr };
	RegisterClassExA(&wc);
	HWND hwnd = CreateWindowA("DX", nullptr, WS_OVERLAPPEDWINDOW, 100, 100, 300, 300, nullptr, nullptr, wc.hInstance, nullptr);

	DXGI_SWAP_CHAIN_DESC sd = { 0 };
	sd.BufferCount = 1;
	sd.BufferDesc.Width = 2;
	sd.BufferDesc.Height = 2;
	sd.BufferDesc.Format = DXGI_FORMAT_R8G8B8A8_UNORM;
	sd.BufferDesc.RefreshRate.Numerator = 60;
	sd.BufferDesc.RefreshRate.Denominator = 1;
	sd.BufferUsage = DXGI_USAGE_RENDER_TARGET_OUTPUT;
	sd.OutputWindow = hwnd;
	sd.SampleDesc.Count = 1;
	sd.SampleDesc.Quality = 0;
	sd.Windowed = true;
	sd.Flags = DXGI_SWAP_CHAIN_FLAG_ALLOW_MODE_SWITCH;

	D3D_FEATURE_LEVEL featureLevelsRequest = D3D_FEATURE_LEVEL_11_0;
	uint32_t numFeatureLevelsRequested = 1;
	D3D_FEATURE_LEVEL supportedFeatureLevels;

	IDXGISwapChain* swapChain = nullptr;
	ID3D11Device* device = nullptr;
	ID3D11DeviceContext* deviceContext = nullptr;

	auto result = D3D11CreateDeviceAndSwapChain(
		nullptr,
		D3D_DRIVER_TYPE_HARDWARE,
		nullptr,
		0,
		&featureLevelsRequest,
		numFeatureLevelsRequested,
		D3D11_SDK_VERSION,
		&sd,
		&swapChain,
		&device,
		&supportedFeatureLevels,
		&deviceContext
	);

	if (FAILED(result))
	{
		std::cout << "FAILED to hook with DirectX SwapChain\n";
		return;
	}

	DWORD_PTR* pSwapChainVtable = nullptr;
	pSwapChainVtable = (DWORD_PTR*)swapChain;
	pSwapChainVtable = (DWORD_PTR*)pSwapChainVtable[0];
	fnIDXGISwapChainPresent = (IDXGISwapChainPresent)(DWORD_PTR)pSwapChainVtable[8];
	swapChain->Release();
	device->Release();
	deviceContext->Release();
	Sleep(2000);
}

HRESULT GetDeviceAndCtxFromSwapchain(IDXGISwapChain* pSwapChain, ID3D11Device** ppDevice, ID3D11DeviceContext** ppContext)
{
	HRESULT ret = pSwapChain->GetDevice(__uuidof(ID3D11Device), (PVOID*)ppDevice);

	if (SUCCEEDED(ret))
		(*ppDevice)->GetImmediateContext(ppContext);

	return ret;
}

static bool gShowMenu = false;
LRESULT CALLBACK mWndProc(HWND hwnd, UINT msg, WPARAM wparam, LPARAM lparam)
{
	ImGuiIO& io = ImGui::GetIO();
	POINT mPos;
	GetCursorPos(&mPos);
	ScreenToClient(window, &mPos);
	ImGui::GetIO().MousePos.x = mPos.x;
	ImGui::GetIO().MousePos.y = mPos.y;

	if (msg == WM_KEYUP)
	{
		if (wparam == VK_INSERT)
		{
			gShowMenu = !gShowMenu;
		}
	}

	if (gShowMenu)
	{
		ImGui_ImplWin32_WndProcHandler(hwnd, msg, wparam, lparam);
		return true;
	}

	return CallWindowProc(OriginalWindowProcHandler, hwnd, msg, wparam, lparam);
}

//Python Objects
PyObject* pName, *pModule, *pFunc, *pValue, *pNum1, *pNum2 = nullptr;
int num1 = 10;
int num2 = 10;
void LoadPyModule()
{
	if (pModule)
	{
		PyImport_ReloadModule(pModule);
	}
	else
	{
		pName = PyUnicode_FromString("myds3code");
		pModule = PyImport_Import(pName);
		Py_DECREF(pName);
	}

	if (pModule != nullptr)
	{
		pFunc = PyObject_GetAttrString(pModule, "calcular");
	}
}
void CallPyFunction(PyObject* head, PyObject* chest, PyObject* hands, PyObject* legs)
{
	PyObject* pArgs = PyTuple_New(4);
	
	PyTuple_SetItem(pArgs, 0, head);
	PyTuple_SetItem(pArgs, 1, chest);
	PyTuple_SetItem(pArgs, 2, hands);
	PyTuple_SetItem(pArgs, 3, legs);

	pValue = PyObject_CallObject(pFunc, pArgs);
	Py_DECREF(pArgs);
}

HMODULE thisModule;
bool presentInitialized = false;

Item cHead, cChest, cHands, cLegs = { 0 };
PyObject* pHead, *pChest, *pHands, *pLegs = nullptr;

HRESULT __fastcall Present(IDXGISwapChain* swapChain, UINT SyncInterval, UINT Flags)
{
	if (!presentInitialized)
	{
		GetDeviceAndCtxFromSwapchain(swapChain, &pDevice, &pContext);
		pSwapChain = swapChain;
		DXGI_SWAP_CHAIN_DESC sd;
		pSwapChain->GetDesc(&sd);

		ImGui::CreateContext();
		ImGuiIO& io = ImGui::GetIO();
		io.ConfigFlags |= ImGuiConfigFlags_NavEnableKeyboard;
		window = sd.OutputWindow;

		OriginalWindowProcHandler = (WNDPROC)SetWindowLongPtr(window, GWLP_WNDPROC, (LONG_PTR)mWndProc);

		ImGui_ImplWin32_Init(window);
		ImGui_ImplDX11_Init(pDevice, pContext);
		ImGui::GetIO().ImeWindowHandle = window;

		ID3D11Texture2D* backBuffer;
		pSwapChain->GetBuffer(0, __uuidof(ID3D11Texture2D), (LPVOID*)&backBuffer);
		if (backBuffer)
		{
			pDevice->CreateRenderTargetView(backBuffer, nullptr, &pRenderTargetView);
			backBuffer->Release();
		}
		presentInitialized = true;
		
		Py_Initialize();
		LoadPyModule();
	}

	ImGui_ImplWin32_NewFrame();
	ImGui_ImplDX11_NewFrame();
	ImGui::NewFrame();

	//use imgui here
	if (gShowMenu)
	{
		ImGui::Begin("Ds3 Inventory Dumper");
		
		std::vector<Item> head;
		PyObject* p_Head = PyList_New(0);

		std::vector<Item> armor;
		PyObject* p_Chest = PyList_New(0);

		std::vector<Item> gloves;
		PyObject* p_Hands = PyList_New(0);

		std::vector<Item> pants;
		PyObject* p_Legs = PyList_New(0);

		std::vector<Item> allItems;
		PyObject* p_All = PyList_New(0);

		for (auto i = 0; i < header.size; i++)
		{
			bool isInDB = GetItemsDB().contains(memItems[i].gid);
			if (isInDB)
			{
				Item item = GetItemsDB().at(memItems[i].gid);
				allItems.push_back(item);

				if (item.name.find("Helm") != std::string::npos ||
					item.name.find("Hood") != std::string::npos ||
					item.name.find("Veil") != std::string::npos ||
					item.name.find("Hat")  != std::string::npos)
				{
					head.push_back(item);
					auto pHead = ItemToPyDict(item);
					PyList_Insert(p_Head, 1, pHead);
					Py_DECREF(pHead);
				}
				if (item.name.find("Armor") != std::string::npos || item.name.find("Robe") != std::string::npos)
				{
					armor.push_back(item);
					auto pItem = ItemToPyDict(item);
					PyList_Insert(p_Chest, 1, pItem);
					Py_DECREF(pItem);
				}
				if (item.name.find("Gauntlet") != std::string::npos || item.name.find("Glove") != std::string::npos)
				{
					gloves.push_back(item);
					auto pItem = ItemToPyDict(item);
					PyList_Insert(p_Hands, 1, pItem);
					Py_DECREF(pItem);
				}
				if (item.name.find("Trouser") != std::string::npos || item.name.find("Legging") != std::string::npos)
				{
					pants.push_back(item);
					auto pItem = ItemToPyDict(item);
					PyList_Insert(p_Legs, 1, pItem);
					Py_DECREF(pItem);
				}
			}
		}
		CallPyFunction(p_Head, p_Chest, p_Hands, p_Legs);
		pHead = PyTuple_GetItem(pValue, 0);
		pChest = PyTuple_GetItem(pValue, 1);
		pHands = PyTuple_GetItem(pValue, 2);
		pLegs = PyTuple_GetItem(pValue, 3);

		cHead = ItemFromPyDIct(pHead);
		cChest = ItemFromPyDIct(pChest);
		cHands = ItemFromPyDIct(pHands);
		cLegs = ItemFromPyDIct(pLegs);

		Py_DECREF(pHead);
		Py_DECREF(pChest);
		Py_DECREF(pHands);
		Py_DECREF(pLegs);
		Py_DECREF(pValue);

		if(ImGui::Button("Reload Python Script"))
		{
			LoadPyModule();
		}
		ImGui::Text("Size of allItems %d", allItems.size());

		if (!cHead.name.empty())
		{
			ImGui::Text("Head:   %s %.1f %.1f", cHead.name.c_str(), cHead.defense, cHead.weight);
		}

		if (!cChest.name.empty())
		{
			ImGui::Text("Armor:  %s %.1f %.1f", cChest.name.c_str(), cChest.defense, cChest.weight);
		}

		if (!cHands.name.empty())
		{
			ImGui::Text("Gloves: %s %.1f %.1f", cHands.name.c_str(), cHands.defense, cHands.weight);
		}

		if (!cLegs.name.empty())
		{
			ImGui::Text("Pants:  %s %.1f %.1f", cLegs.name.c_str(), cLegs.defense, cLegs.weight);
		}

		ImGui::End();
	}
	//
	ImGui::EndFrame();
	ImGui::Render();
	pContext->OMSetRenderTargets(1, &pRenderTargetView, nullptr);
	ImGui_ImplDX11_RenderDrawData(ImGui::GetDrawData());

	return fnIDXGISwapChainPresent(swapChain, SyncInterval, Flags);
}

void DetourDirectXPresent()
{
	DetourTransactionBegin();
	DetourUpdateThread(GetCurrentThread());
	DetourAttach(&(LPVOID&)fnIDXGISwapChainPresent, (PBYTE)Present);
	DetourTransactionCommit();
}

int WINAPI main()
{
	SetupConsole();

	std::cout << "...\n";

	auto currentPath = std::filesystem::current_path();
	LoadItemsDB(currentPath.string() + "\\items.txt");

	for (auto i = 0ui64; i < 4096; i++)
	{
		void* memAddr = (void*)(HEADER_SEARCH + (i * 8ui64));
		InventoryHeader* tempHeader = (InventoryHeader*)memAddr;
		if (tempHeader->validate(HEADER_SEARCH + (i * 8ui64)))
		{
			header = *tempHeader;
			break;
		}
	}
	if (header.size == 0)
	{
		std::cout << "Failed to find Inventory\n";
		//return -1;
	}
	std::cout << "Header Begin: " << header.beginAddr << '\n';
	memItems = (MemItem*)header.beginAddr + 16;

	GetPresent();
	DetourDirectXPresent();
	while (!presentInitialized)
	{
		Sleep(500);
	}
	std::cout << "Finished hooking DirectX\n";
	Sleep(4000);
	return 0;
}

BOOL APIENTRY DllMain(HMODULE hModule,
	DWORD  ul_reason_for_call,
	LPVOID lpReserved
)
{
	thisModule = hModule;
	switch (ul_reason_for_call)
	{
	case DLL_PROCESS_ATTACH: {
		DisableThreadLibraryCalls(hModule);
		CreateThread(NULL, NULL, (LPTHREAD_START_ROUTINE)main, NULL, NULL, NULL);
	}
	case DLL_THREAD_ATTACH:
	{
	}
	case DLL_THREAD_DETACH:
	case DLL_PROCESS_DETACH:
		break;
	}
	return TRUE;
}
