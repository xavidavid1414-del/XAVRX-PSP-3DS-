
#include <3ds.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <dirent.h>
#include <ctype.h>

#include "psp_test.h"

#define MAX_GAMES 20
#define NAME_SIZE 128
#define SECTOR_SIZE 2048

static PrintConsole topScreen, bottomScreen;

static char games[MAX_GAMES][NAME_SIZE];
static char isoInfo[700];
static char cpuTestInfo[500] = "Prueba CPU pendiente.";

static int gameCount = 0;
static int selected = 0;
static int page = 0;

static const char *options[] = {
    "Biblioteca PSP",
    "Estado del sistema",
    "Acerca de XAVRX",
    "Prueba de CPU PSP"
};

static unsigned int readLE16(const unsigned char *p)
{
    return (unsigned int)p[0] |
           ((unsigned int)p[1] << 8);
}

static unsigned int readLE32(const unsigned char *p)
{
    return (unsigned int)p[0] |
           ((unsigned int)p[1] << 8) |
           ((unsigned int)p[2] << 16) |
           ((unsigned int)p[3] << 24);
}

static int isPSPFile(const char *name)
{
    size_t len = strlen(name);

    if (len < 4)
        return 0;

    const char *ext = name + len - 4;

    return
        (tolower((unsigned char)ext[0]) == '.' &&
         tolower((unsigned char)ext[1]) == 'i' &&
         tolower((unsigned char)ext[2]) == 's' &&
         tolower((unsigned char)ext[3]) == 'o') ||
        (tolower((unsigned char)ext[0]) == '.' &&
         tolower((unsigned char)ext[1]) == 'c' &&
         tolower((unsigned char)ext[2]) == 's' &&
         tolower((unsigned char)ext[3]) == 'o');
}

static int isISO(const char *name)
{
    size_t len = strlen(name);

    return len >= 4 &&
        tolower((unsigned char)name[len-3]) == 'i' &&
        tolower((unsigned char)name[len-2]) == 's' &&
        tolower((unsigned char)name[len-1]) == 'o';
}

static void scanGames(void)
{
    gameCount = 0;

    DIR *dir = opendir("sdmc:/PSP");

    if (!dir)
        return;

    struct dirent *entry;

    while ((entry = readdir(dir)) != NULL &&
           gameCount < MAX_GAMES)
    {
        if (entry->d_name[0] == '.')
            continue;

        if (isPSPFile(entry->d_name))
        {
            snprintf(games[gameCount],
                     NAME_SIZE, "%s", entry->d_name);
            gameCount++;
        }
    }

    closedir(dir);
}

static int sameName(const unsigned char *name,
                    int length, const char *wanted)
{
    int i = 0;

    while (i < length && name[i] != ';')
    {
        if (toupper((unsigned char)name[i]) !=
            toupper((unsigned char)wanted[i]))
            return 0;

        i++;
    }

    return wanted[i] == '\0';
}

static int findISOEntry(FILE *f,
                        unsigned int extent,
                        unsigned int size,
                        const char *wanted,
                        unsigned int *foundExtent,
                        unsigned int *foundSize)
{
    unsigned int pos = 0;
    unsigned char record[256];

    while (pos < size)
    {
        unsigned int sectorOffset = pos % SECTOR_SIZE;

        if (fseek(f, (long)extent * SECTOR_SIZE + pos,
                  SEEK_SET) != 0)
            return 0;

        int length = fgetc(f);

        if (length <= 0)
        {
            pos += SECTOR_SIZE - sectorOffset;
            continue;
        }

        if (length < 34 || length > 255 ||
            pos + (unsigned int)length > size)
        {
            pos += SECTOR_SIZE - sectorOffset;
            continue;
        }

        record[0] = (unsigned char)length;

        if (fread(record + 1, 1, length - 1, f)
            != (size_t)(length - 1))
            return 0;

        int nameLength = record[32];

        if (nameLength > 0 &&
            33 + nameLength <= length &&
            sameName(record + 33, nameLength, wanted))
        {
            *foundExtent = readLE32(record + 2);
            *foundSize = readLE32(record + 10);
            return 1;
        }

        pos += (unsigned int)length;
    }

    return 0;
}

