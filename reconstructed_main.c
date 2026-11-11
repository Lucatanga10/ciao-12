/*
 * ============================================================================
 *  RECONSTRUCTED SOURCE CODE - sploit.sprx (fn_console_unlocker)
 * ============================================================================
 *
 *  RICOSTRUITO DA: Analisi binaria di f490d314-sploit.sprx (532.6 KB)
 *  FORMATO: PS4 GoldHEN Plugin (FSELF / SPRX), x86-64 FreeBSD
 *  AUTORE ORIGINALE: "killi" (GoldHEN_Plugins_Repository)
 *  MODIFICATO DA: "itssploited" (Discord: .itssploited)
 *  VERSIONE: "Sploited FN v32"
 *
 *  PERCORSO ORIGINALE: /home/killi/GoldHEN_Plugins_Repository/plugin_src/
 *                       fn_console_unlocker/source/main.c
 *  COMMON:              ../../common/plugin_common.c
 *
 *  TARGET: Fortnite PS4 - CUSA07669 / CUSA07022
 *
 *  NOTA: Questo NON e' il codice sorgente originale esatto. E' una
 *  ricostruzione basata sulla disassembly completa del binario compilato.
 *  I nomi delle variabili locali e alcuni dettagli di flusso sono dedotti
 *  dal contesto. La struttura, le stringhe e la logica sono fedeli al binario.
 *
 *  176 funzioni identificate, 2070 stringhe estratte, 41 import PLT risolti.
 * ============================================================================
 */

/* ========== INCLUDES ========== */
#include <stdint.h>
#include <stddef.h>
#include <stdbool.h>
#include <stdarg.h>

/* PS4 / FreeBSD kernel */
#include <orbis/libkernel.h>
#include <orbis/UserService.h>

/* GoldHEN plugin SDK */
#include <GoldHEN/Common.h>
#include <GoldHEN/Plugin.h>

/* ========== GOLDHEN PLUGIN INFO ========== */
/* vaddr 0x0367fb-0x036902: plugin metadata in .rodata */
GOLDHEN_PLUGIN_INFO("fn_console_unlocker", "Sploited FN v32",
    "Console unlocker + fn.* commands",
    GOLDHEN_PLUGIN_VER(1, 0, 0));

/* ========== FORWARD DECLARATIONS ========== */
static void cmd_handler(const char *cmd);                    /* 0x001fc0 */
static void cmd_dispatch_simple(const char *cmd);            /* 0x000ee0 */
static void cmd_dispatch_arg1(const char *cmd, const char *arg); /* 0x001780 */
static void cmd_dispatch_arg2(const char *cmd, const char *arg); /* 0x001b20 */

/* ========== PLT IMPORT TABLE (41 entries) ========== */
/*
 * PLT 0x15940 = goldhen_log(fmt, file, line, ...)   - GoldHEN debug/assert
 * PLT 0x15950 = strcmp(s1, s2)
 * PLT 0x15960 = sceKernelUsleep(usec)
 * PLT 0x15970 = snprintf(buf, n, fmt, ...)
 * PLT 0x15980 = strlen(s)
 * PLT 0x15990 = memcpy(dst, src, n)
 * PLT 0x1599a = memset(dst, c, n)
 * PLT 0x159b0 = memcpy  (FVector variant)
 * PLT 0x159c0 = memset  (FVector variant)
 * PLT 0x159e0 = sceKernelOpen(path, flags, mode)
 * PLT 0x159f0 = sceKernelClose(fd)
 * PLT 0x15a00 = strncmp(s1, s2, n)
 * PLT 0x15a10 = strncat(dst, src, n)
 * PLT 0x15a20 = sceKernelMmap(addr, len, prot, flags, fd, off)
 * PLT 0x15a30 = sceKernelStat(path, sb)
 * PLT 0x15a50 = strtod(s, endp)
 * PLT 0x15a60 = strstr(haystack, needle)
 * PLT 0x15a70 = sceKernelWrite(fd, buf, nbytes)
 * PLT 0x15a80 = strtol(s, endp, base)
 * PLT 0x15a90 = wcslen / wcscpy
 * PLT 0x15aa0 = sceKernelRead(fd, buf, nbytes)
 * PLT 0x15ab0 = strcasecmp / tolower compare
 * PLT 0x15ac0 = sceKernelDprintf(fd, fmt, ...)  / fdprintf
 * PLT 0x15ad0 = sceKernelGetModuleInfo(pid, handle, info)
 * PLT 0x15ae0 = sceKernelGetModuleList(pid, handles, num)
 * PLT 0x15af0 = sceKernelUnlink(path)            / remove
 * PLT 0x15b00 = sceKernelRename(from, to)        / rename
 * PLT 0x15b10 = sceKernelLoadStartModule(path, argc, argv, flags, opt, res)
 * PLT 0x15b20 = scePthreadCreate(thread, attr, fn, arg)
 * PLT 0x15b30 = sceKernelMprotect(addr, len, prot)
 * PLT 0x15b40 = sceKernelJitCreateSharedMemory(...)
 * PLT 0x15b50 = sceKernelJitMapSharedMemory(...)
 * PLT 0x15b60 = sceKernelGetCpuFrequency()
 * PLT 0x15b70 = sceKernelSendNotificationRequest(...)
 * PLT 0x15b80 = sceKernelGetProcParam()
 * PLT 0x15b90 = sceKernelMmap (alt)
 * PLT 0x15ba0 = sceKernelMunmap(addr, len)
 * PLT 0x15bb0 = sceKernelGetCurrentCpu()
 * PLT 0x15bc0 = sceKernelGetDirectMemorySize()
 */

/* ========== TYPE DEFINITIONS ========== */

/* Unreal Engine 4 basic types (PS4 / x86-64) */
typedef uint16_t FNameEntryId;

typedef struct { float X, Y, Z; }      FVector;
typedef struct { float X, Y, Z, W; }   FQuat;
typedef struct { float Pitch, Yaw, Roll; } FRotator;
typedef struct { float R, G, B, A; }   FLinearColor;
typedef struct { uint8_t R, G, B, A; } FColor;

typedef struct {
    int32_t ComparisonIndex;
    int32_t Number;
} FName;

typedef struct {
    void **VTable;
    /* ... UObject internals ... */
} UObject;

typedef struct {
    UObject Base;
    /* ... AActor fields ... */
    FVector  Location;
    FRotator Rotation;
} AActor;

typedef struct {
    AActor Base;
    /* ... APawn / ACharacter fields ... */
} AFortPawn;

typedef struct {
    void *Controller;
    AFortPawn *Pawn;
    /* ... player state fields ... */
} AFortPlayerState;

/* Menu system */
typedef enum {
    MENU_TAB_AIMBOT = 0,
    MENU_TAB_AIM_CIRCLE,
    MENU_TAB_PLAYER_ESP,
    MENU_TAB_CONTAINER_ESP,
    MENU_TAB_LOOT_ESP,
    MENU_TAB_CROSSHAIR,
    MENU_TAB_VIEW_SETTINGS,
    MENU_TAB_ADVANCED,
    MENU_TAB_COUNT
} MenuTab;

typedef enum {
    MENU_ITEM_TOGGLE,
    MENU_ITEM_SLIDER_INT,
    MENU_ITEM_SLIDER_FLOAT,
    MENU_ITEM_ENUM,
    MENU_ITEM_ACTION,
    MENU_ITEM_TEXT_INPUT
} MenuItemType;

typedef struct {
    const char *label;
    const char *description;
    MenuItemType type;
    void *value_ptr;
    float min_val, max_val, step;
    int enum_count;
    const char **enum_names;
} MenuItem;

/* ========== GLOBAL STATE ========== */
/* Data segment base: vaddr 0x44000, file_off 0x48000 */

/* --- Fortnite PS4 offsets (per CUSA07669 / CUSA07022) --- */
/* Resolved at runtime by find_offset() using pattern scanning */
static uintptr_t g_base_address           = 0;  /* Game executable base */
static uintptr_t g_ProcessEvent           = 0;  /* ~0x7D5160 (CUSA07669) */
static uintptr_t g_StaticFindObject       = 0;  /* ~0x97E236 */
static uintptr_t g_StaticConstructObject  = 0;  /* ~0x97E236 area */
static uintptr_t g_GEngine               = 0;  /* ~0x97A1AE area */
static uintptr_t g_GWorld                 = 0;
static uintptr_t g_FNameToString          = 0;
static uintptr_t g_ClientAckGoodMove      = 0;

/* --- Feature toggle state --- */
static bool g_aimbot_enabled        = false;
static bool g_silent_aim            = false;   /* false=Normal, true=Silent */
static bool g_auto_pickaxe          = false;   /* Auto pickaxe (Silent) */
static bool g_aim_require_los       = true;    /* Line of sight check */
static bool g_movement_prediction   = false;
static int  g_aim_bone              = 0;       /* 0=head, 1=neck, 2=chest, 3=pelvis */
static int  g_aim_activation        = 0;       /* 0=ADS, 1=Shoot */
static float g_aim_fov              = 15.0f;   /* degrees */
static float g_aim_smooth           = 5.0f;    /* 1=snap, higher=smoother */
static float g_prediction_gravity   = 980.0f;  /* cm/s^2 */

static bool g_esp_enabled           = false;   /* Master ESP switch */
static bool g_box_esp               = false;
static bool g_skeleton_esp          = false;
static bool g_container_esp         = false;
static bool g_loot_esp              = false;
static bool g_enemy_snaplines       = false;
static bool g_rgb_esp               = false;   /* Cycle colors through rainbow */
static bool g_chams_enabled         = false;
static bool g_rgb_chams             = false;
static bool g_show_searched         = false;   /* Show already-searched containers */
static int  g_esp_range             = 200;     /* meters, 25-500 step 25 */
static int  g_skeleton_range        = 0;       /* 0 = use master range */
static int  g_container_esp_range   = 100;     /* meters */
static int  g_loot_esp_range        = 100;     /* meters */
static int  g_arrow_range           = 0;       /* 0 = use master, 0-2000 step 50 */
static float g_skeleton_thickness   = 1.0f;    /* 0.5-5.0 step 0.5 */
static float g_snapline_thickness   = 1.0f;
static int  g_snapline_origin       = 0;       /* 0=center, 1=bottom, 2=top */
static FLinearColor g_esp_visible_color = {0.0f, 1.0f, 0.0f, 1.0f};  /* green */
static FLinearColor g_esp_blocked_color = {1.0f, 0.0f, 0.0f, 1.0f};  /* red */

static bool g_loot_show_ammo        = false;
static bool g_loot_show_materials   = false;
static bool g_loot_show_healing     = false;
static bool g_loot_show_weapons     = false;
static int  g_loot_rarity_filter    = 0;       /* minimum rarity */

static bool g_crosshair_enabled     = false;
static int  g_crosshair_shape       = 0;       /* 0=Cross, 1=Dot, 2=Circle */
static int  g_crosshair_color       = 0;       /* 0-6 fixed, 7=RGB cycling */
static int  g_crosshair_size        = 10;      /* pixels */
static int  g_crosshair_thickness   = 2;       /* pixels */
static int  g_crosshair_gap         = 3;       /* pixels, Cross only */
static bool g_confirmed_damage      = false;   /* Show damage numbers */
static bool g_hit_marker            = false;

static bool g_infinite_ammo         = false;
static bool g_no_recoil             = false;
static bool g_no_overheat           = false;   /* Vehicle weapons */
static float g_fire_rate            = 1.0f;    /* multiplier */
static float g_pickaxe_speed        = 1.0f;

static bool g_fov_changer           = false;
static float g_camera_fov           = 80.0f;   /* 60-140 step 5 */

