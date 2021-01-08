cc = clang-cl
flags = -GS- -Ofast -Oi -W4 -DRELEASE_BUILD
libs = d3d11.lib dxguid.lib d3dcompiler.lib user32.lib kernel32.lib
link_flags = -subsystem:windows -entry:entry -nodefaultlib -out:bin/pong.exe $(libs)

all: main.c
	if not exist bin (mkdir bin)
	$(cc) $(flags) main.c -link $(link_flags)

clean:
	rmdir /q bin
	del /q bin/pong.exe