
#include <3ds.h>
#include <stdio.h>
#include <dirent.h>
#include <string.h>
#include <ctype.h>

#define MAX_GAMES 20
#define NAME_SIZE 128

static PrintConsole topScreen, bottomScreen;
static char games[MAX_GAMES][NAME_SIZE];
static char isoInfo[512];
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
    if (len < 4) return 0;

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
    if (!dir) return;

    struct dirent *entry;

    while ((entry = readdir(dir)) != NULL &&
           gameCount < MAX_GAMES)
    {
        if (entry->d_name[0] == '.') continue;

        if (isPSPFile(entry->d_name))
        {
            snprintf(games[gameCount], NAME_SIZE,
                     "%s", entry->d_name);
            gameCount++;
        }
    }

    closedir(dir);
}

static unsigned int readLE32(const unsigned char *p)
{
    return (unsigned int)p[0] |
           ((unsigned int)p[1] << 8) |
           ((unsigned int)p[2] << 16) |
           ((unsigned int)p[3] << 24);
}

static void analyzeISO(const char *name)
{
    char path[256];
    unsigned char descriptor[2048];
    char volume[33];

    snprintf(path, sizeof(path), "sdmc:/PSP/%s", name);

    FILE *file = fopen(path, "rb");

    if (!file)
    {
        snprintf(isoInfo, sizeof(isoInfo),
                 "No se pudo abrir el archivo.");
        return;
    }

    if (fseek(file, 16L * 2048L, SEEK_SET) != 0 ||
        fread(descriptor, 1, sizeof(descriptor), file)
        != sizeof(descriptor))
    {
        snprintf(isoInfo, sizeof(isoInfo),
                 "Archivo demasiado pequeno o incompleto.");
        fclose(file);
        return;
    }

    if (descriptor[0] != 1 ||
        memcmp(descriptor + 1, "CD001", 5) != 0 ||
        descriptor[6] != 1)
    {
        snprintf(isoInfo, sizeof(isoInfo),
                 "No se reconoce un descriptor ISO 9660 valido.");
        fclose(file);
        return;
    }

    memcpy(volume, descriptor + 40, 32);
    volume[32] = '\0';

    for (int i = 31; i >= 0; i--)
    {
        if (volume[i] == ' ')
            volume[i] = '\0';
        else
            break;
    }

    unsigned int sectors = readLE32(descriptor + 80);

    fseek(file, 0, SEEK_END);
    long fileSize = ftell(file);
    fclose(file);

    snprintf(isoInfo, sizeof(isoInfo),
        "ISO 9660: VALIDO\n\n"
        "Volumen: %.32s\n"
        "Sectores: %u\n"
        "Tamano ISO: %llu MB\n"
        "Tamano archivo: %llu MB",
        volume,
        sectors,
        ((unsigned long long)sectors * 2048ULL) /
            (1024ULL * 1024ULL),
        fileSize >= 0 ?
            (unsigned long long)fileSize / (1024ULL * 1024ULL) : 0ULL);
}

static void drawMenu(void)
{
    consoleSelect(&topScreen);
    consoleClear();

    printf("       XAVRX PSP 3DS\n");
    printf("          VERSION 0.4\n\n");

    if (page == 0)
    {
        printf("       MENU PRINCIPAL\n\n");

        for (int i = 0; i < 3; i++)
            printf("%s %s\n\n",
                   i == selected ? ">" : " ",
                   options[i]);

        printf("\nSTART: Salir");
    }
    else if (page == 1)
    {
        printf("       BIBLIOTECA PSP\n\n");
        printf("Archivos: %d\n\n", gameCount);

        if (gameCount == 0)
        {
            printf("No hay archivos detectados.\n");
            printf("Carpeta: sdmc:/PSP/");
        }
        else
        {
            for (int i = 0; i < gameCount; i++)
                printf("%s %s\n",
                    i == selected ? ">" : " ",
                    games[i]);
        }

        printf("\nA: Analizar ISO");
        printf("\nX: Actualizar");
        printf("\nB: Volver");
    }
    else if (page == 2)
    {
        printf("       ESTADO DEL SISTEMA\n\n");
        printf("Consola: Nintendo 2DS XL\n");
        printf("Biblioteca: Activa\n");
        printf("Analizador ISO: Activo\n");
        printf("CSO: Pendiente\n");
        printf("CPU PSP: Pendiente\n");
        printf("GPU PSP: Pendiente\n");
        printf("Audio PSP: Pendiente\n");
        printf("\nB: Volver");
    }
    else if (page == 3)
    {
        printf("       ACERCA DE XAVRX\n\n");
        printf("Proyecto: XAVRX PSP 3DS\n");
        printf("Version: 0.4\n");
        printf("Plataforma: Nintendo 3DS\n");
        printf("Objetivo: Emulacion PSP\n");
        printf("\nProyecto experimental.\n");
        printf("\nB: Volver");
    }
    else if (page == 4)
    {
        printf("       ANALISIS ISO\n\n");
        printf("%s\n", isoInfo);
        printf("\nB: Volver");
    }

    consoleSelect(&bottomScreen);
    consoleClear();

    printf("CONTROLES\n\n");
    printf("Cruceta: Navegar\n");
    printf("A: Seleccionar / Analizar\n");
    printf("X: Actualizar biblioteca\n");
    printf("B: Volver\n");
    printf("START: Salir");
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
        else if (page == 1)
        {
            if (keys & KEY_UP)
            {
                if (gameCount > 0)
                    selected = (selected + gameCount - 1)
                               % gameCount;
                drawMenu();
            }

            if (keys & KEY_DOWN)
            {
                if (gameCount > 0)
                    selected = (selected + 1) % gameCount;
                drawMenu();
            }

            if ((keys & KEY_A) && gameCount > 0)
            {
                size_t len = strlen(games[selected]);

                if (len >= 4 &&
                    tolower((unsigned char)games[selected][len-3]) == 'i' &&
                    tolower((unsigned char)games[selected][len-2]) == 's' &&
                    tolower((unsigned char)games[selected][len-1]) == 'o')
                    analyzeISO(games[selected]);
                else
                    snprintf(isoInfo, sizeof(isoInfo),
                        "Formato CSO detectado.\n"
                        "Descompresor pendiente.");

                page = 4;
                drawMenu();
            }

            if (keys & KEY_X)
            {
                scanGames();
                selected = 0;
                drawMenu();
            }
        }
        else if (keys & KEY_B)
        {
            page = 0;
            selected = 0;
            drawMenu();
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
