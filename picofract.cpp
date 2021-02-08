/*
MIT License

Copyright (c) 2021 James Sutherland

Permission is hereby granted, free of charge, to any person obtaining a copy
of this software and associated documentation files (the "Software"), to deal
in the Software without restriction, including without limitation the rights
to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
copies of the Software, and to permit persons to whom the Software is
furnished to do so, subject to the following conditions:

The above copyright notice and this permission notice shall be included in all
copies or substantial portions of the Software.

THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE
SOFTWARE.
*/

#include "pico/stdlib.h"
#include "pico/multicore.h"

#include "pico_display.hpp"

using namespace pimoroni;

#define FP_FRAC_BITS 23
#define FP_ONE ((int32_t)(1 << FP_FRAC_BITS))
#define FP_CONST(x) ((int32_t)((x)*FP_ONE + ((x) >= 0 ? 0.5 : -0.5)))
#define FP_FROM_INT(x) ((x) << FP_FRAC_BITS)
#define FP_MUL(x, y) ((int32_t)(((int64_t)(x) * (int64_t)(y)) >> FP_FRAC_BITS))
#define FP_DIV(x, y) ((int32_t)(((int64_t)(x) << FP_FRAC_BITS) / (int64_t)(y)))
typedef int32_t fixed;

uint16_t buffer[PicoDisplay::WIDTH * PicoDisplay::HEIGHT];
PicoDisplay pico_display(buffer);

void RenderMandelbrotSet(int iRenderWidth, int iScreenY, int iRenderHeight, fixed fpMinCX, fixed fpMaxCX, fixed fpMinCY, fixed fpMaxCY, int iIterations, int iPixelChunkSize, uint16_t *pPalette);
void CalculateBounds(fixed fpScreenRatio, fixed fpCentreX, fixed fpCentreY, fixed fpWidth, fixed &fpMinCX_out, fixed &fpMaxCX_out, fixed &fpMinCY_out, fixed &fpMaxCY_out);
void CreateRandomPalette(uint16_t *pPalette, int iPaletteSize);

void SetLowDetail(int &iIterations_out, int &iPixelChunkSize_out)
{
    iIterations_out = 80;
    iPixelChunkSize_out = 4;
}

void SetHighDetail(int &iIterations_out, int &iPixelChunkSize_out)
{
    iIterations_out = 200;
    iPixelChunkSize_out = 1;
}

struct ThreadParams
{
    int iRenderWidth;
    int iScreenY;
    int iRenderHeight;
    fixed fpMinCX;
    fixed fpMaxCX;
    fixed fpMinCY;
    fixed fpMaxCY;
    int iIterations;
    int iPixelChunkSize;
    uint16_t *pPalette;

    bool bCore1Done;
    bool bCore1Start;

} g_threadParams;

void WaitFor(bool &bFlag)
{
    while (!bFlag)
    {
        sleep_ms(1);
    }
    bFlag = false;
}

void Core1Main()
{
    while (true)
    {
        WaitFor(g_threadParams.bCore1Start);

        RenderMandelbrotSet(g_threadParams.iRenderWidth, g_threadParams.iScreenY, g_threadParams.iRenderHeight, g_threadParams.fpMinCX, g_threadParams.fpMaxCX, g_threadParams.fpMinCY, g_threadParams.fpMaxCY, g_threadParams.iIterations, g_threadParams.iPixelChunkSize, g_threadParams.pPalette);
        g_threadParams.bCore1Done = 1;
    }
}

