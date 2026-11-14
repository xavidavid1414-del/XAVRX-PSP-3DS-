
#include <3ds.h>
#include <stdio.h>

static PrintConsole topScreen;
static PrintConsole bottomScreen;

static int selected = 0;
static int page = 0;

static const char *options[] = {
    "Biblioteca PSP",
    "Estado del sistema",
    "Acerca de XAVRX"
};

static void drawMenu(void)
{
    consoleSelect(&topScreen);
    consoleClear();

    printf("\x1b[2;8;36mXAVRX PSP 3DS\x1b[0m\n");
    printf("\x1b[4;8mExperimental PSP Project 0.2\n\n");

    if (page == 0)
    {
        printf("       MENU PRINCIPAL\n\n");

        for (int i = 0; i < 3; i++)
        {
            if (i == selected)
                printf("     > %s <\n\n", options[i]);
            else
                printf("       %s\n\n", options[i]);
        }

        printf("\n\n       START: Salir");
    }
    else if (page == 1)
    {
        printf("       BIBLIOTECA PSP\n\n");
        printf("  Formatos previstos:\n");
        printf("  - ISO\n");
        printf("  - CSO\n\n");
        printf("  Lector de imagenes:\n");
        printf("  En desarrollo\n\n");
        printf("  Todavia no hay juegos cargados.\n");
        printf("\n  B: Volver");
    }
    else if (page == 2)
    {
        printf("       ESTADO DEL SISTEMA\n\n");
        printf("  Consola: Nintendo 2DS XL\n");
        printf("  Plataforma: Nintendo 3DS\n");
        printf("  Interfaz: Activa\n");
        printf("  CPU PSP: No implementada\n");
        printf("  GPU PSP: No implementada\n");
        printf("  Audio PSP: No implementado\n");
        printf("\n  B: Volver");
    }
    else if (page == 3)
    {
        printf("       ACERCA DE XAVRX\n\n");
        printf("  Proyecto: XAVRX PSP 3DS\n");
        printf("  Version: 0.2\n");
        printf("  Arquitectura: ARM\n");
        printf("  Objetivo: Emulacion PSP\n\n");
        printf("  Proyecto experimental.\n");
        printf("\n  B: Volver");
    }

    consoleSelect(&bottomScreen);
    consoleClear();

    printf("CONTROLES\n\n");
    printf("D-PAD: Navegar\n");
    printf("A: Seleccionar\n");
    printf("B: Volver\n");
    printf("START: Salir\n");
}

int main(void)
{
    gfxInitDefault();

    consoleInit(GFX_TOP, &topScreen);
    consoleInit(GFX_BOTTOM, &bottomScreen);

    drawMenu();

    while (aptMainLoop())
    {
        hidScanInput();

        u32 keys = hidKeysDown();

        if (page == 0)
        {
            if (keys & KEY_UP)
            {
                selected--;
                if (selected < 0)
                    selected = 2;
                drawMenu();
            }

            if (keys & KEY_DOWN)
            {
                selected++;
                if (selected > 2)
                    selected = 0;
                drawMenu();
            }

            if (keys & KEY_A)
            {
                page = selected + 1;
                drawMenu();
            }
        }
        else
        {
            if (keys & KEY_B)
            {
                page = 0;
                drawMenu();
            }
        }

        if (keys & KEY_START)
            break;

        gfxFlushBuffers();
        gfxSwapBuffers();
        gspWaitForVBlank();
    }

    gfxExit();
    return 0;
}
