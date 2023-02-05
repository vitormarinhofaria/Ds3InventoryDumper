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
std::thread g_python_thread;

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
int hotKeyId = 857;
LRESULT CALLBACK mWndProc(HWND hwnd, UINT msg, WPARAM wparam, LPARAM lparam)
{
	ImGuiIO& io = ImGui::GetIO();
	POINT mPos;
	GetCursorPos(&mPos);
	ScreenToClient(window, &mPos);
	ImGui::GetIO().MousePos.x = static_cast<float>(mPos.x);
	ImGui::GetIO().MousePos.y = static_cast<float>(mPos.y);

	if (msg == WM_KEYUP)
	{
		if (wparam == VK_INSERT)
		{
			gShowMenu = !gShowMenu;
		}
	}
	if (msg == WM_HOTKEY) {
		if (wparam == hotKeyId) {
			SetShouldReloadPython(true);
		}
	}

	if (gShowMenu)
	{
		ImGui_ImplWin32_WndProcHandler(hwnd, msg, wparam, lparam);
	}

	return CallWindowProc(OriginalWindowProcHandler, hwnd, msg, wparam, lparam);
}

HMODULE thisModule;
bool presentInitialized = false;
Item cHead, cChest, cHands, cLegs = { .gid = 0, .name = "N/A", .defense = 0, .weight = 0 };
std::vector<Item>* head, * chest, * hands, * legs = nullptr;
PyData g_Python;

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
		
		head = new std::vector<Item>();
		chest = new std::vector<Item>();
		hands = new std::vector<Item>();
		legs = new std::vector<Item>();

		InitPythonData initData{};
		initData.head = &cHead;
		initData.chest = &cChest;
		initData.gloves = &cHands;
		initData.legs = &cLegs;
		initData.head_vec = head;
		initData.chest_vec = chest;
		initData.gloves_vec = hands;
		initData.legs_vec = legs;
		initData.module_path = ModPath.string();

		g_Python.InitPython(initData);
		/*g_python_thread = std::thread(
			[initData]() {
				using namespace std::chrono_literals;
		std::this_thread::sleep_for(2s);
		while (true) {
			TickPython();
			std::this_thread::sleep_for(200ms);
		}
			});*/

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

		head->clear();
		chest->clear();
		hands->clear();
		legs->clear();

		std::vector<Item> allItems;

		for (auto i = 0; i < header.size; i++)
		{
			bool isInDB = GetItemsDB().contains(memItems[i].gid);
			if (isInDB)
			{
				Item item = GetItemsDB().at(memItems[i].gid);
				allItems.push_back(item);
				std::string item_name = item.name;

				if (item_name.find("Helm") != std::string::npos ||
					item_name.find("Hood") != std::string::npos ||
					item_name.find("Veil") != std::string::npos ||
					item_name.find("Hat") != std::string::npos)
				{
					head->push_back(item);
				}
				if (item_name.find("Armor") != std::string::npos || item_name.find("Robe") != std::string::npos)
				{
					chest->push_back(item);
				}
				if (item_name.find("Gauntlet") != std::string::npos || item_name.find("Glove") != std::string::npos)
				{
					hands->push_back(item);
				}
				if (item_name.find("Trouser") != std::string::npos || item_name.find("Legging") != std::string::npos)
				{
					legs->push_back(item);
				}
			}
		}
		g_Python.Update();
		ImGui::Text("Press CTRL+SHIFT+F11 to reload the Python script");

		ImGui::Text("Size of allItems %d", allItems.size());
		//if (get_py_mutex().try_lock_shared()) {
		ImGui::Text("Head:   %s %.1f %.1f", cHead.name, cHead.defense, cHead.weight);
		ImGui::Text("Armor:  %s %.1f %.1f", cChest.name, cChest.defense, cChest.weight);
		ImGui::Text("Gloves: %s %.1f %.1f", cHands.name, cHands.defense, cHands.weight);
		ImGui::Text("Pants:  %s %.1f %.1f", cLegs.name, cLegs.defense, cLegs.weight);
		//get_py_mutex().unlock_shared();
	//}

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
	std::cout << "You can now go back to the game and press 'Insert' to toogle the overlay.\n";
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
