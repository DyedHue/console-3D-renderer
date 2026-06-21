[Check instructions](#how-to-download)
<img width="1919" height="1024" alt="Screenshot 2026-02-23 224050" src="https://github.com/user-attachments/assets/01d0b076-f040-4f56-be10-c002b06fe22d" /><img width="1919" height="1028" alt="Screenshot 2026-02-23 223936" src="https://github.com/user-attachments/assets/9cc82d44-e4dc-4de5-882b-abfca2907ca2" /><img width="1919" height="1022" alt="Screenshot 2026-02-23 223958" src="https://github.com/user-attachments/assets/dad0c5de-62dd-48e6-adbd-ef2f9a427dee" /><img width="1919" height="1024" alt="Screenshot 2026-02-23 224050" src="https://github.com/user-attachments/assets/dfbc3d1a-e104-4af2-b2a9-54df23cd8f6b" /><img width="1919" height="1023" alt="Screenshot 2026-02-23 223816" src="https://github.com/user-attachments/assets/dd0ce8f4-b3d8-4040-889c-608742e4d245" /><img width="1423" height="869" alt="Screenshot 2026-02-23 211212" src="https://github.com/user-attachments/assets/bc8bd3a0-fabb-4c6d-8b95-326082f4caaa" />

# How to download:
Check the latest [Release](https://github.com/DyedHue/console-3D-renderer/releases) build, download the zip and extract it.  
Or,  
Build it yourself: Go to [Source.cpp](Source.cpp), copy the entire file, compile it to an exe (use maximum compiler optimization). It's recommended to keep the exe contained in a folder of its own.
# How to use:
After running the exe, use control and scroll bar or `+`/`-` to adjust font size to make the screen fully visible.  
  
Use `wasd` keys to move around, `arrow keys` to look around, `space` for going up, `v` for going down, `e` to place blocks and `q` to break them.  
You can press `o` to save your current position and look direction which will be loaded the next time you run the exe.
# How to import models:
You can import 3D models as `.obj` files only. Keep your obj file in the same directory as the exe.  
1. Go to `Models Positions.txt` within the same directory as the exe.
2. First type the name of the file (including `.obj`).
3. After a space, type the x, y and z coordinates (positive z is up) of your desired position to place the model at, each separated by space.
4. After another space, type the **scale** of the object (1 means original size).

More briefly: `{filename.obj} {x} {y} {z} {scale}`  
  
For example : `MyModel.obj 0 3.5 2.89 2` is going to place an object in (x, y, z) = (0, 3.5, 2.89) with 2 times its original size.

You can include 1 model in one line. Any line works. Any number of model works.  
You can make a comment line by starting with a `#`.
  
# How to change settings:
Go to `Settings.txt` within the same directory as the exe.  
You will see some variables set to a certain value there. You can change those values to your liking.
