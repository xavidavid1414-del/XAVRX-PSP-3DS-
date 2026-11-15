
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
static char cpuTestInfo[500] = "Prueba pendiente.";

static int gameCount = 0;
static int selected = 0;
static int page = 0;

static const char *options[] = {
    "BIBLIOTECA PSP",
    "DIAGNOSTICO",
    "CONFIGURACION",
    "ACERCA DE XAVRX"
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
    if (len < 4) return 0;

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

    if (!dir) return;

    struct dirent *entry;

    while ((entry = readdir(dir)) != NULL &&
           gameCount < MAX_GAMES)
    {
        if (entry->d_name[0] == '.') continue;

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
        unsigned int offset = pos % SECTOR_SIZE;

        if (fseek(f, (long)extent * SECTOR_SIZE + pos,
                  SEEK_SET) != 0)
            return 0;

        int length = fgetc(f);

        if (length <= 0)
        {
            pos += SECTOR_SIZE - offset;
            continue;
        }

        if (length < 34 || length > 255 ||
            pos + (unsigned int)length > size)
        {
            pos += SECTOR_SIZE - offset;
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
            if (c == EOF || c == 0) break;

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

    if (!pspMemoryInit(&memory))
    {
        snprintf(cpuTestInfo, sizeof(cpuTestInfo),
                 "ERROR: No se pudo iniciar memoria.");
        return;
    }

    bool passed = pspTestRunBasic(&memory, &result);

    snprintf(cpuTestInfo, sizeof(cpuTestInfo),
        "RESULTADO: %s\n\n"
        "Instrucciones: %lu\n"
        "PC final: 0x%08lX\n"
        "Registro T2: %lu\n"
        "Error: %d\n\n"
        "Esperado: 30",
        passed ? "CORRECTO" : "FALLO",
        (unsigned long)result.instructionsExecuted,
        (unsigned long)result.finalPC,
        (unsigned long)result.result,
        result.error);

    pspMemoryDestroy(&memory);
}

static void drawHeader(const char *subtitle)
{
    consoleSelect(&topScreen);
    consoleClear();

    printf("\x1b[36;1m");
    printf("================================\n");
    printf("          X A V R X\n");
    printf("================================\n");
    printf("\x1b[37m");
    printf("       PSP EMULATOR 3DS\n");
    printf("\x1b[36m");
    printf("       %s\n", subtitle);
    printf("\x1b[37m\n");
}

static void drawMenu(void)
{
    if (page == 0)
    {
        drawHeader("MAIN MENU");

        printf("  SELECT MODULE\n\n");

        for (int i = 0; i < 4; i++)
        {
            if (i == selected)
                printf("\x1b[32;1m > ");
            else
                printf("   ");

            printf("%s\n\n", options[i]);
            printf("\x1b[37m");
        }

        printf("\n--------------------------------\n");
        printf("SYSTEM: READY\n");
        printf("BUILD: XAVRX 0.12");
    }
    else if (page == 1)
    {
        drawHeader("GAME LIBRARY");

        printf("  ARCHIVOS: %d\n\n", gameCount);

        if (gameCount == 0)
        {
            printf("No hay juegos detectados.\n");
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

        printf("\nA: Analizar");
        printf("\nX: Actualizar");
    }
    else if (page == 2)
    {
        drawHeader("SYSTEM DIAGNOSTICS");

        printf("  CPU PSP: PRUEBA DISPONIBLE\n\n");
        printf("%s\n", cpuTestInfo);
        printf("\nA: Ejecutar prueba CPU");
    }
    else if (page == 3)
    {
        drawHeader("SETTINGS");

        printf("  CONFIGURACION\n\n");
        printf("  Biblioteca: sdmc:/PSP/\n");
        printf("  Formato ISO: Experimental\n");
        printf("  Formato CSO: Pendiente\n");
        printf("  GPU PSP: Pendiente\n");
        printf("  Audio PSP: Pendiente\n");
    }
    else if (page == 4)
    {
        drawHeader("ABOUT XAVRX");

        printf("  Proyecto: XAVRX PSP 3DS\n\n");
        printf("  Version: 0.12\n");
        printf("  Plataforma: Nintendo 2DS XL\n");
        printf("  Motor: Experimental\n\n");
        printf("  Proyecto de emulacion PSP.\n");
    }
    else if (page == 5)
    {
        drawHeader("GAME INFORMATION");
        printf("%s\n", isoInfo);
    }

    consoleSelect(&bottomScreen);
    consoleClear();

    printf("\x1b[36;1m");
    printf("       XAVRX CONTROLS\n\n");
    printf("\x1b[37m");
    printf("  UP / DOWN  Navigate\n");
    printf("  A          Select\n");
    printf("  B          Back\n");
    printf("  X          Refresh library\n");
    printf("  START      Exit\n\n");
    printf("--------------------------------\n");
    printf("  XAVRX PSP 3DS v0.12\n");
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
                page = selected + 1;
                selected = 0;

                if (page == 1)
                    scanGames();

                if (page == 2)
                    snprintf(cpuTestInfo, sizeof(cpuTestInfo),
                             "Pulsa A para ejecutar la prueba.");

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
                             "CSO detectado. Descompresion pendiente.");

                page = 5;
                drawMenu();
            }

            if (keys & KEY_X)
            {
                scanGames();
                selected = 0;
                drawMenu();
            }
        }
        else if (page == 2)
        {
            if (keys & KEY_A)
            {
                runCpuTest();
                drawMenu();
            }
        }

        if (keys & KEY_B)
        {
            if (page != 0)
            {
                page = 0;
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