static bool g_auto_pickup           = false;
static bool g_auto_pickup_ammo      = false;
static bool g_auto_pickup_materials = false;
static bool g_auto_pickup_healing   = false;
static bool g_auto_pickup_weapons   = false;
static int  g_auto_pickup_rarity    = 0;

static bool g_auto_open_doors       = false;
static bool g_big_head_mode         = false;
static bool g_view_distance_changed = false;
static float g_view_distance_scale  = 1.0f;

/* --- Auth / desync --- */
static bool g_auth_enabled          = false;   /* Freeze server position */
static uint32_t g_auth_dropped_packets = 0;
static bool g_vehicle_desync        = false;

/* --- Vehicle fly / teleport --- */
static bool g_vfly_active           = false;
static bool g_vtp_active            = false;
static float g_vtp_speed            = 500.0f;  /* m/s */

/* --- Bus changer --- */
static UObject *g_custom_bus        = NULL;

/* --- Cosmetic / skin changer --- */
static UObject *g_custom_skin_parts[8] = {0};
static int  g_custom_skin_count     = 0;

/* --- Menu state --- */
static bool g_menu_visible          = false;
static int  g_menu_tab              = 0;
static int  g_menu_selected         = 0;
static float g_menu_pos_x           = 50.0f;
static float g_menu_pos_y           = 50.0f;
static float g_menu_scale           = 1.0f;

/* --- Logging --- */
static int  g_log_fd                = -1;
static char g_log_buf[0x1000];

/* --- Hook state --- */
static void *g_original_vtable       = NULL;
static void *g_patched_vtable        = NULL;
static void *g_console_draw_orig     = NULL;
static void *g_console_input_orig    = NULL;
static uint32_t g_console_draw_count = 0;
static uint32_t g_draw_hud_events    = 0;
static void *g_my_hud               = NULL;
static void *g_last_hud             = NULL;

/* --- Trophy state --- */
static bool g_trophy_running         = false;
static int  g_trophy_unlocked        = 0;
static int  g_trophy_failed          = 0;
static int  g_trophy_total           = 0;

/* --- tpall state --- */
static bool g_tpall_active           = false;
static float g_tpall_distance        = 200.0f;

/* --- Level sequence --- */
static void *g_playing_sequence      = NULL;
static bool g_seq_nocam              = false;
static float g_seq_rate              = 1.0f;

/* --- Matchmaking / island --- */
static char g_island_override[256]   = {0};

/* --- Time of day --- */
static void *g_todm_manager          = NULL;
static void *g_todm_original         = NULL;


/* ========================================================================= */
/*  UTILITY / HELPER FUNCTIONS                                                */
/* ========================================================================= */

/* 0x000700 */
static uintptr_t get_game_base(void)
{
    /* Uses sceKernelGetModuleInfo to find the Fortnite executable base address */
    OrbisKernelModuleInfo info;
    info.size = sizeof(info);
    /* Module handle 0 = main executable */
    if (sceKernelGetModuleInfo(0, 0, &info) == 0)
        return (uintptr_t)info.segmentInfo[0].address;
    return 0;
}

/* 0x000750 - notification helper */
static void send_notification(const char *text)
{
    OrbisNotificationRequest req;
    memset(&req, 0, sizeof(req));
    req.type = 0;
    req.unk3 = 0;
    req.useIconImageUri = 0;
    req.targetId = -1;
    snprintf(req.message, sizeof(req.message), "%s", text);
    sceKernelSendNotificationRequest(0, &req, sizeof(req), 0);
}

/* 0x000770 - StaticFindObject wrapper */
static UObject *find_object(const char *class_name, UObject *outer, const char *name)
{
    typedef UObject *(*StaticFindObject_t)(void *class_obj, UObject *outer,
                                            const wchar_t *name, bool exact);
    /* Convert name to wide string, call StaticFindObject at resolved offset */
    wchar_t wname[512];
    /* ... ascii to wchar conversion ... */
    return ((StaticFindObject_t)(g_base_address + g_StaticFindObject))(
        NULL, outer, wname, false);
}

/* 0x000ba0 - ProcessEvent wrapper */
static void process_event(UObject *obj, UObject *func, void *params)
{
    typedef void (*ProcessEvent_t)(UObject *, UObject *, void *);
    ((ProcessEvent_t)(g_base_address + g_ProcessEvent))(obj, func, params);
}

/* 0x00b090 - FName::ToString wrapper */
static void fname_to_string(FName *name, char *out, int max_len)
{
    /* Calls UE4 FName::ToString via resolved offset */
    /* Converts FName to a human-readable ASCII string */
}

/* 0x00ba90 - GetNameSafe / FName comparison */
static bool fname_equals(FName *name, const char *str)
{
    char buf[256];
    fname_to_string(name, buf, sizeof(buf));
    return strcmp(buf, str) == 0;
}

/* 0x00bec0 - StaticFindObject by name string */
static UObject *static_find_object(const char *full_path)
{
    return find_object(NULL, NULL, full_path);
}

/* 0x00bf60 - IsValid / is live UObject check */
static bool is_valid_uobject(UObject *obj)
{
    if (!obj) return false;
    /* Check UObject internal flags to verify it hasn't been garbage collected */
    /* Uses bit checks on obj->ObjectFlags */
    return true; /* simplified */
}

/* 0x00cae0 - FindFunction: find a UFunction by name on a UObject */
static UObject *find_function(UObject *obj, const char *func_name)
{
    /* Iterates UClass function list, comparing FName to func_name */
    return find_object(NULL, obj, func_name);
}

/* 0x00cdd0 - GetPathName: get full path of a UObject as string */
static const char *get_path_name(UObject *obj, char *buf, int buf_size)
{
    if (!obj) {
        snprintf(buf, buf_size, "NULL");
        return buf;
    }
    if (!is_valid_uobject(obj)) {
        snprintf(buf, buf_size, "%p (not live)", obj);
        return buf;
    }
    /* Call UObject::GetPathName() via ProcessEvent */
    return buf;
}

/* 0x00d060 - GetAssetRegistry helper */
static void *get_asset_registry(void)
{
    UObject *kismet = static_find_object("Default__KismetSystemLibrary");
    UObject *func = find_function(kismet, "GetAssetRegistry");
    /* ... call and return ... */
    return NULL;
}

/* 0x00d190 / 0x00d270 - String/wchar conversion utilities */
static int ascii_to_wchar(const char *src, wchar_t *dst, int max);
static int wchar_to_ascii(const wchar_t *src, char *dst, int max);

/* 0x00f1d0 - SpawnActor via GameplayStatics */
static AActor *spawn_actor(UObject *world, UObject *actor_class,
                           FVector *location, FRotator *rotation)
{
    UObject *gps = static_find_object("GameplayStatics");
    UObject *begin_func = find_function(gps, "BeginDeferredActorSpawnFromClass");
    UObject *finish_func = find_function(gps, "FinishSpawningActor");

    struct {
        UObject *WorldContextObject;
        UObject *ActorClass;
        FVector SpawnLocation;
        FRotator SpawnRotation;
        AActor *ReturnValue;
    } params;

    memset(&params, 0, sizeof(params));
    params.WorldContextObject = world;
    params.ActorClass = actor_class;
    if (location) memcpy(&params.SpawnLocation, location, sizeof(FVector));
    if (rotation) memcpy(&params.SpawnRotation, rotation, sizeof(FRotator));

    process_event(gps, begin_func, &params);

    struct {
        AActor *Actor;
        AActor *ReturnValue;
    } finish_params = { params.ReturnValue, NULL };

    process_event(gps, finish_func, &finish_params);
    return finish_params.ReturnValue;
}


/* ========================================================================= */
/*  LOG FILE MANAGEMENT                                                       */
/* ========================================================================= */

/* 0x00b9e0 - Initialize log file */
static void log_init(void)
{
    /* Rotate old log: fn_log.txt -> fn_log_prev.txt */
    sceKernelUnlink("/data/GoldHEN/fn_log_prev.txt");
    sceKernelRename("/data/GoldHEN/fn_log.txt", "/data/GoldHEN/fn_log_prev.txt");
    g_log_fd = sceKernelOpen("/data/GoldHEN/fn_log.txt",
                              O_WRONLY | O_CREAT | O_TRUNC, 0666);
}

/* 0x0060e0 - Log a message (to file and console) */
static void fn_log(const char *fmt, ...)
{
    va_list args;
    va_start(args, fmt);
    int len = vsnprintf(g_log_buf, sizeof(g_log_buf), fmt, args);
    va_end(args);

    if (g_log_fd >= 0 && len > 0) {
        sceKernelWrite(g_log_fd, g_log_buf, len);
        sceKernelWrite(g_log_fd, "\n", 1);
    }

    /* Also output to GoldHEN console via send_notification if menu is open */
}


/* ========================================================================= */
/*  FORTNITE OFFSET RESOLUTION                                                */
/* ========================================================================= */

/* 0x0007c0 - Resolve game offsets at init */
static bool resolve_offsets(void)
{
    /* Check if /data/GoldHEN/fn_noslate exists -> skip slate UI patching */
    struct stat st;
    bool no_slate = (sceKernelStat("/data/GoldHEN/fn_noslate", &st) == 0);

    g_base_address = get_game_base();
    if (!g_base_address) {
        goldhen_log("(%s:%d) [fn_console] failed to get game base",
                     __FILE__, __LINE__);
        return false;
    }

    /* Title ID check for offset selection */
    /* CUSA07669 and CUSA07022 use the same offsets in this build */

    /* The offsets are hardcoded RIP-relative in the binary: */
    g_ProcessEvent          = 0x007D5160;
    g_StaticFindObject      = 0x0097E236;
    g_GEngine               = 0x0097A1AE;
    /* ... additional offsets resolved similarly ... */
    /* 48 total offsets covering: ProcessEvent, StaticFindObject,
       StaticConstructObject, GEngine, GWorld, FName functions,
       ClientAckGoodMove, and various gameplay function pointers */

    goldhen_log("(%s:%d) [fn_console] title %s, version %s, base 0x%lx",
                __FILE__, __LINE__, "CUSA07669", "Sploited FN v32", g_base_address);

    return true;
}

/* 0x0008f0 - Post-init setup (auto-mount, graphics presets) */
static void post_init_setup(void)
{
    fn_log("auto-mount Figment_Martin %s", "loaded");
    /* Apply default graphics settings */
    cmd_handler("r.SSR.Quality 0"); /* Disable SSR for performance */
    /* Check file existence for fn_noslate, fn_nomenu, fn_nor3 config flags */
}


/* ========================================================================= */
/*  VTABLE HOOK SYSTEM                                                        */
/* ========================================================================= */

/* 0x014100 - plugin_common.c: safe memory write */
static int kern_write(void *dst, const void *src, size_t len)
{
    goldhen_log("(%s:%d)", "../../common/plugin_common.c", __LINE__);
    /* Use sceKernelMprotect to make target writable, memcpy, restore */
    int ret = sceKernelMprotect(dst, len, 7); /* RWX */
    if (ret == 0) {
        memcpy(dst, src, len);
        sceKernelMprotect(dst, len, 5); /* RX */
    }
    return ret;
}

/* 0x0141a0 - plugin_common.c: allocate executable memory for hooks */
static void *alloc_exec_page(size_t size)
{
    void *mem = NULL;
    goldhen_log("(%s:%d)", "../../common/plugin_common.c", __LINE__);
    /* Use sceKernelJitCreateSharedMemory + sceKernelJitMapSharedMemory */
    int handle = sceKernelJitCreateSharedMemory(0, size, 7, NULL);
    if (handle >= 0) {
        sceKernelJitMapSharedMemory(handle, 0, &mem, size, 7, 0);
    }
    return mem;
}

