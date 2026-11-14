
#include <3ds.h>
#include <stdio.h>

int main(void)
{
    gfxInitDefault();
    consoleInit(GFX_TOP, NULL);

    printf("\x1b[2;8HXAVRX PSP 3DS");
    printf("\x1b[5;5HExperimental PSP Engine");
    printf("\x1b[8;5HInitializing...");
    printf("\x1b[11;5HNew Nintendo 2DS XL");
    printf("\x1b[15;5HDevelopment Version 0.1");

    printf("\x1b[20;5HPress START to exit");

    while (aptMainLoop())
    {
        hidScanInput();

        u32 kDown = hidKeysDown();

        if (kDown & KEY_START)
            break;

        gfxFlushBuffers();
        gfxSwapBuffers();
        gspWaitForVBlank();
    }

    gfxExit();
    return 0;
}
