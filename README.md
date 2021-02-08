# picofract
Mandelbrot Set rendering demo for Raspberry Pi Pico microcontroller with Pico Display Pack.

## Building
If you already have the Pimoroni SDK building, you can add a "projects" folder to pimoroni-pico-main. Put the "picofract" folder into that.

Add the following to the end of pimoroni-pico-main/CMakeLists.txt:

```add_subdirectory(projects)```

Create a new CMakeLists.txt in the "projects" folder, containing the following:

```add_subdirectory(picofract)```

You can then re-run cmake and make as described in the Pimoroni SDK instructions to build the project.

## Controls
The buttons on the Display Pack work as follow:

- X/Y - Zoom
- A+X/Y - Scroll horizontally
- B+X/Y - Scroll vertically
- X+Y - Generate new colour palette