/* 0x0142e0 - Get vtable slot */
static void *get_vtable_slot(void **vtable, int slot)
{
    return vtable[slot];
}

/* 0x014370 / 0x0143d0 - Memory mapping helpers */
static void *mmap_rw(size_t size)
{
    return sceKernelMmap(NULL, size, 0x3 /* PROT_READ|WRITE */,
                         0x1002 /* MAP_ANON|MAP_PRIVATE */, -1, 0);
}

/* 0x014450 - Install vtable hooks for console draw + input */
static bool install_vtable_hooks(void *hud_object)
{
    /* 1. Read the current vtable pointer from the HUD object */
    void **original_vtable = *(void ***)hud_object;
    g_original_vtable = original_vtable;

    /* 2. Allocate a copy of the vtable */
    /* 3. Replace draw/input slots with our hooks */
    /* 4. Write our vtable pointer back to the object */

    /* Uses kern_write() to safely patch vtable pointer */
    /* Specific vtable slot indices for UGameViewportClient::DrawTransition
       and UGameViewportClient::InputKey */

    goldhen_log("(%s:%d) [fn_console] console draw+input hooks installed (vtable %p)",
                __FILE__, __LINE__, g_patched_vtable);
    return true;
}

/* 0x0144c0 - Vtable write helper (with error reporting) */
static bool vtable_write(void **vtable, int slot, void *new_func)
{
    void *page = get_vtable_slot(vtable, slot);
    /* Allocate trampoline page if needed */
    void *trampoline = mmap_rw(0x1000);

    /* Write jump instruction to trampoline */
    int ret = kern_write(&vtable[slot], &new_func, sizeof(void *));
    if (ret != 0) {
        goldhen_log("(%s:%d) [fn_console] vtable write failed for %p slot 0x%x",
                    __FILE__, __LINE__, vtable, slot);
        return false;
    }
    return true;
}

/* 0x0146b0 - Restore original vtable slot */
static bool vtable_restore(void **vtable, int slot, void *orig_func)
{
    return vtable_write(vtable, slot, orig_func);
}


/* ========================================================================= */
/*  AIMBOT SYSTEM                                                             */
/* ========================================================================= */

/* 0x012850 - Get local player pawn */
static AFortPawn *get_local_pawn(void)
{
    /* GWorld -> OwningGameInstance -> LocalPlayers[0] -> PlayerController -> Pawn */
    return NULL; /* resolved via offset chain */
}

/* 0x0009e0 - Aimbot tick (called every frame from ProcessEvent hook) */
static void aimbot_tick(void)
{
    if (!g_aimbot_enabled) return;

    AFortPawn *local_pawn = get_local_pawn();
    if (!local_pawn) return;

    /* Get camera location and rotation */
    FVector cam_loc;
    FRotator cam_rot;
    /* ... read from PlayerController ... */

    /* Find best target */
    AFortPawn *best_target = NULL;
    float best_score = 999999.0f;

    /* Iterate all player pawns in world */
    /* For each enemy pawn:
       1. Check if alive and valid
       2. Get bone location (g_aim_bone: head/neck/chest/pelvis)
       3. Calculate angle from crosshair center
       4. Check within g_aim_fov
       5. If g_aim_require_los: line trace visibility check
       6. If g_movement_prediction: predict position using velocity + g_prediction_gravity
       7. Score by: angle, distance, or health (based on g_aim_sort_mode)
    */

    if (!best_target) return;

    if (g_silent_aim) {
        /* Silent aim: modify bullet trajectory in ProcessEvent
           to redirect toward target bone without moving camera */
        /* Intercepts fire event and adjusts shot direction */
    } else {
        /* Normal aim: smoothly rotate camera toward target */
        /* Apply g_aim_smooth factor (1=instant snap, higher=gradual) */
        FRotator target_rot;
        /* ... calculate rotation to target ... */
        /* Interpolate current -> target rotation by 1/g_aim_smooth */
    }
}

/* 0x001220 - fn.firerate command handler */
static void cmd_firerate(const char *arg)
{
    /* vaddr 0x001220 */
    if (arg && *arg) {
        float rate = strtod(arg, NULL);
        if (rate < 0.1f) rate = 0.1f;
        if (rate > 100.0f) rate = 100.0f;
        g_fire_rate = rate;
    }
    fn_log("fn.firerate %.1f", g_fire_rate);

    /* Fire rate is applied by hooking ProcessEvent:
       When a weapon fire event is processed, the timer/cooldown
       values are divided by g_fire_rate to speed up firing.
       Also affects reload speed and pickaxe swing speed. */
}


/* ========================================================================= */
/*  ESP RENDERING SYSTEM                                                      */
/* ========================================================================= */

/* ESP is rendered via the DrawHUD hook (vtable patched DrawTransition)
   The menu and ESP share the same canvas drawing path. */

/* 0x013a00 - Menu/ESP draw callback (called from hooked DrawHUD) */
static void draw_hud_hook(void *hud, void *canvas)
{
    g_draw_hud_events++;
    g_last_hud = hud;

    if (g_menu_visible) {
        draw_menu(hud, canvas);
    }

    if (g_esp_enabled) {
        draw_esp(hud, canvas);
    }

    if (g_crosshair_enabled) {
        draw_crosshair(hud, canvas);
    }

    if (g_confirmed_damage) {
        draw_damage_numbers(hud, canvas);
    }
}

/* ESP sub-functions (reconstructed from string references) */
static void draw_esp(void *hud, void *canvas)
{
    /* Get screen dimensions from canvas */
    /* Get camera location/rotation for WorldToScreen projection */

    FVector cam_loc;
    FRotator cam_rot;
    /* Read from PlayerController->PlayerCameraManager */

    /* === Player ESP === */
    if (g_box_esp || g_skeleton_esp || g_enemy_snaplines) {
        /* Iterate all FortPawn actors in the world */
        /* For each enemy pawn within g_esp_range meters: */
        /*   1. WorldToScreen project head + feet positions */
        /*   2. Calculate 2D bounding box from 3D bounds */

        /* Box ESP */
        if (g_box_esp) {
            /* Draw 2D rectangle around player
               Color: g_esp_visible_color if visible, g_esp_blocked_color if blocked
               If g_rgb_esp: cycle hue based on game time */
        }

        /* Skeleton ESP */
        if (g_skeleton_esp) {
            /* For pawns within g_skeleton_range (or master range if 0):
               Get bone positions: head, neck, spine, shoulders, elbows, hands,
               hips, knees, feet
               Draw connected lines with g_skeleton_thickness */
        }

        /* Enemy snaplines */
        if (g_enemy_snaplines) {
            /* Draw line from g_snapline_origin (center/bottom/top of screen)
               to enemy feet position
               Width: g_snapline_thickness */
        }

        /* Off-screen arrows */
        if (g_arrow_range > 0) {
            /* For enemies outside screen but within g_arrow_range meters:
               Draw directional arrow at screen edge pointing toward enemy */
        }

        /* Name / distance labels */
        /* Draw player name and distance below/above box */
        /* g_separate_labels: spread overlapping labels vertically */
    }

    /* === Container ESP === */
    if (g_container_esp) {
        /* Search for FortSearchableContainer actors within range */
        /* Draw outline box around each container */
        /* If g_show_searched: also show already-opened containers in grey */
    }

    /* === Loot ESP === */
    if (g_loot_esp) {
        /* Search for FortPickup actors within g_loot_esp_range */
        /* Filter by type: ammo, materials, healing, weapons */
        /* Filter by rarity: g_loot_rarity_filter minimum */
        /* Draw text label with item name and distance */
    }

    /* === Chams === */
    if (g_chams_enabled) {
        /* Apply colored material to enemy player meshes */
        /* If g_rgb_chams: cycle material color through rainbow */
        /* Uses SetOverlayMaterial on skeletal mesh components */
    }

    /* === Big Head Mode === */
    if (g_big_head_mode) {
        /* Scale head bone of enemy pawns */
    }
}

/* Crosshair drawing */
static void draw_crosshair(void *hud, void *canvas)
{
    /* Get screen center coordinates */
    int cx, cy; /* center of screen */

    /* Choose color based on g_crosshair_color (0-6 fixed, 7=RGB cycle) */
    FLinearColor color;
    switch (g_crosshair_color) {
        case 0: color = (FLinearColor){1,1,1,1}; break; /* white */
        case 1: color = (FLinearColor){1,0,0,1}; break; /* red */
        case 2: color = (FLinearColor){0,1,0,1}; break; /* green */
        case 3: color = (FLinearColor){0,0,1,1}; break; /* blue */
        case 4: color = (FLinearColor){1,1,0,1}; break; /* yellow */
        case 5: color = (FLinearColor){0,1,1,1}; break; /* cyan */
        case 6: color = (FLinearColor){1,0,1,1}; break; /* magenta */
        case 7: /* RGB cycling - compute from time */ break;
    }

    switch (g_crosshair_shape) {
        case 0: /* Cross */
            /* Draw 4 lines from center with g_crosshair_gap and g_crosshair_size */
            /* Horizontal: cx-size to cx-gap, cx+gap to cx+size */
            /* Vertical:   cy-size to cy-gap, cy+gap to cy+size */
            break;
        case 1: /* Dot */
            /* Draw filled circle at center, radius = g_crosshair_size/4 */
            break;
        case 2: /* Circle */
            /* Draw circle outline at center, radius = g_crosshair_size */
            break;
    }

    /* Hit marker: if g_hit_marker and recent confirmed damage */
    if (g_hit_marker) {
        /* Draw X-shaped hit marker briefly on confirmed damage */
    }
}

/* Damage number display */
static void draw_damage_numbers(void *hud, void *canvas)
{
    /* Listen for ClientAddDamageNumber events via ProcessEvent hook */
    /* Display floating damage number at hit location */
    /* Numbers float upward and fade out over ~1 second */
}


/* ========================================================================= */
/*  MENU SYSTEM                                                               */
/* ========================================================================= */

/* Menu header: "FN MENU  D-pad move  <> change  X select/type  [] run text  O close" */
/* Toggle: "R1 + Square toggles this menu" */

/* Menu tab descriptions (from rodata strings): */
static const char *menu_tab_descriptions[] = {
    /* Tab 0: Aimbot */
    "Aim mode, activation, target selection and prediction. X enters; Circle returns to tabs.",
    /* Tab 1: Aim circle */
    "Aim-circle range and appearance. X enters; Circle returns to tabs.",
    /* Tab 2: Player ESP */
    "Player labels, boxes, bones and directional markers. X enters; Circle returns to tabs.",
    /* Tab 3: Container/Loot */
    "Player / container ESP",
    /* Tab 4: Crosshair */
    "Center crosshair styling and confirmed damage feedback. X enters to edit.",
    /* Tab 5: View / advanced */
    "X opens the advanced tools. Circle closes the menu from this main tab.",
    /* Tab 6+: additional sub-pages */
};

/* Menu items per tab (reconstructed from string descriptions) */

/* --- AIMBOT TAB --- */
/*
 * "Normal moves the camera; Silent changes shot direction. Each mode keeps its own activation key."
 * "Choose ADS or Shoot for the current aim mode. The other mode keeps its own choice."
 * "Choose the animated bone used for aiming: head, neck, chest or pelvis."
 * "Higher smoothing values make Normal aim turn more gradually. Silent aim does not move the camera."
 * "Require line of sight before selecting a target. Blocked targets release immediately."
 * "Rank eligible targets by crosshair angle, distance or lowest health. Other filters still apply."
 * "Movement prediction"
 * "Prediction gravity (cm/s2)" - "Prediction is OFF. Enable Movement prediction to use gravity correction: 0-3000 cm/s2."
 * "Auto pickaxe (Silent)" - "Auto-swing the equipped pickaxe at eligible Silent aim targets. Normal reach applies; close menu."
 * "Aim FOV (degrees)"
 * "Aim smooth"
 * "Aim height"
 */

