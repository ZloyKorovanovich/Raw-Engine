lflags = -Lext/lib -lglfw3 -lvulkan-1 -lgdi32 -lkernel32 -luser32 -lshell32
cflags = -std=c99 -Wconversion -Wsign-conversion -Wall -Iext/inc

vs_cflags = -spirv -Zpr -T vs_6_0 -E vs_main
fs_cflags = -spirv -Zpr -T ps_6_0 -E fs_main
cs_cflags = -spirv -Zpr -T cs_6_0 -E cs_main

.PHONY: shaders
.PHONY: build
.PHONY: run

# build whole project
build: 
	gcc $(cflags) src/*.c src/**/*.c -o ./out/bin/sharky.exe $(lflags)

run:
	out/bin/sharky.exe

shaders:
	dxc $(vs_cflags) shaders/demo.hlsl -Fo out/data/demo_v.spv
	dxc $(fs_cflags) shaders/demo.hlsl -Fo out/data/demo_f.spv
	dxc $(cs_cflags) shaders/demo.hlsl -Fo out/data/demo_c.spv
	dxc $(vs_cflags) shaders/blit.hlsl -Fo out/data/blit_v.spv
	dxc $(fs_cflags) shaders/blit.hlsl -Fo out/data/blit_f.spv
	dxc $(vs_cflags) shaders/axis.hlsl -Fo out/data/axis_v.spv
	dxc $(fs_cflags) shaders/axis.hlsl -Fo out/data/axis_f.spv
	dxc $(vs_cflags) shaders/planet.hlsl -Fo out/data/planet_v.spv
	dxc $(fs_cflags) shaders/planet.hlsl -Fo out/data/planet_f.spv
	dxc $(vs_cflags) shaders/default.hlsl -Fo out/data/default_v.spv
	dxc $(fs_cflags) shaders/default.hlsl -Fo out/data/default_f.spv
