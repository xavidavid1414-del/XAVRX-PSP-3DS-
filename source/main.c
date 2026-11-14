
#include <3ds.h>
#include <stdio.h>
#include <dirent.h>
#include <string.h>
#include <ctype.h>

#define MAX_GAMES 20
#define NAME_SIZE 128

static PrintConsole topScreen;
static PrintConsole bottomScreen;

static char games[MAX_GAMES][NAME_SIZE];
static int gameCount = 0;
static int selected = 0;
static int page = 0;

static const char *options[] = {
    "Biblioteca PSP",
    "Estado del sistema",
    "Acerca de XAVRX"
};

static int isPSPFile(const char *name)
{
    size_t len = strlen(name);

    if (len < 4)
        return 0;

    const char *ext = name + len - 4;

    return (tolower((unsigned char)ext[0]) == '.' &&
            tolower((unsigned char)ext[1]) == 'i' &&
            tolower((unsigned char)ext[2]) == 's' &&
            tolower((unsigned char)ext[3]) == 'o') ||
           (tolower((unsigned char)ext[0]) == '.' &&
            tolower((unsigned char)ext[1]) == 'c' &&
            tolower((unsigned char)ext[2]) == 's' &&
            tolower((unsigned char)ext[3]) == 'o');
}

static void scanGames(void)
{
    gameCount = 0;

    DIR *dir = opendir("sdmc:/PSP");

    if (!dir)
        return;

    struct dirent *entry;

    while ((entry = readdir(dir)) != NULL && gameCount < MAX_GAMES)
    {
        if (entry->d_name[0] == '.')
            continue;

        if (isPSPFile(entry->d_name))
        {
            snprintf(games[gameCount], NAME_SIZE,
                     "%s", entry->d_name);
            gameCount++;
        }
    }

    closedir(dir);
}

static void drawMenu(void)
{
    consoleSelect(&topScreen);
    consoleClear();

    printf("\x1b[2;8;36mXAVRX PSP 3DS\x1b[0m\n");
    printf("       VERSION 0.3\n\n");

    if (page == 0)
    {
        printf("       MENU PRINCIPAL\n\n");

        for (int i = 0; i < 3; i++)
        {
            printf("%s %s\n\n",
                i == selected ? ">" : " ",
                options[i]);
        }

        printf("\nSTART: Salir");
    }
    else if (page == 1)
    {
        printf("       BIBLIOTECA PSP\n\n");
        printf(" Archivos encontrados: %d\n\n", gameCount);

        if (gameCount == 0)
        {
            printf(" No hay juegos detectados.\n");
            printf(" Coloca ISO/CSO en:\n");
            printf(" sdmc:/PSP/\n");
        }
        else
        {
            for (int i = 0; i < gameCount; i++)
            {
                printf("%s %s\n",
                    i == selected ? ">" : " ",
                    games[i]);
            }
        }

        printf("\nX: Actualizar lista");
        printf("\nB: Volver");
    }
    else if (page == 2)
    {
        printf("       ESTADO DEL SISTEMA\n\n");
        printf(" Consola: Nintendo 2DS XL\n");
        printf(" Interfaz: Activa\n");
        printf(" Lector ISO/CSO: Activo\n");
        printf(" CPU PSP: Pendiente\n");
        printf(" GPU PSP: Pendiente\n");
        printf(" Audio PSP: Pendiente\n");
        printf("\nB: Volver");
    }
    else
    {
        printf("       ACERCA DE XAVRX\n\n");
        printf(" Proyecto: XAVRX PSP 3DS\n");
        printf(" Version: 0.3\n");
        printf(" Objetivo: Emulacion PSP\n");
        printf(" Plataforma: Nintendo 3DS\n");
        printf("\nProyecto experimental.\n");
        printf("\nB: Volver");
    }

    consoleSelect(&bottomScreen);
    consoleClear();

    printf("CONTROLES\n\n");
    printf("D-PAD: Navegar\n");
    printf("A: Seleccionar\n");
    printf("X: Actualizar biblioteca\n");
    printf("B: Volver\n");
    printf("START: Salir\n");
}

int main(void)
{
    gfxInitDefault();

    consoleInit(GFX_TOP, &topScreen);
    consoleInit(GFX_BOTTOM, &bottomScreen);

    scanGames();
    drawMenu();

    while (aptMainLoop())
    {
        hidScanInput();
        u32 keys = hidKeysDown();

        if (page == 0)
        {
            if (keys & KEY_UP)
            {
                selected = (selected + 2) % 3;
                drawMenu();
            }

            if (keys & KEY_DOWN)
            {
                selected = (selected + 1) % 3;
                drawMenu();
            }

            if (keys & KEY_A)
            {
                page = selected + 1;
                selected = 0;

                if (page == 1)
                    scanGames();

                drawMenu();
            }
        }
        else
        {
            if (keys & KEY_B)
            {
                page = 0;
                selected = 0;
                drawMenu();
            }

            if (page == 1 && (keys & KEY_X))
            {
                scanGames();
                selected = 0;
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