/* --- PLAYER ESP TAB --- */
/*
 * "Master switch for player and container ESP. Individual visual options keep their own settings."
 * "Draw boxes around loaded enemies. Skeletons, labels and distance have separate switches."
 * "Box ESP"
 * "Skeleton ESP" - "Draw connected player bones. Uses the skeleton range and thickness settings."
 * "Skeleton range (meters)" - "Skeleton limit: 0-2000m, in 50m steps. 0 uses master range; master still caps this."
 * "Skeleton thickness (px)" - "Skeleton stroke width: 0.5-5.0 pixels, in 0.5-pixel steps."
 * "Enemy snaplines"
 * "Snapline origin" - "Start snaplines at the screen center, bottom or top. Off-screen ends stop at the screen edge."
 * "Snapline thickness (px)" - "Snapline stroke width: 0.5-5.0 pixels, in 0.5-pixel steps."
 * "Off-screen enemy arrows"
 * "Arrow range (meters)" - "Off-screen arrow limit: 0-2000m, in 50m steps. 0 uses the master ESP range."
 * "RGB ESP" - "Cycle ESP colors through a rainbow. Hidden targets use a dimmer shade."
 * "Chams" - chams_enabled
 * "RGB chams" - "Cycle compatible player materials through rainbow colors. Requires Chams ON."
 * "Color for blocked boxes, bones and labels. RGB ESP overrides this while enabled."
 * "Separate overlapping label blocks. Hide crowded labels when no clear space remains."
 */

/* --- CONTAINER ESP TAB --- */
/*
 * "CONTAINER ESP"
 * "Container ESP" - container_esp
 * "Show searched containers" - "Include searched containers in the original container ESP. Requires master ESP ON."
 * "Loot ESP"
 * "Loot ESP distance: 25-500m, in 25m steps. Only items loaded by the game can appear."
 * "Include loaded ammunition in Loot ESP. Auto-pickup filters are separate."
 * "Include loaded weapons in Loot ESP. The loot rarity filter also applies."
 * "Include recognized healing items in Loot ESP. The loot rarity filter also applies."
 */

/* --- CROSSHAIR TAB --- */
/*
 * "CROSSHAIR"
 * "Custom crosshair" - "Show a screen-center crosshair, independent of ESP and ADS. Shape and style are below."
 * "Crosshair shape" - "Choose Cross, Dot or Circle. Enable Custom crosshair to display it in game."
 * "Crosshair color" - "Choose one of seven fixed crosshair colors or continuously cycling RGB."
 * "Crosshair size (px)"
 * "Crosshair thickness (px)"
 * "Crosshair gap (px)" - "Crosshair center gap: 0-16 pixels. Applies to the Cross shape only."
 * "Confirmed damage numbers" - "Show damage from confirmed client damage events. Values are not guessed from fired shots."
 * "Show a hit marker only when the game confirms damage. Firing alone does not trigger it."
 */

/* --- VIEW / SETTINGS TAB --- */
/*
 * "FOV Changer" - "Camera view: 60-140 degrees, in 5-degree steps. Requires FOV Changer ON."
 * "View distance" - "Set the original view-distance value, then select Apply view distance."
 * "Apply view distance" - "Apply the selected view-distance value. Player loading remains controlled by the game."
 * "Auto pickup"
 * "Auto pickup - ammo" - "Allow Auto pickup to collect ammunition. Requires Auto pickup ON."
 * "Auto pickup - materials" - "Allow Auto pickup to collect materials. Requires Auto pickup ON."
 * "Auto pickup - healing" - "Allow Auto pickup to collect recognized healing items. The pickup rarity filter also applies."
 * "Auto pickup - weapons" - "Allow Auto pickup to collect weapons. The pickup rarity filter also applies."
 * "Fire/reload/swing rate"
 * "Keep local bullets on their input aim direction. Separate from recoil and Normal/Silent aim mode."
 * "Unlock all trophies (press twice)"
 */

/* 0x013a00 - Menu drawing function */
static void draw_menu(void *hud, void *canvas)
{
    /* Draw semi-transparent background panel */
    /* Title: "FN MENU  D-pad move  <> change  X select/type  [] run text  O close" */

    /* Draw tab headers at top */
    for (int t = 0; t < MENU_TAB_COUNT; t++) {
        /* Highlight current tab */
        /* Draw tab name */
    }

    /* Draw items for current tab */
    /* Each item: label on left, value/toggle on right */
    /* Selected item has highlight background */
    /* Description text at bottom for selected item */

    /* Input handling via hooked InputKey:
       D-pad Up/Down = navigate items
       D-pad Left/Right = change value
       X = select / enter sub-menu / edit text field
       Square = execute text command
       Circle = back / close
       R1 + Square = toggle menu visibility
    */
}

/* 0x001550 - PS4 on-screen keyboard for text input */
static bool show_keyboard(const char *title, char *out_buf, int max_len)
{
    /* Load libSceImeDialog dynamically */
    int ret = sceKernelLoadStartModule("libSceImeDialog.sprx", 0, NULL, 0, NULL, NULL);
    if (ret < 0) {
        fn_log("PS4 keyboard unavailable (couldn't load libSceImeDialog)");
        return false;
    }

    /* Resolve sceImeDialogInit, sceImeDialogGetResult, sceImeDialogTerm */
    /* Get user ID via sceUserServiceGetInitialUser */
    /* Show dialog, wait for result, copy text to out_buf */
    return true;
}


/* ========================================================================= */
/*  CONSOLE COMMAND HANDLERS                                                  */
/* ========================================================================= */

/* 0x001fc0 - MAIN COMMAND DISPATCHER
 * This is the largest function in the binary (~16KB of code).
 * It receives console text input and dispatches to sub-handlers.
 * All "fn.*" commands route through here.
 */
