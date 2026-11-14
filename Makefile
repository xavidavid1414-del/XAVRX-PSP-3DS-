#---------------------------------------------------------------------------------
SUFIJOS:
#---------------------------------------------------------------------------------

ifeq ($(strip $(DEVKITARM)),)
$(error "Por favor, configure DEVKITARM en su entorno. export DEVKITARM=<ruta a>devkitARM")
fin si

TOPDIR ?= $(CURDIR)
incluir $(DEVKITARM)/3ds_rules

#---------------------------------------------------------------------------------
# TARGET es el nombre de la salida
# BUILD es el directorio donde se colocarán los archivos objeto y los archivos intermedios.
# SOURCES es una lista de directorios que contienen código fuente
# DATA es una lista de directorios que contienen archivos de datos.
# INCLUDES es una lista de directorios que contienen archivos de cabecera
# GRAPHICS es una lista de directorios que contienen archivos gráficos.
# GFXBUILD es el directorio donde se colocarán los archivos gráficos convertidos.
# Si se establece en $(BUILD), se vinculará estáticamente en el archivo convertido.
# archivos como si fueran archivos de datos.
#
# NO_SMDH: si se establece en cualquier valor, no se genera ningún archivo SMDH.
# ROMFS es el directorio que contiene el RomFS, relativo al Makefile (Opcional)
# APP_TITLE es el nombre de la aplicación almacenada en el archivo SMDH (Opcional)
# APP_DESCRIPTION es la descripción de la aplicación almacenada en el archivo SMDH (Opcional)
# APP_AUTHOR es el autor de la aplicación almacenada en el archivo SMDH (Opcional)
# ICON es el nombre del archivo del icono (.png), relativo a la carpeta del proyecto.
# Si no se especifica, intenta utilizar uno de los siguientes (en este orden):
# - <Nombre del proyecto>.png
# - icono.png
# - <carpeta libctru>/default_icon.png
#---------------------------------------------------------------------------------
OBJETIVO := $(notdir $(CURDIR))
COMPAÑAR := construir
FUENTES := fuente
DATOS := datos
INCLUYE := incluir
GRÁFICOS := gfx
GFXBUILD := $(BUILD)
#ROMFS := romfs
#GFXBUILD := $(ROMFS)/gfx

#---------------------------------------------------------------------------------
# opciones para la generación de código
#---------------------------------------------------------------------------------
ARCH := -march=armv6k -mtune=mpcore -mfloat-abi=hard -mtp=soft

CFLAGS := -g -Wall -O2 -mword-relocations \
			-secciones-de-función \
			$(ARCH)

CFLAGS += $(INCLUIR) -D__3DS__

CXXFLAGS := $(CFLAGS) -fno-rtti -fno-exceptions -std=gnu++11

ASFLAGS := -g $(ARCH)
LDFLAGS = -specs=3dsx.specs -g $(ARCH) -Wl,-Map,$(notdir $*.map)

LIBS := -lctru -lm

#---------------------------------------------------------------------------------
# lista de directorios que contienen bibliotecas, este debe ser el nivel superior que contiene
# incluir y biblioteca
#---------------------------------------------------------------------------------
LIBDIRS := $(CTRULIB)


#---------------------------------------------------------------------------------
# No hay necesidad real de editar nada más allá de este punto a menos que necesite agregar algo adicional
# reglas para diferentes extensiones de archivo
#---------------------------------------------------------------------------------
ifneq ($(BUILD),$(notdir $(CURDIR)))
#---------------------------------------------------------------------------------

export SALIDA := $(CURDIR)/$(TARGET)
exportar TOPDIR := $(CURDIR)

export VPATH := $(foreach dir,$(SOURCES),$(CURDIR)/$(dir)) \
			$(foreach dir,$(GRAPHICS),$(CURDIR)/$(dir)) \
			$(para cada directorio,$(DATOS),$(CURDIR)/$(directorio))

exportar DEPSDIR := $(CURDIR)/$(BUILD)

