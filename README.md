# LuaMacro
An extented Lua engine written in C++, it automates your keyboard/mouse according to an editable Lua script. Besides, the engine also provides screen capturing and template matching functions, it enables the Lua script to execute particular actions based on screen content.

Check out **sample_script.lua** for how to edit the Lua script.

## Prerequisites
* Windows
* VisualStudio 2015
* Lua 5.3
* OpenCV 3.1.0

## Installation
### 1. Setup Lua
* Download dynamic libraries. You could get it from [LuaBinaries](http://luabinaries.sourceforge.net/).
* Header files(.h/.hpp) are available in `LuaMacro\LuaMacro\lua` already, hence you don't have to copy them.
* If your target platform is **x86**, copy `lua53.lib` into `LuaMacro\LuaMacro\lua\lib\x86`, or `LuaMacro\LuaMacro\lua\lib\x64` for **x64**.
### 2. Setup OpenCV
* Download dynamic libraries. You could get it from [opencv.org](https://opencv.org/releases/).
* Copy the header files(.h/.hpp) into `LuaMacro\LuaMacro\opencv\include`.
* If your target platform is **x86**, copy `opencv_world310.lib` into `LuaMacro\LuaMacro\opencv\x86\vc14\lib`, or `LuaMacro\LuaMacro\opencv\x64\vc14\lib` for **x64**.
### 3. Compiling
* Open *LuaMacro.sln* by VisualStudio 2015.
* Select your desired **Solution Configurations**(*Debug* or *Release*) and **Solution Platforms**(*x86* or *x64*).
* Build solution.
* If the compilation is successful, the built executable file will be named **LuaMacro.exe** in `LuaMacro\{Platform}\{Configuration}` (for example if you select *Release* build under *x64*, it will be `LuaMacro\x64\Release`).
### 4. The Last Thing to Do
* Put **config.ini**, **utility.lua**, **lua53.dll**, and **opencv_world310.dll** along with **LuaMacro.exe** in the same folder, then ship them for your use.

## Running
LuaMacro.exe can be run using the following command:
```
LuaMacro.exe {lua_script_path}
```
where `{lua_script_path}` points to the Lua script that you want to execute. After the application launched, to start the script, please press the hotkey defined as `HOTKEY` variable in the Lua script. For example, in **sample_script.lua**, the hotkey is **F8**, which is defined as:
```
HOTKEY = VK_F8
```
where *VK_F8* is defined in **utility.lua**, you can change the hotkey to any key available in that.

Press the hotkey again will stop the script.

## License
[MIT License](https://raw.githubusercontent.com/shucheng-liu/LuaMacro/master/LICENSE)
