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

std::filesystem::path ModPath{};

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

void CheckImGuiConfig() {
	if (!std::filesystem::exists("imgui.ini")) {
		std::string imgui_ini = R"ini([Window][Debug##Default]
			Pos = 60, 60
			Size = 400, 400
			Collapsed = 0

			[Window][Ds3 Inventory Dumper]
			Pos = 1283, 3
			Size = 320, 145
			Collapsed = 0)ini";
		std::ofstream imgui_ini_file("imgui.ini");
		imgui_ini_file << imgui_ini;
		imgui_ini_file.flush();
		imgui_ini_file.close();
	}
}

static bool gShowMenu = false;
void LoadPyModule(); //Foward declaration
int hotKeyId = 857;
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
	if (msg == WM_HOTKEY) {
		if (wparam == hotKeyId) {
			LoadPyModule();
		}
	}

	if (gShowMenu)
	{
		ImGui_ImplWin32_WndProcHandler(hwnd, msg, wparam, lparam);
	}

	return CallWindowProc(OriginalWindowProcHandler, hwnd, msg, wparam, lparam);
}

//Python Objects
PyObject* pName, * pModule, * pFunc, * pValue, * pNum1, * pNum2 = nullptr;

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
		std::cout << __FUNCTION__ << ":" << __LINE__ << " Loaded calcular from myds3code.py\n";
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

Item cHead, cChest, cHands, cLegs = {};
PyObject* pHead, * pChest, * pHands, * pLegs = nullptr;

std::string pythonCode;

HRESULT __fastcall Present(IDXGISwapChain* swapChain, UINT SyncInterval, UINT Flags)
{
	if (!presentInitialized)
	{
		std::cout << __FUNCTION__ << ":" << __LINE__ << " Initializing present method\n";
		GetDeviceAndCtxFromSwapchain(swapChain, &pDevice, &pContext);
		pSwapChain = swapChain;
		DXGI_SWAP_CHAIN_DESC sd;
		pSwapChain->GetDesc(&sd);

		ImGui::CreateContext();
		ImGuiIO& io = ImGui::GetIO();
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
			std::cout << __FUNCTION__ << ":" << __LINE__ << " pRenderTargetView: " << std::hex << pRenderTargetView << std::dec << "\n";
		}
		presentInitialized = true;
		std::cout << __FUNCTION__ << ":" << __LINE__ << " pSwapChain: " << std::hex << pSwapChain << " window: " << window << "\n" << std::dec;

		PyPreConfig preConfig{};
		PyPreConfig_InitPythonConfig(&preConfig);
		Py_PreInitialize(&preConfig);

		PyConfig pyConfig{};
		PyConfig_InitPythonConfig(&pyConfig);

		std::string pythonHome = ModPath.string() + "/python3";
		wchar_t wPyHome[MAX_PATH] = { 0 };
		MultiByteToWideChar(CP_UTF8, MB_PRECOMPOSED, pythonHome.c_str(), pythonHome.size(), wPyHome, sizeof(wPyHome));
		pyConfig.home = wPyHome;
		PyWideStringList_Append(&pyConfig.module_search_paths, wPyHome);

		wchar_t wModulePath[MAX_PATH] = { 0 };
		MultiByteToWideChar(CP_UTF8, MB_PRECOMPOSED, ModPath.string().c_str(), ModPath.string().size(), wModulePath, sizeof(wModulePath));
		PyWideStringList_Append(&pyConfig.module_search_paths, wModulePath);

		std::wstring wLibsPath = std::wstring(wPyHome) + L"/Lib";
		PyWideStringList_Append(&pyConfig.module_search_paths, wLibsPath.data());
		pyConfig.module_search_paths_set = 1;

		pyConfig.optimization_level = 2;

		auto status = Py_InitializeFromConfig(&pyConfig);
		if (PyStatus_IsError(status)) {
			std::cout << status.err_msg << '\n';
		}
		else {
			std::cout << __FUNCTION__ << ":" << __LINE__ << " Python Initialized\n";
		}
		LoadPyModule();
		RegisterHotKey(window, hotKeyId, MOD_SHIFT | MOD_CONTROL | MOD_NOREPEAT, VK_F11);
	}

	ImGui_ImplWin32_NewFrame();
	ImGui_ImplDX11_NewFrame();
	ImGui::NewFrame();
	//use imgui here
	if (gShowMenu)
	{
		ImGui::Begin("Ds3 Inventory Dumper");
		std::wstring s = L"FPS: " + std::to_wstring(ImGui::GetIO().Framerate);
		SetConsoleTitleW(s.c_str());

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
					item.name.find("Hat") != std::string::npos)
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
		Py_DECREF(p_Head);
		Py_DECREF(p_Chest);
		Py_DECREF(p_Hands);
		Py_DECREF(p_Legs);
		if (!head.empty()) {
			pHead = PyTuple_GetItem(pValue, 0);
			cHead = ItemFromPyDict(pHead);
			Py_DECREF(pHead);
		}
		if (!armor.empty()) {
			pChest = PyTuple_GetItem(pValue, 1);
			cChest = ItemFromPyDict(pChest);
			Py_DECREF(pChest);
		}
		if (!gloves.empty()) {
			pHands = PyTuple_GetItem(pValue, 2);
			cHands = ItemFromPyDict(pHands);
			Py_DECREF(pHands);
		}
		if (!pants.empty()) {
			pLegs = PyTuple_GetItem(pValue, 3);
			cLegs = ItemFromPyDict(pLegs);
			Py_DECREF(pLegs);
		}

		if (pValue) {
			Py_DECREF(pValue);
		}

		ImGui::Text("Press CTRL+SHIFT+F11 to reload the Python script");

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
	CheckImGuiConfig();
	std::cout << "...\n";

	char file_name[256];
	GetModuleFileNameA(thisModule, file_name, 256);
	auto fileNamePath = std::filesystem::path(file_name);
	ModPath = fileNamePath.parent_path();
	std::cout << ModPath.string() << "\n";

	auto itemsPath = ModPath.string() + "/items.txt";
	std::cout << "Loading items database...\n";
	LoadItemsDB(itemsPath);
	std::cout << "Loaded items database.\n";

	std::cout << "Begin scanning for inventory header...\n";
	header = ScanEx(nullptr);
	//header.size = (header.endAddr - header.beginAddr) / sizeof(MemItem);
	std::cout << "Header Begin: " << std::hex << header.beginAddr << " end: " << header.endAddr << std::dec << " size: " << header.size << '\n';
	memItems = (MemItem*)header.beginAddr + 16;

	GetPresent();
	DetourDirectXPresent();
	while (!presentInitialized)
	{
		Sleep(500);
	}
	std::cout << "Finished hooking DirectX\n";
	std::cout << "You can now go back to the game and press 'Insert' to toogle the overlay\n";
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
