# Ds3InventoryDumper

## It's a tool for reading a Dark Souls 3 player inventory and selecting some itens based on a user written Python script

The project was created with the intention to practice and learn about analyzing another process memory, hooking in an existing DirectX swapchain, and loading and executing Python code from a C++ program

### Compiling

You will need Visual Studio with the C++ workload installed and VCPKG for installing dependencies.

Install the dependencies with [VCPKG](https://github.com/microsoft/vcpkg)

``` powershell
./vcpkg.exe install imgui[win32-binding,dx11-binding] detours python3 --triplet=x64-windows
```

Then open the Ds3InventoryDumper.sln on Visual Studio and press F7 to compile all the projects in the Solution.

### Usage

Open the Dark Souls 3 game and enter you saved game. Once you're loaded into the map, run the Ds3InventoryDumper.exe as Administrator.