static void cmd_handler(const char *cmd)
{
    char buf[512];
    char arg1[256], arg2[256], arg3[256];

    /* Skip "fn." prefix if present */
    if (strncmp(cmd, "fn.", 3) == 0)
        cmd += 3;

    /* === SIMPLE TOGGLES (no arguments) === */

    if (strcmp(cmd, "help") == 0) {
        /* Print full command list */
        fn_log("fn.menu / fn.menustatus / fn.keylog   menu debug: toggle menu, draw state, last keys the menu saw");
        fn_log("fn.vtp [height]    fly your vehicle to the map marker at high speed (again = cancel). fn.vtp speed <m/s>, now %.0f", g_vtp_speed);
        fn_log("fn.vtp tp [height] [old] | fn.vtp server   old instant teleport (client) / ask server to teleport you there");
        fn_log("fn.esp [players|all|name|off|status|searched|stencil <n>]  player boxes or container outlines");
        fn_log("fn.bus <BBID_x | bp name | path> | off   change the battle bus (client-side), e.g. fn.bus BBID_WinterBus_V2");
        fn_log("fn.move <style|0-6> [hold] | hold | list   set your EFortMovementStyle: Running Walking Charging Sprinting...");
        fn_log("fn.seq <name|/path> | loop <name> | list [text] | stop | pause | rate <x> | nocam | status  play a level sequence");
        fn_log("fn.vstatus         vehicle fly debug info (vehicle + all input axes)");
        fn_log("fn.aimfov <deg>    aimbot fov, now %.1f", g_aim_fov);
        fn_log("fn.norecoil        remove weapon recoil");
        fn_log("fn.cheat <text>    send any cheat command to the server");
        fn_log("fn.res <percent>   render resolution %% (r.ScreenPercentage), e.g. 70-100");
        fn_log("fn.thankbus        thank the bus driver (ServerThankBusDriver, works while on the bus)");
        fn_log("fn.map             print the name of the current map/world");
        fn_log("fn.plugins         list game feature plugins (ListGameFeaturePlugins)");
        fn_log("fn.mount <plugin or folder> [loaded]  load + activate a game feature plugin");
        fn_log("fn.loadlevel <name or path> [x y z]  load a map client-side");
        fn_log("fn.levelstatus     loaded/visible state of the levels fn.loadlevel made");
        fn_log("fn.sublevels       load the sub-levels of the last fn.loadlevel map");
        fn_log("(every line printed is also saved to /data/GoldHEN/fn_log.txt - check it after a crash)");
        fn_log("fn.tpall [dist]    toggle: pin every pawn to one spot in front of you (client-side), dist %.0f", g_tpall_distance);
        fn_log("fn.firerate <x>    fire rate + reload speed + pickaxe swing multiplier, now %.2f", g_fire_rate);
        fn_log("fn.noheat          vehicle weapons never overheat (boats, turrets, cannons)");
        fn_log("fn.destroy         destroy the actor under your crosshair - anything with collision");
        fn_log("fn.look            print the actor under your crosshair (class, component, hit point)");
        fn_log("fn.trophies [confirm]   unlock every PS4 trophy for this game (irreversible - asks twice)");
        fn_log("fn.view <scale>    view distance scale (r.ViewDistanceScale), 1 = normal");
        fn_log("fn.aimsmooth <n>   1 = snap, higher = smoother, now %.1f", g_aim_smooth);
        fn_log("fn.spawn <class> [fwd]  spawn an actor client-side on your pawn");
        fn_log("fn.findplugin <text>  search for a game feature plugin by name");
        fn_log("fn.vdesync [status] toggle: ignore server corrections for your car");
        fn_log("fn.move <style|0-6> [hold] | hold | list   set your EFortMovementStyle");
        fn_log("fn.gfx <preset>    apply graphics preset");
        fn_log("fn.host <map> <mode> <options>  host a custom match (server travel)");
        fn_log("fn.todm <auto|restore|status>  time of day management");
        return;
    }

    /* === AUTH (freeze server position) === */
    if (strcmp(cmd, "auth") == 0) {
        g_auth_enabled = !g_auth_enabled;
        if (g_auth_enabled) {
            /* Hook ClientAckGoodMove to drop server position corrections */
            UObject *func = find_function(get_local_pawn(), "ClientAckGoodMove");
            if (!func) {
                fn_log("ClientAckGoodMove not found - you'll freeze after a second");
                g_auth_enabled = false;
                return;
            }
            fn_log("auth ON - server position frozen, move freely");
            /* Disable packed movement RPCs */
            cmd_handler("p.NetUsePackedMovementRPCs 1");
        } else {
            fn_log("auth OFF (%u movement packets were dropped) - the server will pull you back",
                    g_auth_dropped_packets);
            g_auth_dropped_packets = 0;
        }
        return;
    }

    /* === ESP === */
    if (strncmp(cmd, "esp", 3) == 0) {
        const char *sub = cmd + 3;
        while (*sub == ' ') sub++;

        if (*sub == '\0' || strcmp(sub, "players") == 0) {
            g_esp_enabled = !g_esp_enabled;
            g_box_esp = g_esp_enabled;
            if (g_esp_enabled)
                fn_log("esp ON - %s", "players");
            else
                fn_log("esp OFF");
        } else if (strcmp(sub, "range") == 0) {
            /* Print/change range */
        } else if (strcmp(sub, "searched") == 0) {
            g_show_searched = !g_show_searched;
            fn_log("esp: %s already-searched containers",
                    g_show_searched ? "showing (grey)" : "OFF");
        } else if (strcmp(sub, "all") == 0) {
            g_esp_enabled = true;
            g_box_esp = true;
            g_container_esp = true;
            g_loot_esp = true;
            fn_log("esp ON - %s", "all");
        } else if (strcmp(sub, "off") == 0) {
            g_esp_enabled = false;
            fn_log("esp OFF");
        } else {
            fn_log("esp %s - %s", sub, "unknown subcommand");
        }
        return;
    }

    /* === BUS CHANGER === */
    if (strncmp(cmd, "bus", 3) == 0) {
        cmd_bus_changer(cmd + 3);
        return;
    }

    /* === COSMETIC / SKIN CHANGER === */
    if (strncmp(cmd, "cosmetic", 8) == 0) {
        cmd_cosmetic(cmd + 8);
        return;
    }

    /* === THANOS MODE (mount Figment_Martin) === */
    if (strcmp(cmd, "thanos") == 0) {
        fn_log("FortniteGame/Plugins/GameFeatures/Figment/Figment_Martin");
        cmd_handler("fn.mount Figment_Martin");
        return;
    }

    /* === MOVE (movement style) === */
    if (strncmp(cmd, "move", 4) == 0) {
        cmd_move(cmd + 4);
        return;
    }

    /* === TODM (time of day) === */
    if (strncmp(cmd, "todm", 4) == 0) {
        cmd_todm(cmd + 4);
        return;
    }

    /* === SUBLEVELS === */
    if (strcmp(cmd, "sublevels") == 0) {
        cmd_sublevels();
        return;
    }

    /* === LOADCELLS === */
    if (strncmp(cmd, "loadcells", 9) == 0) {
        cmd_loadcells(cmd + 9);
        return;
    }

    /* === MESH (skeletal mesh replacement) === */
    if (strncmp(cmd, "mesh", 4) == 0) {
        cmd_mesh(cmd + 4);
        return;
    }

    /* === THANKBUS === */
    if (strcmp(cmd, "thankbus") == 0) {
        /* Call ServerThankBusDriver via ProcessEvent */
        AFortPawn *pawn = get_local_pawn();
        if (!pawn) { fn_log("no pawn"); return; }
        UObject *func = find_function((UObject*)pawn, "ServerThankBusDriver");
        if (func) process_event((UObject*)pawn, func, NULL);
        return;
    }

    /* === DESTROY (trace + destroy actor) === */
    if (strcmp(cmd, "destroy") == 0) {
        cmd_destroy();
        return;
    }

    /* === LOOK (trace + print actor info) === */
    if (strcmp(cmd, "look") == 0) {
        cmd_look();
        return;
    }

    /* === AUTHSTATUS (movement debug) === */
    if (strcmp(cmd, "authstatus") == 0) {
        AFortPawn *pawn = get_local_pawn();
        if (!pawn) { fn_log("no pawn"); return; }
        fn_log("movementMode=%u maxWalk=%.0f velocity=%.0f %.0f %.0f",
               /* read from CharacterMovementComponent */ 0, 0.0f, 0.0f, 0.0f, 0.0f);
        return;
    }

    /* === MOUNT (game feature plugin) === */
    if (strncmp(cmd, "mount", 5) == 0) {
        cmd_mount(cmd + 5);
        return;
    }

    /* === GFSTATE (game feature status) === */
    if (strcmp(cmd, "gfstate") == 0) {
        cmd_gfstate(NULL);
        return;
    }

    /* === FINDPLUGIN (search plugins) === */
    if (strncmp(cmd, "findplugin", 10) == 0) {
        cmd_findplugin(cmd + 10);
        return;
    }

    /* === MAP (print current world) === */
    if (strcmp(cmd, "map") == 0) {
        void *world = *(void **)(g_base_address + g_GWorld);
        if (!world) { fn_log("no world"); return; }
        /* Get world's MapName and OuterName */
        char name_buf[256], pkg_buf[256];
        fn_log("current map: %s  (package %s)", name_buf, pkg_buf);
        return;
    }

    /* === HOST (server travel) === */
    if (strncmp(cmd, "host", 4) == 0) {
        cmd_host(cmd + 4);
        return;
    }

    /* === LOADLEVEL === */
    if (strncmp(cmd, "loadlevel", 9) == 0) {
        cmd_loadlevel(cmd + 9);
        return;
    }

    /* === TPALL (pin all pawns) === */
    if (strncmp(cmd, "tpall", 5) == 0) {
        g_tpall_active = !g_tpall_active;
        if (g_tpall_active) fn_log("tpall ON");
        else fn_log("tpall OFF");
        return;
    }

    /* === PLAYLIST === */
    if (strncmp(cmd, "playlist", 8) == 0) {
        cmd_playlist(cmd + 8);
        return;
    }

    /* === MM (matchmaking island override) === */
    if (strncmp(cmd, "mm", 2) == 0) {
        cmd_matchmaking(cmd + 2);
        return;
    }

    /* === GFX (graphics presets) === */
    if (strncmp(cmd, "gfx", 3) == 0) {
        /* Execute graphics console commands: r.SSR.Quality, ContactShadows, etc. */
        cmd_handler(cmd + 4); /* Forward as raw UE4 console command */
        return;
    }

    /* === FPS === */
    if (strncmp(cmd, "fps", 3) == 0) {
        cmd_fps(cmd + 3);
        return;
    }

    /* === RES (render resolution) === */
    if (strncmp(cmd, "res", 3) == 0) {
        /* r.ScreenPercentage <value> */
        return;
    }

    /* === VIEW DISTANCE === */
    if (strncmp(cmd, "view", 4) == 0) {
        float scale = strtod(cmd + 4, NULL);
        if (scale > 0.0f) {
            g_view_distance_scale = scale;
            char vd_cmd[64];
            snprintf(vd_cmd, sizeof(vd_cmd), "r.ViewDistanceScale %s", cmd + 5);
            /* Execute via UE4 console */
        }
        return;
    }

    /* === CANCEL MATCHMAKING === */
    if (strcmp(cmd, "cancel") == 0) {
        /* CancelMatchmaking */
        return;
    }

    /* === STW COMMANDS === */
    if (strncmp(cmd, "stw", 3) == 0) {
        cmd_stw(cmd + 3);
        return;
    }

    /* === GIVE (weapon/item) === */
    if (strncmp(cmd, "give", 4) == 0) {
        cmd_give(cmd + 4);
        return;
    }

    /* === R3CONSOLE === */
    if (strcmp(cmd, "r3console") == 0) {
        /* Print engine object dump info */
        return;
    }

    /* === KEYLOG (debug input) === */
    if (strcmp(cmd, "keylog") == 0) {
        fn_log("console InputKey calls: %u (newest last)", /* g_keylog_count */ 0);
        return;
    }

    /* === MENUHOOK (debug) === */
    if (strcmp(cmd, "menuhook") == 0) {
        /* Toggle menu hook debug output */
        return;
    }

    /* === MENU === */
    if (strcmp(cmd, "menu") == 0) {
        g_menu_visible = !g_menu_visible;
        fn_log("menu %s (draw events so far: %u)",
               g_menu_visible ? "open" : "closed", g_draw_hud_events);
        return;
    }

    /* === MENUSTATUS (debug) === */
    if (strcmp(cmd, "menustatus") == 0) {
        fn_log("consoleDraws=%u (menu draw hook) drawHudEvents=%u lastHud=%p myHud=%p",
               g_console_draw_count, g_draw_hud_events, g_last_hud, g_my_hud);
        return;
    }

    /* === FORCEHUD (debug) === */
    if (strcmp(cmd, "forcehud") == 0) {
        /* Force HUD visibility flags */
        return;
    }

    /* === VDESYNC (vehicle desync) === */
    if (strncmp(cmd, "vdesync", 7) == 0) {
        g_vehicle_desync = !g_vehicle_desync;
        fn_log("vehicle desync %s - %s",
               g_vehicle_desync ? "ON" : "OFF",
               g_vehicle_desync ? "get in a car (or use fn.vtp)" :
                   "server corrections restored");
        return;
    }

    /* === VTP (vehicle teleport/fly) === */
    if (strncmp(cmd, "vtp", 3) == 0) {
        cmd_vtp(cmd + 3);
        return;
    }

    /* === VSTATUS (vehicle debug) === */
    if (strcmp(cmd, "vstatus") == 0) {
        /* Print vehicle physics state and input axes */
        fn_log("input: fwd=%.2f right=%.2f pitch=%.2f [3]=%.2f [4]=%.2f steer=%.2f",
               0.0f, 0.0f, 0.0f, 0.0f, 0.0f, 0.0f);
        return;
    }

    /* === AIMOFFSET === */
    if (strncmp(cmd, "aimoffset", 9) == 0) {
        fn_log("aim offset = %.1f", /* g_aim_offset */ 0.0f);
        return;
    }

    /* === FIRERATE === */
    if (strncmp(cmd, "firerate", 8) == 0) {
        cmd_firerate(cmd + 8);
        fn_log("fire rate + reload x%.2f (guns + pickaxe)%s", g_fire_rate,
               g_fire_rate != 1.0f ? "" : " (normal)");
        return;
    }

    /* === NOHEAT === */
    if (strcmp(cmd, "noheat") == 0) {
        g_no_overheat = !g_no_overheat;
        fn_log("no overheat %s", g_no_overheat ? "ON" : "OFF");
        return;
    }

    /* === NORECOIL === */
    if (strcmp(cmd, "norecoil") == 0) {
        g_no_recoil = !g_no_recoil;
        fn_log("no recoil %s", g_no_recoil ? "ON" : "OFF");
        return;
    }

    /* === AMMO (infinite ammo) === */
    if (strcmp(cmd, "ammo") == 0) {
        g_infinite_ammo = !g_infinite_ammo;
        fn_log("infinite ammo %s", g_infinite_ammo ? "ON" : "OFF");
        return;
    }

    /* === AIMBOT === */
    if (strcmp(cmd, "aimbot") == 0) {
        g_aimbot_enabled = !g_aimbot_enabled;
        fn_log("aimbot %s", g_aimbot_enabled ? "ON" : "OFF");
        return;
    }

    /* === AIMSMOOTH === */
    if (strncmp(cmd, "aimsmooth", 9) == 0) {
        float val = strtod(cmd + 9, NULL);
        if (val >= 1.0f) g_aim_smooth = val;
        fn_log("fn.aimsmooth <n>   1 = snap, higher = smoother, now %.1f", g_aim_smooth);
        return;
    }

    /* === AIMFOV === */
    if (strncmp(cmd, "aimfov", 6) == 0) {
        float val = strtod(cmd + 6, NULL);
        if (val > 0.0f) g_aim_fov = val;
        fn_log("fn.aimfov <deg>    aimbot fov, now %.1f", g_aim_fov);
        return;
    }

    /* === SEQ (level sequence playback) === */
    if (strncmp(cmd, "seq", 3) == 0) {
        cmd_seq(cmd + 3);
        return;
    }

    /* === SPAWN === */
    if (strncmp(cmd, "spawn", 5) == 0) {
        cmd_spawn(cmd + 5);
        return;
    }

    /* === TROPHIES === */
    if (strncmp(cmd, "trophies", 8) == 0) {
        cmd_trophies(cmd + 8);
        return;
    }

    /* === PLUGINS (list game feature plugins) === */
    if (strcmp(cmd, "plugins") == 0) {
        cmd_list_plugins();
        return;
    }

    /* === Fall through: try as raw UE4 console command === */
    /* Forward to UGameplayStatics::ExecuteConsoleCommand */
    fn_log("fn.cheat %s", cmd);
}


/* ========================================================================= */
/*  SUB-COMMAND HANDLERS                                                      */
/* ========================================================================= */