int main()
{
    pico_display.init();
    pico_display.set_backlight(100);

    srand(time_us_32());

    const int iRenderWidth = PicoDisplay::WIDTH;
    const int iRenderHeight = PicoDisplay::HEIGHT;
    const fixed fpScreenRatio = FP_DIV(FP_FROM_INT(PicoDisplay::HEIGHT), FP_FROM_INT(PicoDisplay::WIDTH));

    pico_display.set_pen(0, 0, 0);
    pico_display.clear();
    pico_display.update();

    // Create a palette
    const int iPaletteSize = 256;
    uint16_t *pPalette = (uint16_t *)malloc(sizeof(uint16_t) * iPaletteSize);

    CreateRandomPalette(pPalette, iPaletteSize);

    // Start second thread, don't let it start rendering yet
    g_threadParams.bCore1Done = false;
    g_threadParams.bCore1Start = false;
    multicore_launch_core1(Core1Main);

    fixed fpCentreX = FP_CONST(-0.5);
    fixed fpCentreY = FP_CONST(0.0);
    fixed fpWidth = FP_CONST(4.0);

    // High detail for first render at startup
    int iIterations;
    int iPixelChunkSize;
    SetHighDetail(iIterations, iPixelChunkSize);

    // After ~1000ms, re-render in high detail
    const int iRefineTime = 1000;
    int iRefineCounter = iRefineTime;
    bool bPaletteGenerationLatch = false;

    while (true)
    {
        fixed fpMinCX, fpMaxCX, fpMinCY, fpMaxCY;
        CalculateBounds(fpScreenRatio, fpCentreX, fpCentreY, fpWidth, fpMinCX, fpMaxCX, fpMinCY, fpMaxCY);

        // Set up parameters for second thread to render first half of screen
        g_threadParams.iRenderWidth = iRenderWidth;
        g_threadParams.iScreenY = 0;
        g_threadParams.iRenderHeight = iRenderHeight / 2;
        g_threadParams.fpMinCX = fpMinCX;
        g_threadParams.fpMaxCX = fpMaxCX;
        g_threadParams.fpMinCY = fpMinCY;
        g_threadParams.fpMaxCY = (fpMaxCY + fpMinCY) / 2;
        g_threadParams.iIterations = iIterations;
        g_threadParams.iPixelChunkSize = iPixelChunkSize;
        g_threadParams.pPalette = pPalette;
        g_threadParams.bCore1Done = false;

        // Tell thread to start
        g_threadParams.bCore1Start = true;

        pico_display.set_led(64, 0, 0);

        // Render lower half of screen on main thread
        RenderMandelbrotSet(iRenderWidth, iRenderHeight / 2, iRenderHeight, fpMinCX, fpMaxCX, (fpMaxCY + fpMinCY) / 2, fpMaxCY, iIterations, iPixelChunkSize, pPalette);

        pico_display.set_led(0, 0, 64);

        // Make sure second thread has finished
        WaitFor(g_threadParams.bCore1Done);

        // Display image
        pico_display.update();
        pico_display.set_led(0, 0, 0);

        SetLowDetail(iIterations, iPixelChunkSize);

        bool bDoRender = false;

        // Loop, waiting for botton presses
        while (!bDoRender)
        {
            sleep_ms(1);

            fixed fpOffsetScale = FP_CONST(0.1);
            fixed fpZoomScale = FP_CONST(0.2);

            bool bNeedsRender = false;

            if (pico_display.is_pressed(pico_display.A) && pico_display.is_pressed(pico_display.B))
            {
                // Press A & B to regenerate palette
                if (!bPaletteGenerationLatch)
                {
                    CreateRandomPalette(pPalette, iPaletteSize);
                    bNeedsRender = true;
                    bPaletteGenerationLatch = true;
                }
            }
            else
            {
                bPaletteGenerationLatch = false;
            }

            if (pico_display.is_pressed(pico_display.A))
            {
                // Hold A to move left/right
                if (pico_display.is_pressed(pico_display.X))
                {
                    fpCentreX -= FP_MUL(fpWidth, fpOffsetScale);
                    bNeedsRender = true;
                }
                else if (pico_display.is_pressed(pico_display.Y))
                {
                    fpCentreX += FP_MUL(fpWidth, fpOffsetScale);
                    bNeedsRender = true;
                }
            }
            else if (pico_display.is_pressed(pico_display.B))
            {
                // Hold B to move up/down
                if (pico_display.is_pressed(pico_display.X))
                {
                    fpCentreY -= FP_MUL(fpWidth, fpOffsetScale);
                    bNeedsRender = true;
                }
                else if (pico_display.is_pressed(pico_display.Y))
                {
                    fpCentreY += FP_MUL(fpWidth, fpOffsetScale);
                    bNeedsRender = true;
                }
            }
            else
            {
                // Otherwise zoom in/out
                if (pico_display.is_pressed(pico_display.X))
                {
                    fpWidth = FP_MUL(fpWidth, FP_ONE - fpZoomScale);
                    bNeedsRender = true;
                }
                else if (pico_display.is_pressed(pico_display.Y))
                {
                    fpWidth = FP_MUL(fpWidth, FP_ONE + fpZoomScale);
                    bNeedsRender = true;
                }
            }

            if (bNeedsRender)
            {
                // Re-render if we've moved
                bDoRender = true;
                iRefineCounter = 0;
            }
            else
            {
                // After a set time, re-render at higher details
                iRefineCounter++;
                if (iRefineCounter == iRefineTime)
                {
                    bDoRender = true;
                    SetHighDetail(iIterations, iPixelChunkSize);
                }
            }
        }
    }

    return 0;
}

