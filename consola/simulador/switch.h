// Simulador de NHSE para Switch en el Mac: lo justo de libnx para que la app se compile y funcione aquí, con la partida
// en una carpeta, los botones y el teclado sacados de un guion y fotos de la pantalla. Ver sim.cpp.
#pragma once
#include <cstddef>
#include <cstdint>

typedef uint8_t u8;
typedef uint16_t u16;
typedef uint32_t u32;
typedef uint64_t u64;
typedef int64_t s64;
typedef u32 Result;
#define R_FAILED(rc) ((rc) != 0)
#define R_SUCCEEDED(rc) ((rc) == 0)

// La pantalla: sin ventana y con el dibujo por software, y una foto en cada presentación pedida.
#define SDL_RenderPresent sim_present
#define SDL_CreateRenderer sim_renderer
#define SDL_CreateWindow sim_ventana

Result romfsInit();
Result romfsExit();
bool appletMainLoop();

enum PlServiceType { PlServiceType_User };
enum PlSharedFontType { PlSharedFontType_Standard };
struct PlFontData { void* address; u32 size; };
Result plInitialize(PlServiceType);
void plExit();
Result plGetSharedFontByType(PlFontData*, PlSharedFontType);

enum : u64 {
    HidNpadButton_A = 1ULL << 0, HidNpadButton_B = 1ULL << 1, HidNpadButton_X = 1ULL << 2, HidNpadButton_Y = 1ULL << 3,
    HidNpadButton_L = 1ULL << 6, HidNpadButton_R = 1ULL << 7, HidNpadButton_ZL = 1ULL << 8, HidNpadButton_ZR = 1ULL << 9,
    HidNpadButton_Plus = 1ULL << 10, HidNpadButton_Minus = 1ULL << 11,
    HidNpadButton_AnyLeft = 1ULL << 12, HidNpadButton_AnyUp = 1ULL << 13, HidNpadButton_AnyRight = 1ULL << 14, HidNpadButton_AnyDown = 1ULL << 15,
};
enum { HidNpadStyleSet_NpadStandard = 0 };
struct PadState { u64 down, held; };
void padConfigureInput(int, int);
void padInitializeDefault(PadState*);
void padUpdate(PadState*);
u64 padGetButtonsDown(const PadState*);
u64 padGetButtons(const PadState*);

struct HidTouchState { u32 x, y; };
struct HidTouchScreenState { int count; HidTouchState touches[16]; };
void hidInitializeTouchScreen();
int hidGetTouchScreenStates(HidTouchScreenState*, int);

enum SwkbdType { SwkbdType_Normal, SwkbdType_NumPad };
struct SwkbdConfig { char cabecera[256]; };
Result swkbdCreate(SwkbdConfig*, int);
void swkbdConfigMakePresetDefault(SwkbdConfig*);
void swkbdConfigSetType(SwkbdConfig*, SwkbdType);
void swkbdConfigSetStringLenMax(SwkbdConfig*, int);
void swkbdConfigSetHeaderText(SwkbdConfig*, const char*);
void swkbdConfigSetGuideText(SwkbdConfig*, const char*);
void swkbdConfigSetInitialText(SwkbdConfig*, const char*);
Result swkbdShow(SwkbdConfig*, char*, size_t);
void swkbdClose(SwkbdConfig*);

// La partida: una carpeta (NHSE_SIM_PARTIDA).
struct FsFileSystem { int abierto; };
struct FsFile { void* f; };
struct FsDir { void* d; };
enum FsSaveDataType { FsSaveDataType_Device };
enum FsSaveDataSpaceId { FsSaveDataSpaceId_User };
struct FsSaveDataAttribute { u64 application_id; FsSaveDataType save_data_type; };
enum { FsOpenMode_Read = 1, FsOpenMode_Write = 2 };
enum { FsReadOption_None = 0 };
enum { FsWriteOption_Flush = 1 };
enum { FsDirOpenMode_ReadDirs = 1, FsDirOpenMode_ReadFiles = 2 };
enum FsDirEntryType { FsDirEntryType_Dir = 0, FsDirEntryType_File = 1 };
struct FsDirectoryEntry { char name[0x301]; u8 type; s64 file_size; };
Result fsOpenSaveDataFileSystem(FsFileSystem*, FsSaveDataSpaceId, const FsSaveDataAttribute*);
Result fsFsOpenFile(FsFileSystem*, const char*, int, FsFile*);
Result fsFileGetSize(FsFile*, s64*);
Result fsFileRead(FsFile*, s64, void*, u64, int, u64*);
Result fsFileWrite(FsFile*, s64, const void*, u64, int);
void fsFileClose(FsFile*);
Result fsFsCommit(FsFileSystem*);
void fsFsClose(FsFileSystem*);
Result fsFsOpenDirectory(FsFileSystem*, const char*, int, FsDir*);
Result fsDirRead(FsDir*, s64*, size_t, FsDirectoryEntry*);
void fsDirClose(FsDir*);
