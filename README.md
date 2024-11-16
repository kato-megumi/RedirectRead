# RedirectRead

## Overview

RedirectRead is a small tool designed to redirect read operations from one folder to another. This allows game save files to be stored in the game folder itself, which is useful when the game is placed on a NAS.

## How to Use

1. Place `Injector.exe` and `FileReadHook.dll` in the game folder.
2. Write the save folder path to `hook.conf`.
3. Run `Injector.exe <game.exe>` or drag the game executable to `Injector.exe`.
