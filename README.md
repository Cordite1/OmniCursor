OmniCursor is a native, modular C++ cursor injection framework for the World of Warcraft Vanilla client (1.12.1).

OmniCursor safely intercepts the game's native rendering pipeline (both Software and Hardware Cursor modes) to allow dynamic, drag-and-drop custom cursors based on in-game context (like hovering over custom gathering nodes).

Installation

Download the latest OmniCursor.dll from the Releases tab.\
Place OmniCursor.dll into the WoW root directory.\
Ensure your custom .png cursors (32x32 pixels) are placed in:  Data/Interface/Cursor/  (the Cursor folder doesn't exist on standard WoW installs, create it)\
Launch the game

Adding Custom Cursors 

OmniCursor dynamically builds its database based on the files in your Cursor folder and the game's LockType.dbc.

Example: Adding a Custom Tree Node Cursor

Create your custom icon and save it as Tree.png (must be exactly 32x32).\
Create your out-of-range icon (usually desaturated) and save it as UnableTree.png.\
Place both in Data/Interface/Cursor/.\
Open your server/client's LockType.dbc. Create a new entry for your node type.\
In the field CursorName of LockType.dbc, type Tree.\
The stock client cannot handle this entry and displays the basic gauntlet cursor only. OmniCursor will automatically intercept the request for Tree and UnableTree and draw your PNGs instead.

Building from Source 

If you want to contribute or build the DLL yourself:\
Requirements\
Visual Studio 2022 \
Target Architecture: x86 (32-bit)\
Compilation Steps\
Ensure the Build Configuration is set to Release | x86.\
Build > Build Solution.\
The compiled .dll will be output to the Release folder.\
(Note: This project relies on MinHook for memory detours and stb_image for PNG loading. Their source files are included in this repository.)\

License \
This project is open-source and available under the MIT License. Feel free to use, modify, and distribute this framework for your own servers and projects!