static int readSFO(FILE *f,
                   unsigned int extent,
                   unsigned int size,
                   char *title,
                   char *gameID,
                   char *version)
{
    unsigned char header[20];

    if (size < 20 ||
        fseek(f, (long)extent * SECTOR_SIZE, SEEK_SET) != 0 ||
        fread(header, 1, 20, f) != 20)
        return 0;

    if (memcmp(header, "\0PSF", 4) != 0)
        return 0;

    unsigned int keyOffset = readLE32(header + 8);
    unsigned int dataOffset = readLE32(header + 12);
    unsigned int entries = readLE32(header + 16);

    if (entries > 128 ||
        keyOffset >= size ||
        dataOffset >= size)
        return 0;

    title[0] = '\0';
    gameID[0] = '\0';
    version[0] = '\0';

    for (unsigned int i = 0; i < entries; i++)
    {
        unsigned char entry[16];
        unsigned int entryPos = 20 + i * 16;

        if (entryPos + 16 > size ||
            fseek(f, (long)extent * SECTOR_SIZE + entryPos,
                  SEEK_SET) != 0 ||
            fread(entry, 1, 16, f) != 16)
            return 0;

        unsigned int key = readLE16(entry);
        unsigned int format = readLE16(entry + 2);
        unsigned int dataLength = readLE32(entry + 4);
        unsigned int dataPos = readLE32(entry + 12);

        if (format != 0x0204 ||
            dataLength == 0 ||
            dataLength > 127 ||
            dataPos > size - dataOffset ||
            dataLength > size - dataOffset - dataPos ||
            key >= size - keyOffset)
            continue;

        char keyName[64];
        unsigned int j = 0;

        while (j < 63 && key + j < size - keyOffset)
        {
            if (fseek(f, (long)extent * SECTOR_SIZE +
                      keyOffset + key + j, SEEK_SET) != 0)
                break;

            int c = fgetc(f);

            if (c == EOF || c == 0)
                break;

            keyName[j++] = (char)c;
        }

        keyName[j] = '\0';

        char value[128];

        if (fseek(f, (long)extent * SECTOR_SIZE +
                  dataOffset + dataPos, SEEK_SET) != 0)
            continue;

        if (fread(value, 1, dataLength, f) != dataLength)
            continue;

        value[dataLength] = '\0';

        if (strcmp(keyName, "TITLE") == 0)
            snprintf(title, 128, "%s", value);

        if (strcmp(keyName, "DISC_ID") == 0)
            snprintf(gameID, 32, "%s", value);

        if (strcmp(keyName, "DISC_VERSION") == 0)
            snprintf(version, 32, "%s", value);
    }

    return 1;
}

static void analyzeISO(const char *name)
{
    char path[256];
    unsigned char pvd[SECTOR_SIZE];

    unsigned int rootExtent, rootSize;
    unsigned int pspExtent, pspSize;
    unsigned int sfoExtent, sfoSize;

    char title[128] = "No disponible";
    char gameID[32] = "No disponible";
    char version[32] = "No disponible";

    snprintf(path, sizeof(path), "sdmc:/PSP/%s", name);

    FILE *f = fopen(path, "rb");

    if (!f)
    {
        snprintf(isoInfo, sizeof(isoInfo),
                 "No se pudo abrir el ISO.");
        return;
    }

    if (fseek(f, 16L * SECTOR_SIZE, SEEK_SET) != 0 ||
        fread(pvd, 1, SECTOR_SIZE, f) != SECTOR_SIZE ||
        pvd[0] != 1 ||
        memcmp(pvd + 1, "CD001", 5) != 0 ||
        pvd[6] != 1)
    {
        snprintf(isoInfo, sizeof(isoInfo),
                 "ISO 9660 no valido.");
        fclose(f);
        return;
    }

    rootExtent = readLE32(pvd + 158);
    rootSize = readLE32(pvd + 166);

    if (!findISOEntry(f, rootExtent, rootSize,
                      "PSP_GAME", &pspExtent, &pspSize))
    {
        snprintf(isoInfo, sizeof(isoInfo),
                 "No se encontro PSP_GAME.");
        fclose(f);
        return;
    }

    if (!findISOEntry(f, pspExtent, pspSize,
                      "PARAM.SFO", &sfoExtent, &sfoSize))
    {
        snprintf(isoInfo, sizeof(isoInfo),
                 "No se encontro PARAM.SFO.");
        fclose(f);
        return;
    }

    if (!readSFO(f, sfoExtent, sfoSize,
                 title, gameID, version))
    {
        snprintf(isoInfo, sizeof(isoInfo),
                 "Error al leer PARAM.SFO.");
        fclose(f);
        return;
    }

    snprintf(isoInfo, sizeof(isoInfo),
        "INFORMACION DEL JUEGO\n\n"
        "Archivo: %.90s\n"
        "Titulo: %.100s\n"
        "Game ID: %.20s\n"
        "Version: %.20s\n\n"
        "PARAM.SFO analizado.",
        name, title, gameID, version);

    fclose(f);
}