/* 0x006570 - Bus changer */
static void cmd_bus_changer(const char *arg)
{
    while (*arg == ' ') arg++;

    if (!*arg || strcmp(arg, "off") == 0) {
        g_custom_bus = NULL;
        fn_log("bus changer off");
        return;
    }

    /* Try BBID_ prefix first */
    if (strncmp(arg, "BBID_", 5) == 0) {
        /* Search asset registry for matching battle bus cosmetic */
        UObject *bus_item = find_bus_asset(arg);
        if (bus_item) {
            /* Load and apply bus prefab */
            g_custom_bus = bus_item;
            return;
        }
    }

    /* Try as blueprint path */
    UObject *obj = static_find_object(arg);
    if (obj) {
        /* Verify it's a battle bus item */
        char class_name[256];
        get_path_name(obj, class_name, sizeof(class_name));
        if (strstr(class_name, "BattleBus") || strstr(class_name, "Aircraft")) {
            g_custom_bus = obj;
            return;
        }
        fn_log("'%s' is a %s, not a battle bus item", arg, class_name);
        return;
    }

    fn_log("no bus '%s' - use a cosmetic id (BBID_WinterBus_V2), a BP name from /Game/Athena/Aircraft/Skins/, or a full path", arg);
}

/* 0x006a80 - Cosmetic/skin changer */
static void cmd_cosmetic(const char *arg)
{
    while (*arg == ' ') arg++;

    /* Look up CID_ (Character Item Definition) */
    char cid_name[256];
    if (strncmp(arg, "CID_", 4) != 0) {
        fn_log("[skin] not an asset name - trying it as a display name");
        /* Search by display name */
    }

    fn_log("[skin] CID: %s", cid_name);

    /* Find the CharacterItemDefinition asset */
    UObject *cid = find_asset_by_name(arg, "CharacterItemDefinition");
    if (!cid) return;

    /* Get BaseCharacterParts array from the CID */
    fn_log("[skin] BaseCharacterParts: data=%p num=%d", NULL, 0);

    /* Load each character part and apply to the local pawn */
    /* Uses SetOverlayMaterial and part replacement via ProcessEvent */
}

/* 0x007080 - Movement style command */
static void cmd_move(const char *arg)
{
    while (*arg == ' ') arg++;

    AFortPawn *pawn = get_local_pawn();
    if (!pawn) { fn_log("no pawn"); return; }

    if (strcmp(arg, "list") == 0) {
        /* Print available movement styles */
        return;
    }
    if (strcmp(arg, "hold") == 0) {
        /* Toggle hold mode */
        return;
    }

    /* Find SetCurrentMovementStyle function */
    UObject *func = find_function((UObject*)pawn, "SetCurrentMovementStyle");
    if (!func) return;

    /* Parse style number 0-6 or name */
    /* 0=Running, 1=Walking, 2=Charging, 3=Sprinting,
       4=PersonalVehicle, 5=Flying, 6=Tethered */

    fn_log("worked");
}

/* 0x007520 - Time of day management */
static void cmd_todm(const char *arg)
{
    while (*arg == ' ') arg++;

    if (strcmp(arg, "restore") == 0) {
        /* Restore original time-of-day manager */
        fn_log("[todm] now using the loaded map's time of day (fn.todm restore = back to the original)");
        return;
    }
    if (strcmp(arg, "auto") == 0) {
        /* Auto time-of-day mode */
        return;
    }
    if (strcmp(arg, "status") == 0) {
        /* Print current TODM state */
        return;
    }

    /* Find the loaded level's WorldTimeOfDayManager */
    fn_log("[todm] level not loaded");
}

/* 0x007710 - Level status command */
static void cmd_levelstatus(void)
{
    /* Print loaded/visible state of levels created by fn.loadlevel */
    fn_log("no levels loaded with fn.loadlevel yet");
}

/* 0x007a90 - Load cells (World Partition) */
static void cmd_loadcells(const char *arg)
{
    fn_log("[cells] GetAssetsByPath=%p registry=%p", NULL, NULL);
    /* Use GetAssetsByPath to find cell levels */
    /* Request streaming of nearby cells */
}

/* 0x007e40 - Trophy unlocker */
static void cmd_trophies(const char *arg)
{
    while (*arg == ' ') arg++;

    if (g_trophy_running) {
        fn_log("[trophy] already running (%d/%d)", g_trophy_unlocked, g_trophy_total);
        return;
    }

    if (strcmp(arg, "confirm") != 0) {
        fn_log("[trophy] this unlocks EVERY Fortnite trophy on your real PSN profile and can't be undone.");
        fn_log("[trophy] press it again within 15 s (or fn.trophies confirm) to go ahead.");
        /* Set confirmation timer */
        return;
    }

    /* Actually unlock trophies */
    trophy_unlock_all();
}

/* 0x00f600 - Trophy unlock implementation */
static void trophy_unlock_all(void)
{
    /* Load libSceNpTrophy.sprx */
    int handle = sceKernelLoadStartModule("libSceNpTrophy.sprx", 0, NULL, 0, NULL, NULL);

    /* Resolve trophy API functions */
    void *ctx_func = NULL, *unlock_func = NULL, *state_func = NULL;
    /* dlsym: sceNpTrophyCreateContext, sceNpTrophyUnlockTrophy,
              sceNpTrophyGetTrophyUnlockState */

    fn_log("[trophy] lib: ctx=%p handle=%p register=%p unlock=%p state=%p user=%p",
            ctx_func, NULL, NULL, unlock_func, state_func, NULL);

    /* Create trophy context */
    int32_t trophy_ctx = -1;
    int32_t trophy_handle = -1;
    /* sceNpTrophyCreateContext -> trophy_ctx */
    fn_log("[trophy] CreateHandle -> 0x%08x handle %d", 0, trophy_handle);

    /* Register context */
    fn_log("[trophy] RegisterContext -> 0x%08x", 0);

    /* Get current unlock state */
    uint32_t total_trophies = 0;
    int already_unlocked = 0;
    fn_log("[trophy] GetTrophyUnlockState -> 0x%08x: %u trophies, %d already unlocked",
            0, total_trophies, already_unlocked);

    if (total_trophies == 0) {
        fn_log("[trophy] no usable trophy context");
        return;
    }

    /* Unlock remaining trophies one by one */
    g_trophy_running = true;
    g_trophy_total = total_trophies;
    fn_log("[trophy] unlocking the rest one at a time...");

    for (uint32_t i = 0; i < total_trophies; i++) {
        /* Check if already unlocked, skip if so */
        /* sceNpTrophyUnlockTrophy(trophy_ctx, trophy_handle, i) */
        sceKernelUsleep(100000); /* 100ms between unlocks */
    }

    fn_log("[trophy] done: %d unlocked, %d failed", g_trophy_unlocked, g_trophy_failed);
    g_trophy_running = false;
}

/* 0x007ef0 - Level sequence player */
static void cmd_seq(const char *arg)
{
    while (*arg == ' ') arg++;

    if (!*arg) {
        fn_log("fn.seq stop | pause | rate <x> | nocam | status");
        return;
    }

    if (strcmp(arg, "stop") == 0) {
        if (!g_playing_sequence) {
            fn_log("[seq] nothing playing");
            return;
        }
        /* Stop current sequence */
        return;
    }

    if (strcmp(arg, "pause") == 0) {
        /* Toggle pause */
        return;
    }

    if (strncmp(arg, "rate", 4) == 0) {
        if (!g_playing_sequence) {
            fn_log("[seq] nothing playing");
            return;
        }
        float rate = strtod(arg + 4, NULL);
        if (rate > 0) g_seq_rate = rate;
        return;
    }

    if (strcmp(arg, "nocam") == 0) {
        g_seq_nocam = !g_seq_nocam;
        fn_log("fn.seq nocam %s", g_seq_nocam ? "ON" : "OFF (you keep your camera)");
        return;
    }

    if (strcmp(arg, "status") == 0) {
        if (!g_playing_sequence) {
            fn_log("[seq] nothing playing");
            return;
        }
        /* GetCurrentTime */
        fn_log("[seq] %s at %.1f s", "playing", 0.0f);
        return;
    }

    if (strncmp(arg, "list", 4) == 0) {
        cmd_seq_list(arg + 4);
        return;
    }

    if (strncmp(arg, "loop", 4) == 0) {
        cmd_seq_play(arg + 4, true);
        return;
    }

    /* Play sequence by name */
    cmd_seq_play(arg, false);
}

/* 0x00fc20 - Search for level sequences */
static void cmd_seq_list(const char *filter)
{
    /* Use asset registry to find LevelSequence assets */
    /* /Script/LevelSequence.LevelSequence */
    int found = 0, total = 0;
    fn_log("[seq] %d of %d level sequences match '%s'%s",
            found, total, filter, found > 20 ? " (showing first 20)" : "");
}

/* 0x010110 - Play a level sequence */
static void cmd_seq_play(const char *name, bool loop)
{
    /* Find or load the level sequence asset */
    /* Spawn ALevelSequenceActor */
    /* Get ULevelSequencePlayer component */
    /* Call Play or PlayLooping */
    if (loop) {
        fn_log("fn.seq loop %s", name);
    }
    if (g_seq_nocam) {
        fn_log(", camera cuts off");
    }
}

/* 0x008310 - Mesh replacement command */
static void cmd_mesh(const char *arg)
{
    /* Parse: <mesh_path> <slot> <material> */
    char mesh_path[256], slot_name[32], mat_name[16];
    int n = sscanf(arg, "%255s %31s %15s", mesh_path, slot_name, mat_name);

    /* Load the skeletal mesh asset */
    /* If not loadable, try mounting the plugin that contains it */
    fn_log("[mesh] not loadable yet - mounting '%s'; run the command again in a few seconds", mesh_path);

    /* Apply to pawn's skeletal mesh component in the specified slot */
    /* Slots: body, head, hat, faceacc, misc */
}

/* 0x0087c0 - Spawn actor command */
static void cmd_spawn(const char *arg)
{
    while (*arg == ' ') arg++;

    AFortPawn *pawn = get_local_pawn();
    if (!pawn) { fn_log("no pawn"); return; }

    /* Parse class name and optional forward distance */
    char class_name[256];
    float fwd_distance = 200.0f; /* default 2m in front */

    /* Load the actor class */
    UObject *actor_class = static_find_object(class_name);
    if (!actor_class) {
        fn_log("no class '%s' - try the full path, e.g. fn.spawn /Game/Athena/Items/.../BP_Thing", class_name);
        return;
    }

    /* Calculate spawn location in front of player */
    FVector spawn_loc = pawn->Base.Location;
    /* Add forward vector * fwd_distance */

    AActor *spawned = spawn_actor(NULL, actor_class, &spawn_loc, NULL);
    fn_log("spawned %s (%s) at %.0f %.0f %.0f - fn.destroy it when you're done",
            class_name, "Actor", spawn_loc.X, spawn_loc.Y, spawn_loc.Z);
}

/* 0x008f00 - Look/Destroy: line trace from crosshair */
static void cmd_look_or_destroy(bool do_destroy)
{
    /* Find LineTraceSingleForObjects function */
    UObject *trace_func = find_function(NULL, "LineTraceSingleForObjects");
    if (!trace_func) {
        fn_log("line trace function not found");
        return;
    }

    /* Perform trace from camera through crosshair */
    /* If hit: report actor class and component */
    /* If do_destroy: call K2_DestroyActor */

    fn_log("nothing with collision in front of you (within %.0fm)", 10000.0f);
}

static void cmd_destroy(void) { cmd_look_or_destroy(true); }
static void cmd_look(void)    { cmd_look_or_destroy(false); }

/* 0x00b2f0 - Vehicle fly to marker */
static void cmd_vtp_fly(void)
{
    if (g_vtp_active) {
        fn_log("cancelled");
        g_vtp_active = false;
        return;
    }

    /* Get map marker position */
    FVector marker_pos;
    if (!get_marker_position(&marker_pos)) return;

    g_vtp_active = true;
    float distance = /* calculate 3D distance */ 0.0f;
    fn_log("vtp: flying to marker (%.0f m away) at %.0f m/s - run it again to cancel",
            distance, g_vtp_speed);

    /* Start vehicle fly thread/tick */
}