CFILES := $(foreach dir,$(SOURCES),$(notdir $(wildcard $(dir)/*.c)))
CPPFILES := $(foreach dir,$(SOURCES),$(notdir $(wildcard $(dir)/*.cpp)))
ARCHIVOS := $(para cada directorio,$(FUENTES),$(nodirectorio $(comodín $(directorio)/*.s)))
PICAFILES := $(foreach dir,$(SOURCES),$(notdir $(wildcard $(dir)/*.v.pica)))
SHLISTFILES := $(foreach dir,$(SOURCES),$(notdir $(wildcard $(dir)/*.shlist)))
GFXFILES := $(foreach dir,$(GRAPHICS),$(notdir $(wildcard $(dir)/*.t3s)))
BINFILES := $(foreach dir,$(DATA),$(notdir $(wildcard $(dir)/*.*)))

#---------------------------------------------------------------------------------
# use CXX para enlazar proyectos C++, CC para C estándar
#---------------------------------------------------------------------------------
ifeq ($(strip $(CPPFILES)),)
#---------------------------------------------------------------------------------
	exportar LD := $(CC)
#---------------------------------------------------------------------------------
demás
#---------------------------------------------------------------------------------
	exportar LD := $(CXX)
#---------------------------------------------------------------------------------
fin si
#---------------------------------------------------------------------------------

#---------------------------------------------------------------------------------
ifeq ($(GFXBUILD),$(BUILD))
#---------------------------------------------------------------------------------
export T3XFILES := $(GFXFILES:.t3s=.t3x)
#---------------------------------------------------------------------------------
demás
#---------------------------------------------------------------------------------
export ROMFS_T3XFILES := $(patsubst %.t3s, $(GFXBUILD)/%.t3x, $(GFXFILES))
export T3XHFILES := $(patsubst %.t3s, $(BUILD)/%.h, $(GFXFILES))
#---------------------------------------------------------------------------------
fin si
#---------------------------------------------------------------------------------

export OFILES_SOURCES := $(CPPFILES:.cpp=.o) $(CFILES:.c=.o) $(SFILES:.s=.o)

export OFILES_BIN := $(addsuffix .o,$(BINFILES)) \
			$(PICAFILES:.v.pica=.shbin.o) $(SHLISTFILES:.shlist=.shbin.o) \
			$(addsuffix .o,$(T3XFILES))

exportar OFILES := $(OFILES_BIN) $(OFILES_SOURCES)

export HFILES := $(PICAFILES:.v.pica=_shbin.h) $(SHLISTFILES:.shlist=_shbin.h) \
			$(addsuffix .h,$(subst .,_,$(BINFILES))) \
			$(GFXFILES:.t3s=.h)

export INCLUDE := $(foreach dir,$(INCLUDES),-I$(CURDIR)/$(dir)) \
			$(foreach dir,$(LIBDIRS),-I$(dir)/include) \
			-I$(CURDIR)/$(BUILD)

export LIBPATHS := $(foreach dir,$(LIBDIRS),-L$(dir)/lib)

export _3DSXDEPS := $(if $(NO_SMDH),,$(OUTPUT).smdh)

ifeq ($(strip $(ICON)),)
	iconos := $(wildcard *.png)
	ifneq (,$(findstring $(TARGET).png,$(icons)))
		export APP_ICON := $(TOPDIR)/$(TARGET).png
	demás
		ifneq (,$(findstring icon.png,$(icons)))
			export APP_ICON := $(TOPDIR)/icon.png
		fin si
	fin si
demás
	export APP_ICON := $(TOPDIR)/$(ICON)
fin si

ifeq ($(strip $(NO_SMDH)),)
	export _3DSXFLAGS += --smdh=$(CURDIR)/$(TARGET).smdh
fin si

ifneq ($(ROMFS),)
	export _3DSXFLAGS += --romfs=$(CURDIR)/$(ROMFS)
fin si

.FALSO: todo limpio

#---------------------------------------------------------------------------------
todos: $(BUILD) $(GFXBUILD) $(DEPSDIR) $(ROMFS_T3XFILES) $(T3XHFILES)
	@$(MAKE) --no-print-directory -C $(BUILD) -f $(CURDIR)/Makefile

$(CONSTRUIR):
	@mkdir -p $@

ifneq ($(GFXBUILD),$(BUILD))
$(GFXBUILD):
	@mkdir -p $@
fin si

ifneq ($(DEPSDIR),$(BUILD))
$(DEPSDIR):
	@mkdir -p $@
fin si

#---------------------------------------------------------------------------------
limpio:
	@echo limpio ...
	@rm -fr $(BUILD) $(TARGET).3dsx $(OUTPUT).smdh $(TARGET).elf $(GFXBUILD)

#---------------------------------------------------------------------------------
$(GFXBUILD)/%.t3x $(BUILD)/%.h : %.t3s
#---------------------------------------------------------------------------------
	@echo $(notdir $<)
	@tex3ds -i $< -H $(BUILD)/$*.h -d $(DEPSDIR)/$*.d -o $(GFXBUILD)/$*.t3x

#---------------------------------------------------------------------------------
demás

#---------------------------------------------------------------------------------
# objetivos principales
#---------------------------------------------------------------------------------
$(SALIDA).3dsx : $(SALIDA).elf $(_3DSXDEPS)

$(OFILES_SOURCES) : $(HFILES)

$(OUTPUT).elf : $(OFILES)

#---------------------------------------------------------------------------------
# Necesitas una regla como esta para cada extensión que uses como datos binarios.
#---------------------------------------------------------------------------------
%.bin.o %_bin.h : %.bin
#---------------------------------------------------------------------------------
	@echo $(notdir $<)
	@$(bin2o)

#---------------------------------------------------------------------------------
.PRECIOSO: %.t3x %.shbin
#---------------------------------------------------------------------------------
%.t3x.o %_t3x.h : %.t3x
#---------------------------------------------------------------------------------
	$(SILENTMSG) $(notdir $<)
	$(bin2o)

#---------------------------------------------------------------------------------
%.shbin.o %_shbin.h : %.shbin
#---------------------------------------------------------------------------------
	$(SILENTMSG) $(notdir $<)
	$(bin2o)

-incluir $(DEPSDIR)/*.d

#---------------------------------------------------------------------------------------
fin si
#---------------------------------------------------------------------------------------