static void runCpuTest(void)
{
    PSPMemory memory;
    PSPTestResult result;

    snprintf(cpuTestInfo, sizeof(cpuTestInfo),
             "Iniciando prueba de CPU...");

    if (!pspMemoryInit(&memory))
    {
        snprintf(cpuTestInfo, sizeof(cpuTestInfo),
                 "ERROR: No se pudo inicializar la memoria PSP.");
        return;
    }

    bool passed = pspTestRunBasic(&memory, &result);

    snprintf(cpuTestInfo, sizeof(cpuTestInfo),
        "RESULTADO: %s\n\n"
        "Instrucciones: %lu\n"
        "PC final: 0x%08lX\n"
        "Registro t2: %lu\n"
        "Error: %d\n\n"
        "Resultado esperado: 30",
        passed ? "CORRECTO" : "FALLO",
        (unsigned long)result.instructionsExecuted,
        (unsigned long)result.finalPC,
        (unsigned long)result.result,
        result.error);

    pspMemoryDestroy(&memory);
}

static void drawMenu(void)
{
    consoleSelect(&topScreen);
    consoleClear();

    printf("       XAVRX PSP 3DS\n");
    printf("          VERSION 0.5\n\n");

    if (page == 0)
    {
        printf("       MENU PRINCIPAL\n\n");

        for (int i = 0; i < 4; i++)
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
        printf("Archivos: %d\n\n", gameCount);

        if (gameCount == 0)
        {
            printf("No hay archivos detectados.\n");
            printf("Carpeta: sdmc:/PSP/");
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

        printf("\nA: Analizar ISO");
        printf("\nX: Actualizar");
        printf("\nB: Volver");
    }
    else if (page == 2)
    {
        printf("       ESTADO DEL SISTEMA\n\n");
        printf("Consola: Nintendo 2DS XL\n");
        printf("Biblioteca: Activa\n");
        printf("ISO Analyzer: Activo\n");
        printf("PARAM.SFO: Experimental\n");
        printf("CSO: Pendiente\n");
        printf("CPU PSP: En pruebas\n");
        printf("GPU PSP: Pendiente\n");
        printf("Audio PSP: Pendiente\n");
        printf("\nB: Volver");
    }
    else if (page == 3)
    {
        printf("       ACERCA DE XAVRX\n\n");
        printf("Proyecto: XAVRX PSP 3DS\n");
        printf("Version: 0.5\n");
        printf("Plataforma: Nintendo 3DS\n");
        printf("Objetivo: Emulacion PSP\n");
        printf("\nProyecto experimental.\n");
        printf("\nB: Volver");
    }
    else if (page == 4)
    {
        printf("       GAME INFO\n\n");
        printf("%s\n", isoInfo);
        printf("\nB: Volver");
    }
    else if (page == 5)
    {
        printf("       PRUEBA CPU PSP\n\n");
        printf("%s\n", cpuTestInfo);
        printf("\nB: Volver");
    }

    consoleSelect(&bottomScreen);
    consoleClear();

    printf("CONTROLES\n\n");
    printf("Cruceta: Navegar\n");
    printf("A: Seleccionar\n");
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
                selected = (selected + 3) % 4;
                drawMenu();
            }

            if (keys & KEY_DOWN)
            {
                selected = (selected + 1) % 4;
                drawMenu();
            }

            if (keys & KEY_A)
            {
                if (selected == 0)
                    page = 1;
                else if (selected == 1)
                    page = 2;
                else if (selected == 2)
                    page = 3;
                else
                {
                    page = 5;
                    runCpuTest();
                }

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
                if (isISO(games[selected]))
                    analyzeISO(games[selected]);
                else
                    snprintf(isoInfo, sizeof(isoInfo),
                        "Archivo CSO detectado.\n"
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