/* 0x00b600 - Vehicle teleport to marker */
static void cmd_vtp_teleport(const char *arg)
{
    /* Get marker position */
    FVector marker_pos;
    if (!get_marker_position(&marker_pos)) return;

    if (strcmp(arg, "server") == 0) {
        /* RequestVehicleTeleport via server RPC */
        fn_log("asked the server to teleport you to your map marker (ServerTeleportToMapMarker)");
        return;
    }

    /* Client-side teleport via K2_SetActorLocation */
    g_vehicle_desync = true;
    fn_log("vehicle desync ON (car is client-side now)");

    UObject *tp_func = find_function(NULL, "TeleportTo");
    if (tp_func) {
        fn_log("%s %s to marker %.0f %.0f %.0f",
                "TeleportTo", "OK", marker_pos.X, marker_pos.Y, marker_pos.Z);
    } else {
        fn_log("%s %s", "TeleportTo", "FAILED");
    }
}

/* 0x005761 - VTP command parser */
static void cmd_vtp(const char *arg)
{
    while (*arg == ' ') arg++;

    if (strncmp(arg, "speed", 5) == 0) {
        float speed = strtod(arg + 5, NULL);
        if (speed > 0) g_vtp_speed = speed;
        fn_log("vtp speed = %.0f m/s", g_vtp_speed);
        return;
    }

    if (strcmp(arg, "warp") == 0) {
        cmd_vtp_fly();
        return;
    }

    if (strcmp(arg, "server") == 0) {
        cmd_vtp_teleport("server");
        return;
    }

    if (strncmp(arg, "tp", 2) == 0) {
        cmd_vtp_teleport(arg + 2);
        return;
    }

    /* Default: toggle vehicle fly */
    cmd_vtp_fly();
}

/* 0x008cf0 - Get map marker position */
static bool get_marker_position(FVector *out)
{
    /* Find the marker component on the local player state */
    /* Read marker entries array, find newest marker */

    fn_log("marker component not found");
    /* or: fn_log("no map marker placed"); */
    return false;
}

/* 0x009300 - Mount STW (Save the World) plugins */
static void cmd_mount_stw(void)
{
    /* Mount SaveTheWorldRoot first, then SaveTheWorld */
    fn_log("SaveTheWorld needs SaveTheWorldRoot - loading that first");

    /* file:../../../FortniteGame/Plugins/GameFeatures/SaveTheWorldRoot/SaveTheWorldRoot.uplugin */
    /* file:../../../FortniteGame/Plugins/GameFeatures/SaveTheWorld/SaveTheWorld.uplugin */
    /* Also mount STW sub-plugins matching STW_* */

    fn_log("asked GameFeatures to mount '%s' (Loaded, not activated)", "SaveTheWorld");
}

/* 0x006ec0 / 0x006f70 - Mount generic game feature plugin */
static void cmd_mount(const char *arg)
{
    while (*arg == ' ') arg++;

    fn_log("[mount] mounting %s (content only, not activated)", arg);

    /* If STW-related, use special STW mount path */
    if (strstr(arg, "SaveTheWorld") || strstr(arg, "stw")) {
        cmd_mount_stw();
        return;
    }

    /* Build uplugin path and request mount via GameFeaturesSubsystem */
    /* file:../../../FortniteGame/Plugins/GameFeatures/<name>/<name>.uplugin */
}

/* 0x0099c0 - Game feature state query */
static void cmd_gfstate(const char *filter)
{
    /* Find UGameFeaturesSubsystem */
    UObject *gfs = static_find_object("GameFeaturesSubsystem");
    if (!gfs) {
        fn_log("[gfstate] GameFeaturesSubsystem not found");
        return;
    }

    /* Iterate registered game feature plugins, print state */
    int matched = 0, total = 0;
    fn_log("[gfstate] %d of %d listed lines matched '%s'%s",
            matched, total, filter ? filter : "*", "");
}

/* 0x009cf0 - Find plugin by name */
static void cmd_findplugin(const char *arg)
{
    /* Search ListGameFeaturePlugins output for matching names */
    fn_log("%d plugins match '%s'%s", 0, arg, "");
}

/* 0x00a7f0 - Load level instance */
static void cmd_loadlevel(const char *arg)
{
    while (*arg == ' ') arg++;

    char path[200];
    double x = 0, y = 0, z = 0;
    int n = sscanf(arg, "%199s %lf %lf %lf", path, &x, &y, &z);

    /* If short name, try known mappings first */
    /* Otherwise require full path like /Game/Folder/MapName */

    fn_log("'%s' -> %s (remembered)", path, "resolved_path");

    /* Need full path, not short name */
    if (path[0] != '/') {
        fn_log("give the full path instead, e.g. fn.loadlevel /Figment_S10_Map/Athena_Terrain_S10 (it mounts the plugin itself)");
        return;
    }

    /* Call UGameplayStatics::LoadLevelInstance */
    FVector loc = {(float)x, (float)y, (float)z};
    fn_log("LoadLevelInstance('%s') at %.0f %.0f %.0f -> %s (streaming level %p)",
            path, loc.X, loc.Y, loc.Z, "OK", NULL);
}

/* 0x008e70 / 0x006390 - Get player controller / game instance */
static void *get_player_controller(void);
static void *get_game_instance(void);

/* 0x00adc0 - Start matchmaking */
static void cmd_matchmaking(const char *arg)
{
    while (*arg == ' ') arg++;

    /* Find FortMatchmakingContext */
    void *ctx = /* ... */ NULL;
    if (!ctx) {
        fn_log("matchmaking context not found (are you in the lobby?)");
        return;
    }

    /* Find and call StartMatchmaking on context */
    fn_log("matchmaking started (context %p)", ctx);
}

/* 0x00ae30 - Matchmaking helper (call method by name) */
static void matchmaking_call(const char *method_name)
{
    void *ctx = NULL;
    if (!ctx) {
        fn_log("[mm] matchmaking context not found (go to the lobby)");
        return;
    }

    UObject *func = find_function((UObject*)ctx, method_name);
    if (!func) {
        fn_log("[mm] FortMatchmakingContext::%s not found", method_name);
        return;
    }

    process_event((UObject*)ctx, func, NULL);
    fn_log("[mm] %s called on %s", method_name, "FortMatchmakingContext");
}

/* Host command */
static void cmd_host(const char *arg)
{
    while (*arg == ' ') arg++;

    char map[200], mode[160], opts[160];
    int n = sscanf(arg, "%199s %159s %159s", map, mode, opts);

    if (n < 1) return;

    /* Default game mode */
    if (n < 2) strcpy(mode, "/Script/FortniteGame.FortGameModeAthena");

    /* Build ServerTravel URL */
    char travel_url[512];
    snprintf(travel_url, sizeof(travel_url), "open %s?game=%s%s",
             map, mode, n >= 3 ? opts : "?listen");

    fn_log("opening: %s", travel_url);
    /* Execute via console command */
}

/* FPS command */
static void cmd_fps(const char *arg)
{
    while (*arg == ' ') arg++;
    int fps = atoi(arg);
    if (fps > 0) {
        char cmd_buf[64];
        /* Set VSync interval and max FPS */
        int sync_interval = (fps <= 30) ? 2 : (fps <= 60) ? 1 : 0;
        snprintf(cmd_buf, sizeof(cmd_buf), "rhi.SyncInterval %d", sync_interval);
        cmd_handler(cmd_buf);
        snprintf(cmd_buf, sizeof(cmd_buf), "r.VSync %d", sync_interval > 0 ? 1 : 0);
        cmd_handler(cmd_buf);
        /* t.MaxFPS <value> */
        fn_log("fps cap %d (sync interval %d)", fps, sync_interval);
    }
}

/* Give weapon/item */
static void cmd_give(const char *arg)
{
    while (*arg == ' ') arg++;

    char item_name[128];
    int count = 1;
    sscanf(arg, "%127s %d", item_name, &count);

    /* If starts with WID_, it's a weapon item definition */
    if (strncmp(item_name, "WID_", 4) == 0) {
        /* Look up in asset registry and grant via server */
    }

    /* Send cheat command: fn.give <item> */
    fn_log("fn.give %s", item_name);
}

/* STW sub-commands */
static void cmd_stw(const char *arg)
{
    while (*arg == ' ') arg++;

    if (strncmp(arg, "quickplay", 9) == 0) {
        /* StartQuickPlay */
        return;
    }
    if (strcmp(arg, "outpost") == 0) {
        /* Load outpost/homebase */
        return;
    }
    if (strcmp(arg, "critical") == 0) {
        /* StartCriticalMission */
        return;
    }
    if (strcmp(arg, "wargame") == 0) {
        /* StartWargameSimulationAssist */
        return;
    }
    if (strcmp(arg, "hestia") == 0) {
        /* StartHestiaBeautyMission */
        return;
    }
    /* etc */
}

/* Playlist command */
static void cmd_playlist(const char *arg)
{
    while (*arg == ' ') arg++;
    /* Set MM.IslandOverride */
    fn_log("MM.IslandOverride");
}

/* List game feature plugins */
static void cmd_list_plugins(void)
{
    UObject *func = static_find_object("ListGameFeaturePlugins");
    /* Call and print results */
}

/* Asset finding helpers */
static UObject *find_asset_by_name(const char *name, const char *class_filter)
{
    /* Use asset registry to search by name, optionally filtered by class */
    return NULL;
}

static UObject *find_bus_asset(const char *bbid)
{
    /* Search for BBID_* in battle bus cosmetic assets */
    UObject *def = static_find_object("Default__KismetSystemLibrary");
    UObject *make_path_func = find_function(def, "MakeSoftObjectPath");
    /* ... */
    return NULL;
}

static bool get_marker_position_impl(FVector *out);
static void cmd_sublevels(void);


/* ========================================================================= */
/*  PROCESSEVENT HOOK                                                         */
/* ========================================================================= */

/* 0x000c20 / 0x000d30 - ProcessEvent hook (intercepts all UE4 events) */
static void hooked_process_event(UObject *obj, UObject *func, void *params)
{
    /* This is the core hook that enables most features.
       It intercepts ProcessEvent calls and:
       1. Detects weapon fire events -> apply fire rate multiplier
       2. Detects ammo consumption -> block if infinite ammo
       3. Detects recoil application -> block if no recoil
       4. Detects vehicle overheat -> block if no overheat
       5. Detects HUD draw -> inject ESP/menu rendering
       6. Detects input events -> handle menu navigation
       7. Detects ClientAckGoodMove -> block if auth enabled
       8. Calls aimbot_tick() on camera update events
    */

    /* Check function name against known event names */
    /* If it's a fire rate timer: multiply by g_fire_rate */
    /* If it's ammo decrement and g_infinite_ammo: skip */
    /* If it's recoil and g_no_recoil: zero out */
    /* If it's overheat and g_no_overheat: reset */
    /* If it's ClientAckGoodMove and g_auth_enabled: drop, increment counter */

    /* Call original ProcessEvent */
    /* original_ProcessEvent(obj, func, params); */
}


/* ========================================================================= */
/*  INPUT HOOK (console command interception)                                 */
/* ========================================================================= */

/* 0x000e30 - Console input hook */
static bool hooked_input_key(void *viewport, int controller_id,
                              FName key, int event_type, float amount,
                              bool gamepad)
{
    /* Log input if keylog mode is active */

    /* Check for menu toggle: R1 + Square */
    /* Check for D-pad navigation when menu is visible */
    /* Check for X/Circle/Square button actions */

    /* If input starts with "fn." -> route to cmd_handler */

    /* If text starts with a known command prefix, handle it */
    /* Otherwise pass to original InputKey */

    return true; /* consumed */
}