void RenderMandelbrotSet(int iRenderWidth, int iScreenY, int iRenderHeight, fixed fpMinCX, fixed fpMaxCX, fixed fpMinCY, fixed fpMaxCY, int iIterations, int iPixelChunkSize, uint16_t *pPalette)
{
    const fixed fpPixelWidth = FP_DIV(fpMaxCX - fpMinCX, FP_FROM_INT(iRenderWidth));
    const fixed fpPixelHeight = FP_DIV(fpMaxCY - fpMinCY, FP_FROM_INT(iRenderHeight - iScreenY));

    const fixed fpEscapeRadius = FP_FROM_INT(2);
    const fixed fpER2 = FP_MUL(fpEscapeRadius, fpEscapeRadius);

    const int iStartY = iScreenY;

    uint16_t *pFramebuffer = pico_display.frame_buffer + iRenderWidth * iScreenY;

    for (; iScreenY < iRenderHeight; iScreenY++)
    {
        fixed fpCy = fpMinCY + FP_MUL(FP_FROM_INT(iScreenY - iStartY), fpPixelHeight);

        fixed fpAbsCy = fpCy < 0 ? -fpCy : fpCy;
        if (fpAbsCy < fpPixelHeight / 2)
        {
            fpCy = 0.0;
        }

        for (int iScreenX = 0; iScreenX < iRenderWidth; iScreenX += iPixelChunkSize)
        {
            fixed fpCx = fpMinCX + iScreenX * fpPixelWidth;

            fixed fpZx = 0;
            fixed fpZy = 0;
            fixed fpZx2 = 0;
            fixed fpZy2 = 0;

            int iIterationCount;
            for (iIterationCount = 0; iIterationCount < iIterations && ((fpZx2 + fpZy2) < fpER2); iIterationCount++)
            {
                fpZy = 2 * FP_MUL(fpZx, fpZy) + fpCy;
                fpZx = fpZx2 - fpZy2 + fpCx;
                fpZx2 = FP_MUL(fpZx, fpZx);
                fpZy2 = FP_MUL(fpZy, fpZy);
            };

            Pen pen;

            if (iIterationCount == iIterations)
            {
                pen = 0;
            }
            else
            {
                // Assumes palette size is > max iterations
                pen = pPalette[iIterationCount];
            };

            for (int iOffsetX = 0; iOffsetX < iPixelChunkSize; iOffsetX++)
            {
                *(pFramebuffer + iScreenX + iOffsetX) = pen;
            }
        }

        pFramebuffer += iRenderWidth;
    }
}

void CalculateBounds(fixed fpScreenRatio, fixed fpCentreX, fixed fpCentreY, fixed fpWidth, fixed &fpMinCX_out, fixed &fpMaxCX_out, fixed &fpMinCY_out, fixed &fpMaxCY_out)
{
    fixed fpHalfWidth = FP_DIV(fpWidth, FP_FROM_INT(2));
    fixed fpHalfHeight = FP_MUL(fpHalfWidth, fpScreenRatio);
    fpMinCX_out = fpCentreX - fpHalfWidth;
    fpMaxCX_out = fpCentreX + fpHalfWidth;
    fpMinCY_out = fpCentreY - fpHalfHeight;
    fpMaxCY_out = fpCentreY + fpHalfHeight;
}

void CreateRandomPalette(uint16_t *pPalette, int iPaletteSize)
{
    // TODO: Create better palettes
    int iRShift = rand() % 3;
    int iGShift = rand() % 3;
    int iBShift = rand() % 3;

    for (int i = 0; i < iPaletteSize; i++)
    {
        pPalette[i] = pico_display.create_pen(i << iRShift, i << iGShift, i << iBShift);
    }
}