/* ========================================================================= */
/*  PLUGIN ENTRY POINTS                                                       */
/* ========================================================================= */

/* 0x0135e0 - module_start (GoldHEN plugin entry) */
int module_start(size_t argc, const void *argv)
{
    goldhen_log("(%s:%d) [fn_console] starting", __FILE__, __LINE__);

    /* Check for /data/GoldHEN/fn_nomenu flag */
    /* Check for /data/GoldHEN/fn_nor3 flag */

    /* Initialize log file */
    log_init();

    /* Resolve game offsets */
    if (!resolve_offsets()) {
        return -1;
    }

    /* Allocate callback return page for hooks */
    void *callback_page = alloc_exec_page(0x4000);
    if (!callback_page) {
        fn_log("Callback return page could not be allocated. No hooks installed.");
        return -1;
    }

    /* Protect extension pages */
    /* "Extension page protection was rejected. No hooks installed." */

    /* Install vtable hooks for console draw and input */
    /* These hook into UGameViewportClient's vtable to:
       1. Intercept DrawTransition for ESP/menu rendering
       2. Intercept InputKey for command input and menu navigation
    */

    /* Post-init setup (auto-mount content, apply defaults) */
    post_init_setup();

    /* Notify user */
    send_notification("Sploited FN v32 loaded");
    fn_log("R1 + Square toggles this menu");
    fn_log("D-pad right now opens the console (via the input hook)");

    return 0;
}

/* 0x0137a0 / 0x013810 / 0x013880 - module_stop / cleanup */
int module_stop(void)
{
    /* Restore original vtable entries */
    /* Close log file */
    if (g_log_fd >= 0) {
        sceKernelClose(g_log_fd);
        g_log_fd = -1;
    }
    return 0;
}


/* ========================================================================= */
/*  ASSET REGISTRY HELPERS (from fn_console_unlocker internals)               */
/* ========================================================================= */

/* 0x00c020 - Search asset registry by class and name */
static UObject *registry_find_asset(const char *class_path, const char *name)
{
    /* Use /Script/FortniteGame asset path lookups */
    /* Search among registered assets, fuzzy match if needed */
    fn_log("[reg] %s found among %d %s -> %s", name, 0, class_path, "result");
    /* or: fn_log("[reg] %s not among the %d %s assets", name, 0, class_path); */
    return NULL;
}

/* 0x00c3b0 - Get UKismetSystemLibrary default object */
static UObject *get_kismet_system_library(void)
{
    return static_find_object("Default__KismetSystemLibrary");
}

/* 0x00c4c0 - Load asset (blocking) */
static UObject *load_asset_blocking(const char *path)
{
    fn_log("loading %s ...", path);
    /* Use LoadAsset_Blocking / LoadClassAsset_Blocking */
    return NULL;
}

/* 0x00c6a0 - Convert soft class path to loaded class */
static UObject *resolve_soft_class(const char *path)
{
    UObject *kismet = get_kismet_system_library();
    UObject *conv_func = find_function(kismet, "Conv_SoftClassPathToSoftClassRef");
    /* ... */
    return NULL;
}

/* 0x00d350 - Apply skin / character parts to pawn */
static void apply_character_parts(AFortPawn *pawn, UObject **parts, int count)
{
    /* Get player state */
    AFortPlayerState *ps = NULL; /* from pawn->PlayerState */
    if (!ps) {
        fn_log("[skin] no player state (pc=%p)", NULL);
        return;
    }

    fn_log("[skin] pawn %s", "FortPawn");

    /* Find character part application functions on FortPlayerState */
    UObject *apply_func = find_function((UObject*)ps, "ServerChoosePart");
    UObject *remove_func = find_function((UObject*)ps, "RemoveCharacterPart");

    if (!apply_func || !remove_func) {
        fn_log("[skin] character part functions not found");
        return;
    }

    /* For each part: find its slot, check if slot is occupied */
    for (int i = 0; i < count; i++) {
        /* If no existing part in slot, add empty placeholder first */
        fn_log("[skin]  part %d: %s", i, "PartName");
    }

    fn_log("[skin] applied %d/%d character parts%s", count, count, "");
}

/* 0x00dce0 - Search CID by display name */
static UObject *find_cid_by_display_name(const char *display_name)
{
    /* Use asset registry GetTagValue("DisplayName") */
    /* Search /Script/FortniteGame CharacterItemDefinition assets */
    /* Filter out Bean/Juno entries */
    fn_log("[name] DisplayName tags are stripped in this build - use the CID name instead");
    return NULL;
}

/* 0x00e940 - Scan for empty character parts */
static void scan_empty_character_parts(void)
{
    /* Look for CP_Head_F_EmptyHead, CP_M_MED_Hat_Empty, etc. */
    fn_log("[skin] scanned %d character parts, %d empty ones. per slot: head=%s hat=%s tail=%s face=%s",
            0, 0, "none", "none", "none", "none");
}

/* 0x010e30 - Set overlay material on mesh component */
static void set_overlay_material(void *mesh_component, UObject *material)
{
    UObject *get_num = find_function((UObject*)mesh_component, "GetNumMaterials");
    UObject *set_mat = find_function((UObject*)mesh_component, "SetOverlayMaterial");
    /* Apply material to all slots */
}

/* 0x010b10 - Get skeletal mesh component from pawn */
static void *get_pawn_mesh_component(AFortPawn *pawn, int slot_index)
{
    if (!pawn) {
        fn_log("[mesh] no pawn");
        return NULL;
    }
    /* Traverse CharacterParts array to find component by slot */
    return NULL;
}

/* 0x011010 - Line trace helper (for look/destroy) */
static bool line_trace_single(FVector *start, FVector *end, void **hit_result)
{
    UObject *kismet = get_kismet_system_library();
    /* Call LineTraceSingleForObjects */
    return false;
}

/* 0x012ca0 - Spawn bus model */
static void spawn_bus_model(UObject *bus_class, FVector *location)
{
    AActor *bus = spawn_actor(NULL, bus_class, location, NULL);
    if (!bus) {
        fn_log("bus: spawning the new model failed");
        return;
    }
}

/* 0x013480 - Set actor location */
static void set_actor_location(AActor *actor, FVector *location)
{
    UObject *func = find_function((UObject*)actor, "K2_SetActorLocation");
    if (func) {
        struct { FVector NewLocation; bool bSweep; bool bTeleport; } params;
        memcpy(&params.NewLocation, location, sizeof(FVector));
        params.bSweep = false;
        params.bTeleport = true;
        process_event((UObject*)actor, func, &params);
    }
}

/* 0x012500 / 0x012770 - Vehicle fly tick */
static void vtp_fly_tick(void)
{
    if (!g_vtp_active) return;

    /* Get current vehicle position and marker position */
    /* Calculate direction, move vehicle at g_vtp_speed */
    /* If reached destination, stop */
    fn_log("vtp: %s", "arrived");
}

/* 0x012fa0 - Tpall tick (pin all pawns) */
static void tpall_tick(void)
{
    if (!g_tpall_active) return;

    AFortPawn *local = get_local_pawn();
    if (!local) return;

    /* Calculate position in front of local pawn at g_tpall_distance */
    FVector target;

    /* For each enemy pawn: set location to target */
    /* Uses K2_SetActorLocation */
}


/* ========================================================================= */
/*  PLUGIN COMMON (from ../../common/plugin_common.c)                         */
/* ========================================================================= */

/* 0x014100 - kern_write (already defined above) */
/* 0x0141a0 - alloc_exec_page (already defined above) */

/* 0x014a20 / 0x014a50-0x014b40 - GoldHEN API helpers */
/* These wrap GoldHEN kernel read/write/notify primitives */

/* 0x014b40 / 0x014bc0 - Kernel read/write with notification */
static int kern_read(void *dst, void *src, size_t len)
{
    /* Read kernel memory via GoldHEN primitives */
    return 0;
}

/* 0x014c20 - Memory search/pattern scan */
static void *pattern_scan(void *base, size_t size, const uint8_t *pattern,
                           const uint8_t *mask, size_t pattern_len)
{
    /* Boyer-Moore-Horspool or simple byte scan */
    return NULL;
}

/* 0x0158d0 / 0x015900 - Dynamic library function resolution */
static void *dlsym_resolve(int handle, const char *symbol)
{
    /* sceKernelDlsym equivalent */
    return NULL;
}


/* ========================================================================= */
/*  EH_FRAME DATA (exception handling - compiler generated)                   */
/* ========================================================================= */
/* vaddr 0x3905c-0x39718: GNU_EH_FRAME / .eh_frame_hdr
   This is compiler-generated exception handling metadata.
   Not reconstructible to source code. */


/* ========================================================================= */
/*  FORTNITE GAME CONTENT STRINGS (rodata, vaddr 0x3c000+)                   */
/* ========================================================================= */
/* The binary contains ~600 Fortnite asset path strings used for:
 * - Creative island IDs and names
 * - STW content paths (Figment, Husk, Encounters, etc.)
 * - Juno/Tabasco season content references
 * - Weapon/vehicle/cosmetic asset paths
 * - UI widget paths
 * - Animation/sequence references
 * These are used by the fn.loadlevel, fn.mount, fn.spawn, fn.seq commands
 * to find and load game content by name.
 *
 * Example asset paths found in the binary:
 *   /Game/Athena/Aircraft/Skins/...
 *   /Game/Athena/Items/Weapons/...
 *   Juno/JunoTabasco... (Season content)
 *   Creative/CRP/... (Creative islands)
 *   Figment/Figment_... (STW Figment content)
 *   FNE/... (Fortnite Event content)
 *   STW/... (Save the World content)
 */


/* ========================================================================= */
/*  END OF RECONSTRUCTED SOURCE                                               */
/* ========================================================================= */
/*
 * RIEPILOGO (Summary):
 *
 * Questo file ricostruisce la struttura completa di sploit.sprx:
 *
 * FUNZIONI PRINCIPALI:
 *   module_start()    - Entry point, risolve offset, installa hook
 *   module_stop()     - Cleanup, ripristina vtable originale
 *   cmd_handler()     - Dispatcher principale comandi (fn.*)
 *   draw_hud_hook()   - Hook DrawHUD per ESP/menu/crosshair
 *   hooked_process_event() - Hook ProcessEvent per cheat runtime
 *   hooked_input_key()     - Hook input per menu e console
 *
 * FEATURES (30+ comandi):
 *   Aimbot:     Normal + Silent, FOV, smooth, bone select, LOS, prediction
 *   ESP:        Box, Skeleton, Container, Loot, Snaplines, Arrows, Labels
 *   Visual:     Chams (RGB), Crosshair, Damage numbers, Hit marker, BigHead
 *   Combat:     Infinite ammo, No recoil, No overheat, Fire rate mod
 *   Movement:   Auth (freeze pos), Vehicle desync, VTP fly, VTP teleport
 *   World:      LoadLevel, Mount plugins, Spawn actors, Destroy, Look
 *   Cosmetic:   Skin changer (CID), Bus changer (BBID), Mesh replacement
 *   Media:      Level sequence player, Time of day control
 *   System:     Trophy unlocker, PS4 keyboard, GFX presets, FPS cap
 *   Debug:      menustatus, vstatus, authstatus, keylog, gfstate
 *
 * BINARY LAYOUT:
 *   0x000000-0x015930: 176 functions (~86 KB of executable code)
 *   0x015930-0x015bd0: PLT stubs (41 imported functions)
 *   0x015bd0-0x038000: Rodata (strings, constants, tables)
 *   0x038000-0x03c000: Exception handling data
 *   0x03c000-0x044000: Additional rodata + GOT
 *   0x044000-0x080000: Data segment (global variables, UE4 offsets)
 *   0x084000-0x088700: SCE metadata (dynamic linking info)
 */
