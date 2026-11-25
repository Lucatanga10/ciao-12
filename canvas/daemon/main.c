#define _GNU_SOURCE

#include "chain.h"
#include "ipc.h"
#include "memread.h"
#include "offsets.h"
#include "protocol.h"
#include "selftest.h"
#include "vecmath.h"

#include <errno.h>
#include <math.h>
#include <signal.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include <unistd.h>

#define OJBS_DEFAULT_PACKAGE "com.epicgames.fortnite"
#define OJBS_DEFAULT_FPS 60
#define OJBS_DEFAULT_MAX_METERS 300.0
#define OJBS_BOX_COLOR ((int32_t)0xFFFF4848)
#define OJBS_RECONNECT_SLEEP_NS 500000000L

static volatile sig_atomic_t g_stop;

static void on_signal(int signal_number)
{
    (void)signal_number;
    g_stop = 1;
}

typedef struct {
    const char *package;
    const char *module;
    const char *socket_name;
    int fps;
    double max_meters;
    bool selftest;
    bool demo;
} cli_options;

static void print_usage(const char *argv0)
{
    fprintf(stderr,
        "uso: %s [opzioni]\n"
        "  --selftest          verifica matematica e protocollo, poi esce\n"
        "  --demo              manda box sintetici, non legge nessun processo\n"
        "  --package <nome>    pacchetto target (default %s)\n"
        "  --module <nome>     modulo da cui parte GWorld (default dal config)\n"
        "  --socket <nome>     abstract socket dell'APK (default %s)\n"
        "  --fps <n>           frame al secondo (default %d)\n"
        "  --max-dist <m>      distanza massima in metri (default %.0f)\n"
        "  --help              questo testo\n",
        argv0, OJBS_DEFAULT_PACKAGE, OJBS_SOCKET_NAME,
        OJBS_DEFAULT_FPS, OJBS_DEFAULT_MAX_METERS);
}

static bool parse_args(int argc, char **argv, cli_options *options)
{
    int i;

    options->package = OJBS_DEFAULT_PACKAGE;
    options->module = NULL;
    options->socket_name = OJBS_SOCKET_NAME;
    options->fps = OJBS_DEFAULT_FPS;
    options->max_meters = OJBS_DEFAULT_MAX_METERS;
    options->selftest = false;
    options->demo = false;

    for (i = 1; i < argc; ++i) {
        const char *arg = argv[i];

        if (strcmp(arg, "--selftest") == 0) {
            options->selftest = true;
        } else if (strcmp(arg, "--demo") == 0) {
            options->demo = true;
        } else if (strcmp(arg, "--help") == 0) {
            return false;
        } else if (strcmp(arg, "--package") == 0 && i + 1 < argc) {
            options->package = argv[++i];
        } else if (strcmp(arg, "--module") == 0 && i + 1 < argc) {
            options->module = argv[++i];
        } else if (strcmp(arg, "--socket") == 0 && i + 1 < argc) {
            options->socket_name = argv[++i];
        } else if (strcmp(arg, "--fps") == 0 && i + 1 < argc) {
            options->fps = atoi(argv[++i]);
        } else if (strcmp(arg, "--max-dist") == 0 && i + 1 < argc) {
            options->max_meters = atof(argv[++i]);
        } else {
            fprintf(stderr, "[args] opzione sconosciuta: %s\n", arg);
            return false;
        }
    }

    if (options->fps < 1) {
        options->fps = 1;
    }
    if (options->fps > 240) {
        options->fps = 240;
    }
    if (!(options->max_meters > 0.0)) {
        options->max_meters = OJBS_DEFAULT_MAX_METERS;
    }

    return true;
}

static void sleep_ns(long nanoseconds)
{
    struct timespec request;

    if (nanoseconds <= 0) {
        return;
    }

    request.tv_sec = nanoseconds / 1000000000L;
    request.tv_nsec = nanoseconds % 1000000000L;

    while (nanosleep(&request, &request) != 0) {
        if (errno != EINTR || g_stop) {
            return;
        }
    }
}

static long now_ns(void)
{
    struct timespec ts;

    if (clock_gettime(CLOCK_MONOTONIC, &ts) != 0) {
        return 0;
    }
    return (long)(ts.tv_sec * 1000000000L + ts.tv_nsec);
}

/* Attende che l'APK stia ascoltando. Torna false solo se arriva un segnale. */
static bool wait_for_link(ipc_link *link, const char *socket_name)
{
    bool announced = false;

    while (!g_stop) {
        if (ipc_connect(link, socket_name)) {
            fprintf(stderr, "[ipc] collegato a @%s, superficie %ux%u\n",
                    socket_name, link->screen_width, link->screen_height);
            return true;
        }

        if (!announced) {
            fprintf(stderr, "[ipc] @%s non in ascolto, riprovo\n", socket_name);
            announced = true;
        }

        sleep_ns(OJBS_RECONNECT_SLEEP_NS);
    }

    return false;
}

static int build_demo_boxes(esp_box *out, int capacity,
                            uint32_t screen_width, uint32_t screen_height,
                            double tick)
{
    const int count = (capacity < 5) ? capacity : 5;
    const double width = (double)screen_width;
    const double height = (double)screen_height;
    int i;

    for (i = 0; i < count; ++i) {
        const double phase = tick + i * 1.7;
        const double swing = (sin(phase) + 1.0) * 0.5;
        const double bob = sin(phase * 1.9);
        const double depth = (cos(phase * 0.8) + 1.0) * 0.5;

        const double center_x = width * (0.18 + 0.64 * swing);
        const double center_y = height * (0.45 + 0.08 * bob);
        const double box_h = height * 0.26 * (0.7 + 0.3 * depth);
        const double box_w = box_h * 0.5;

        memset(&out[i], 0, sizeof(out[i]));
        out[i].left = (float)(center_x - box_w * 0.5);
        out[i].top = (float)(center_y - box_h * 0.5);
        out[i].right = (float)(center_x + box_w * 0.5);
        out[i].bottom = (float)(center_y + box_h * 0.5);
        out[i].argb = (i % 2 == 0) ? OJBS_BOX_COLOR : (int32_t)0xFF78AAFF;
        out[i].health = (float)((sin(phase * 0.6) + 1.0) * 0.5);
        snprintf(out[i].name, sizeof(out[i].name), "demo_%d", i);
    }

    return count;
}

static int build_live_boxes(const chain_ctx *ctx, const chain_camera *camera,
                            const chain_actor *actors, int actor_count,
                            uint32_t screen_width, uint32_t screen_height,
                            esp_box *out, int capacity)
{
    const basis3 basis = rot_to_basis(camera->rotation);
    const double pawn_height = ctx->off->pawn_height;
    int produced = 0;
    int i;

    for (i = 0; i < actor_count && produced < capacity; ++i) {
        vec3 feet = actors[i].position;
        vec3 head = vec3_add(feet, vec3_make(0.0, 0.0, pawn_height));
        vec2 feet_screen;
        vec2 head_screen;
        double box_h;
        double box_w;

        if (!world_to_screen(feet, camera->position, basis, camera->fov,
                             (double)screen_width, (double)screen_height,
                             &feet_screen)) {
            continue;
        }
        if (!world_to_screen(head, camera->position, basis, camera->fov,
                             (double)screen_width, (double)screen_height,
                             &head_screen)) {
            continue;
        }

        box_h = feet_screen.y - head_screen.y;
        if (!(box_h > 1.0)) {
            continue;
        }
        box_w = box_h * 0.5;

        memset(&out[produced], 0, sizeof(out[produced]));
        out[produced].left = (float)(feet_screen.x - box_w * 0.5);
        out[produced].top = (float)head_screen.y;
        out[produced].right = (float)(feet_screen.x + box_w * 0.5);
        out[produced].bottom = (float)feet_screen.y;
        out[produced].argb = OJBS_BOX_COLOR;
        out[produced].health = -1.0f;
        snprintf(out[produced].name, sizeof(out[produced].name),
                 "%.0fm", actors[i].distance / 100.0);

        produced += 1;
    }

    return produced;
}

static int run_demo(const cli_options *options)
{
    ipc_link link;
    esp_box boxes[8];
    uint8_t frame[OJBS_MAX_FRAME];
    const long budget = 1000000000L / options->fps;
    double tick = 0.0;

    ipc_init(&link);

    if (!wait_for_link(&link, options->socket_name)) {
        ipc_close(&link);
        return 0;
    }

    fprintf(stderr, "[demo] box sintetici, nessun processo letto\n");

    while (!g_stop) {
        const long started = now_ns();
        int count;
        size_t length;
        long spent;

        count = build_demo_boxes(boxes, (int)(sizeof(boxes) / sizeof(boxes[0])),
                                 link.screen_width, link.screen_height, tick);

        length = proto_encode_frame(boxes, (uint16_t)count, frame, sizeof(frame));
        if (length == 0) {
            fprintf(stderr, "[demo] encode fallito, mi fermo\n");
            break;
        }

        if (!ipc_send(&link, frame, length)) {
            fprintf(stderr, "[ipc] link caduto, riprovo\n");
            if (!wait_for_link(&link, options->socket_name)) {
                break;
            }
        }

        tick += 0.03;

        spent = now_ns() - started;
        sleep_ns(budget - spent);
    }

    ipc_close(&link);
    return 0;
}

static int run_live(const cli_options *options, const ue_offsets *off)
{
    chain_ctx ctx;
    ipc_link link;
    ue_offsets mutable_off;
    chain_actor actors[OJBS_MAX_ACTORS];
    esp_box boxes[OJBS_MAX_BOXES];
    uint8_t frame[OJBS_MAX_FRAME];
    const char *module_name;
    const long budget = 1000000000L / options->fps;
    bool camera_ready;
    bool walk_ready;
    bool self_filter_ready;
    char missing[512];
    pid_t pid;

    bool position_locked = false;
    bool levels_found = false;
    bool camera_scanned = false;
    bool world_dumped = false;
    uint32_t levels_offset = 0;
    int scan_cooldown = 0;
    int levels_cooldown = 0;
    int diag_tick = 0;
    int diag_count = 0;
    const int diag_limit = 60;
    uint64_t last_world = 0;

    memcpy(&mutable_off, off, sizeof(mutable_off));
    memset(&ctx, 0, sizeof(ctx));
    ctx.off = &mutable_off;

    module_name = (options->module != NULL) ? options->module : mutable_off.module_name;

    walk_ready = offsets_feature_ready(&mutable_off, FEAT_ACTOR_WALK,
                                       missing, sizeof(missing));
    camera_ready = walk_ready &&
                   offsets_feature_ready(&mutable_off, FEAT_LOCAL_CAMERA,
                                         missing, sizeof(missing));
    self_filter_ready = offsets_feature_ready(&mutable_off, FEAT_SELF_FILTER,
                                              NULL, 0);

    ipc_init(&link);
    if (!wait_for_link(&link, options->socket_name)) {
        ipc_close(&link);
        return 0;
    }

    if (!walk_ready || !camera_ready) {
        fprintf(stderr, "[gate] nessun box: mancano %s\n", missing);
        fprintf(stderr, "[gate] mando frame vuoti; --demo per provare il disegno\n");

        while (!g_stop) {
            size_t length = proto_encode_frame(NULL, 0, frame, sizeof(frame));
            if (length == 0 || !ipc_send(&link, frame, length)) {
                if (!wait_for_link(&link, options->socket_name)) {
                    break;
                }
            }
            sleep_ns(500000000L);
        }

        ipc_close(&link);
        return 2;
    }

    pid = mem_find_pid(options->package);
    if (pid <= 0) {
        fprintf(stderr, "[proc] pacchetto %s non in esecuzione\n", options->package);
        ipc_close(&link);
        return 3;
    }

    if (!mem_find_module(pid, module_name, &ctx.target)) {
        fprintf(stderr, "[proc] modulo %s non mappato nel pid %d\n",
                module_name, (int)pid);
        ipc_close(&link);
        return 4;
    }

    fprintf(stderr, "[proc] pid %d, %s base 0x%llx size 0x%llx\n",
            (int)pid, ctx.target.module_name,
            (unsigned long long)ctx.target.module_base,
            (unsigned long long)(ctx.target.module_end - ctx.target.module_base));

    fprintf(stderr, "[wait] 5 secondi — torna su Fortnite\n");
    sleep_ns(5000000000L);
    fprintf(stderr, "[wait] via!\n");

    while (!g_stop) {
        const long started = now_ns();
        uint64_t world = 0;
        uint64_t controller = 0;
        uint64_t self_pawn = 0;
        chain_camera camera;
        int actor_count = 0;
        int box_count = 0;
        size_t length;
        long spent;
        bool chain_ok = false;

        memset(&camera, 0, sizeof(camera));

        if (chain_resolve_world(&ctx, &world) &&
            chain_local_controller(&ctx, world, &controller) &&
            chain_read_camera(&ctx, controller, &camera)) {
            chain_ok = true;
        }

        if (chain_ok) {
            diag_tick++;

            /* GWorld change detection — re-dump on new world */
            if (world != last_world) {
                if (last_world != 0) {
                    fprintf(stderr,
                            "[watch] GWorld CAMBIATO: 0x%llx -> 0x%llx\n",
                            (unsigned long long)last_world,
                            (unsigned long long)world);
                    /* Reset everything for new world */
                    position_locked = false;
                    levels_found = false;
                    camera_scanned = false;
                    levels_offset = 0;
                    scan_cooldown = 0;
                    levels_cooldown = 0;
                }
                last_world = world;
                world_dumped = false;
            }

            /* --- One-time world structure dump --- */
            if (!world_dumped) {
                uint64_t persistent_level = 0;
                uint64_t pcm = 0;
                uint32_t doff;

                mem_read_ptr(&ctx.target,
                             world + mutable_off.uworld_persistent_level,
                             &persistent_level);
                mem_read_ptr(&ctx.target,
                             controller + mutable_off.apc_player_camera_manager,
                             &pcm);
                fprintf(stderr,
                        "[dump] world=0x%llx persistent_level=0x%llx "
                        "pcm=0x%llx\n",
                        (unsigned long long)world,
                        (unsigned long long)persistent_level,
                        (unsigned long long)pcm);

                fprintf(stderr, "[dump] --- TArray scan world+0x0..0x500 ---\n");
                for (doff = 0x0; doff <= 0x500; doff += 8) {
                    uint64_t d_data = 0;
                    int32_t d_num = 0, d_max = 0;
                    int d_check, d_valid, d_i;
                    bool d_has_pl;

                    if (!mem_read_ptr(&ctx.target, world + doff, &d_data)) {
                        continue;
                    }
                    if (!mem_read_i32(&ctx.target, world + doff + 8, &d_num)) {
                        continue;
                    }
                    if (!mem_read_i32(&ctx.target, world + doff + 12, &d_max)) {
                        continue;
                    }
                    if (d_num <= 0 || d_num > 50000 || d_max < d_num
                        || d_max > 100000) {
                        continue;
                    }

                    d_check = d_num < 30 ? d_num : 30;
                    d_valid = 0;
                    d_has_pl = false;
                    for (d_i = 0; d_i < d_check; ++d_i) {
                        uint64_t elem = 0;
                        if (!mem_read_ptr(&ctx.target,
                                          d_data + (uint64_t)d_i * 8,
                                          &elem)) {
                            continue;
                        }
                        if (elem == persistent_level) {
                            d_has_pl = true;
                        }
                        if (mem_looks_like_uobject(&ctx.target, elem)) {
                            d_valid++;
                        }
                    }

                    fprintf(stderr,
                            "[dump] +0x%03x: data=0x%llx num=%d max=%d "
                            "uobj=%d/%d%s\n",
                            doff, (unsigned long long)d_data,
                            d_num, d_max, d_valid, d_check,
                            d_has_pl ? " <-- HAS PL" : "");
                }

                mem_read_ptr(&ctx.target,
                             controller + mutable_off.apc_player_camera_manager,
                             &pcm);
                /* --- Match detector: pawn raw read --- */
                {
                    uint64_t pawn_raw = 0;
                    mem_read_u64(&ctx.target, controller + 0x80,
                                 &pawn_raw);
                    fprintf(stderr,
                            "[dump] pawn_raw(PC+0x80)=0x%llx %s\n",
                            (unsigned long long)pawn_raw,
                            pawn_raw == 0 ? "LOBBY" : "IN-MATCH");
                }

                /* --- UWorld UObject map (all ptrs) --- */
                fprintf(stderr,
                        "[dump] --- UWorld UObject ptrs ---\n");
                for (doff = 0x0; doff <= 0x500; doff += 8) {
                    uint64_t wo = 0;
                    if (!mem_read_ptr(&ctx.target, world + doff, &wo)) {
                        continue;
                    }
                    if (wo == 0) continue;
                    if (!mem_looks_like_uobject(&ctx.target, wo)) continue;
                    fprintf(stderr, "[dump] w+0x%03x=0x%llx",
                            doff, (unsigned long long)wo);
                    /* quick check for TArray of UObjects inside */
                    uint32_t gs_off;
                    int found_ta = 0;
                    for (gs_off = 0x28; gs_off <= 0x600 && found_ta < 3;
                         gs_off += 8) {
                        uint64_t ta_data = 0;
                        int32_t ta_num = 0, ta_max = 0;
                        int ta_valid = 0, ta_check, ta_i;
                        if (!mem_read_ptr(&ctx.target,
                                          wo + gs_off, &ta_data)) {
                            continue;
                        }
                        if (!mem_read_i32(&ctx.target,
                                          wo + gs_off + 8, &ta_num)) {
                            continue;
                        }
                        if (!mem_read_i32(&ctx.target,
                                          wo + gs_off + 12, &ta_max)) {
                            continue;
                        }
                        if (ta_num < 2 || ta_num > 200 || ta_max < ta_num
                            || ta_max > 1000) {
                            continue;
                        }
                        ta_check = ta_num < 20 ? ta_num : 20;
                        for (ta_i = 0; ta_i < ta_check; ++ta_i) {
                            uint64_t elem = 0;
                            if (!mem_read_ptr(&ctx.target,
                                              ta_data + (uint64_t)ta_i * 8,
                                              &elem)) {
                                continue;
                            }
                            if (mem_looks_like_uobject(&ctx.target, elem)) {
                                ta_valid++;
                            }
                        }
                        if (ta_valid >= 2) {
                            fprintf(stderr,
                                    " [+0x%03x:arr %d uobj=%d/%d]",
                                    gs_off, ta_num, ta_valid, ta_check);
                            found_ta++;
                        }
                    }
                    fprintf(stderr, "\n");
                }

                /* --- Actor-based GameState finder --- */
                fprintf(stderr,
                        "[dump] --- actor GameState scan ---\n");
                if (persistent_level != 0) {
                    uint64_t act_data = 0;
                    int32_t act_num = 0;
                    chain_read_tarray(&ctx, persistent_level + 0xe0,
                                      &act_data, &act_num);
                    int gs_found = 0;
                    int ai;
                    for (ai = 0; ai < act_num && ai < 280 && gs_found < 5;
                         ++ai) {
                        uint64_t actor = 0;
                        uint64_t raw = 0;
                        mem_read_u64(&ctx.target,
                                     act_data + (uint64_t)ai * 8, &raw);
                        if (raw == 0) continue;
                        /* skip encrypted handles: upper32==lower32 */
                        if ((raw >> 32) == (raw & 0xFFFFFFFF)
                            && raw < 0x100000000ULL) {
                            continue;
                        }
                        if (!mem_read_ptr(&ctx.target,
                                          act_data + (uint64_t)ai * 8,
                                          &actor)) {
                            continue;
                        }
                        if (actor == 0) continue;
                        /* scan actor for TArray of UObjects */
                        uint32_t gs_off;
                        for (gs_off = 0x28; gs_off <= 0x600;
                             gs_off += 8) {
                            uint64_t ta_data2 = 0;
                            int32_t ta_num2 = 0, ta_max2 = 0;
                            int ta_valid2 = 0, ta_check2, ta_i2;
                            if (!mem_read_ptr(&ctx.target,
                                              actor + gs_off, &ta_data2)) {
                                continue;
                            }
                            if (!mem_read_i32(&ctx.target,
                                              actor + gs_off + 8,
                                              &ta_num2)) {
                                continue;
                            }
                            if (!mem_read_i32(&ctx.target,
                                              actor + gs_off + 12,
                                              &ta_max2)) {
                                continue;
                            }
                            if (ta_num2 < 5 || ta_num2 > 200
                                || ta_max2 < ta_num2
                                || ta_max2 > 1000) {
                                continue;
                            }
                            ta_check2 = ta_num2 < 20 ? ta_num2 : 20;
                            for (ta_i2 = 0; ta_i2 < ta_check2; ++ta_i2) {
                                uint64_t elem = 0;
                                if (!mem_read_ptr(&ctx.target,
                                        ta_data2 + (uint64_t)ta_i2 * 8,
                                        &elem)) {
                                    continue;
                                }
                                if (mem_looks_like_uobject(&ctx.target,
                                                           elem)) {
                                    ta_valid2++;
                                }
                            }
                            if (ta_valid2 >= 3) {
                                fprintf(stderr,
                                        "[dump] actor[%d]=0x%llx "
                                        "+0x%03x: TArray num=%d "
                                        "uobj=%d/%d GAMESTATE?\n",
                                        ai, (unsigned long long)actor,
                                        gs_off, ta_num2,
                                        ta_valid2, ta_check2);
                                gs_found++;
                            }
                        }
                    }
                    if (gs_found == 0) {
                        fprintf(stderr,
                                "[dump] no GameState found in %d actors\n",
                                act_num < 280 ? act_num : 280);
                    }
                }

                /* --- PCM hex dump around FOV --- */
                fprintf(stderr,
                        "[dump] --- PCM hex 0x1560..0x15D0 ---\n");
                if (pcm != 0) {
                    uint8_t hbuf[0x70];
                    if (mem_read(&ctx.target, pcm + 0x1560,
                                 hbuf, sizeof(hbuf))) {
                        int hi;
                        for (hi = 0; hi < 0x70; hi += 16) {
                            fprintf(stderr, "[dump] +%04x:",
                                    0x1560 + hi);
                            int hj;
                            for (hj = 0; hj < 16 && hi + hj < 0x70;
                                 ++hj) {
                                fprintf(stderr, " %02x", hbuf[hi + hj]);
                            }
                            fprintf(stderr, "\n");
                        }
                    }
                }

                /* --- PC rotation scan --- */
                fprintf(stderr,
                        "[dump] --- PC rotation scan ---\n");
                {
                    uint8_t pc_buf[0x500];
                    if (mem_read(&ctx.target, controller,
                                 pc_buf, sizeof(pc_buf))) {
                        int pi;
                        for (pi = 0; pi + 12 <= 0x500; pi += 4) {
                            float rp, ry, rr;
                            memcpy(&rp, pc_buf + pi, 4);
                            memcpy(&ry, pc_buf + pi + 4, 4);
                            memcpy(&rr, pc_buf + pi + 8, 4);
                            if (!isfinite(rp) || !isfinite(ry)
                                || !isfinite(rr)) {
                                continue;
                            }
                            /* pitch: -90..90, yaw: -360..360 */
                            if (rp > -90.0f && rp < 90.0f
                                && rp != 0.0f
                                && ry > -360.0f && ry < 360.0f
                                && ry != 0.0f) {
                                fprintf(stderr,
                                        "[dump] PC+0x%03x rot? "
                                        "(%.2f,%.2f,%.2f)\n",
                                        pi, rp, ry, rr);
                            }
                        }
                    }
                }

                /* --- PCM float position scan --- */
                fprintf(stderr,
                        "[dump] --- PCM float scan ---\n");
                if (pcm != 0) {
                    uint8_t fbuf[0x3000];
                    if (mem_read(&ctx.target, pcm, fbuf, sizeof(fbuf))) {
                        int fi;
                        for (fi = 0; fi + 12 <= 0x3000; fi += 4) {
                            float fx, fy, fz;
                            double fmag;
                            memcpy(&fx, fbuf + fi, 4);
                            memcpy(&fy, fbuf + fi + 4, 4);
                            memcpy(&fz, fbuf + fi + 8, 4);
                            if (!isfinite(fx) || !isfinite(fy)
                                || !isfinite(fz)) {
                                continue;
                            }
                            if (fx == 0.0f && fy == 0.0f && fz == 0.0f) {
                                continue;
                            }
                            fmag = sqrt((double)fx*fx
                                      + (double)fy*fy
                                      + (double)fz*fz);
                            if (fmag > 1000.0 && fmag < 1e7) {
                                fprintf(stderr,
                                        "[dump] PCM+0x%04x flt "
                                        "(%.0f,%.0f,%.0f) mag=%.0f\n",
                                        fi, fx, fy, fz, fmag);
                            }
                        }
                        /* also try doubles at every 8 bytes */
                        for (fi = 0; fi + 24 <= 0x3000; fi += 8) {
                            double dx, dy, dz, dmag;
                            memcpy(&dx, fbuf + fi, 8);
                            memcpy(&dy, fbuf + fi + 8, 8);
                            memcpy(&dz, fbuf + fi + 16, 8);
                            if (!isfinite(dx) || !isfinite(dy)
                                || !isfinite(dz)) {
                                continue;
                            }
                            if (dx == 0.0 && dy == 0.0 && dz == 0.0) {
                                continue;
                            }
                            dmag = sqrt(dx*dx + dy*dy + dz*dz);
                            if (dmag > 1000.0 && dmag < 1e7) {
                                fprintf(stderr,
                                        "[dump] PCM+0x%04x dbl "
                                        "(%.0f,%.0f,%.0f) mag=%.0f\n",
                                        fi, dx, dy, dz, dmag);
                            }
                        }
                    }
                }

                /* --- Deep actor probe --- */
                fprintf(stderr,
                        "[dump] --- actor probe (first 10) ---\n");
                if (persistent_level != 0) {
                    uint64_t act_data = 0;
                    int32_t act_num = 0;
                    chain_read_tarray(&ctx, persistent_level + 0xe0,
                                      &act_data, &act_num);
                    fprintf(stderr, "[dump] level+0xe0 actors=%d\n",
                            act_num);

                    int ai;
                    for (ai = 0; ai < act_num && ai < 10; ++ai) {
                        uint64_t actor = 0;
                        if (!mem_read_ptr(&ctx.target,
                                          act_data + (uint64_t)ai * 8,
                                          &actor)) {
                            continue;
                        }
                        if (actor == 0) continue;

                        /* scan actor for RootComponent-like ptrs */
                        uint32_t aoff;
                        int rc_hits = 0;
                        fprintf(stderr,
                                "[dump] actor[%d]=0x%llx", ai,
                                (unsigned long long)actor);
                        for (aoff = 0x100; aoff <= 0x500 && rc_hits < 3;
                             aoff += 8) {
                            uint64_t maybe_rc = 0;
                            if (!mem_read_ptr(&ctx.target,
                                              actor + aoff, &maybe_rc)) {
                                continue;
                            }
                            if (maybe_rc == 0) continue;
                            if (!mem_looks_like_uobject(&ctx.target,
                                                        maybe_rc)) {
                                continue;
                            }
                            /* check if RC has position-like data */
                            double pos[3] = {0};
                            mem_read(&ctx.target, maybe_rc + 0x140,
                                     pos, sizeof(pos));
                            double mag = sqrt(pos[0]*pos[0]
                                            + pos[1]*pos[1]
                                            + pos[2]*pos[2]);
                            if (mag > 10.0 && mag < 1e8) {
                                fprintf(stderr,
                                        " RC+0x%x=0x%llx "
                                        "pos(%.0f,%.0f,%.0f)",
                                        aoff,
                                        (unsigned long long)maybe_rc,
                                        pos[0], pos[1], pos[2]);
                                rc_hits++;
                            }
                            /* try float position too */
                            float fpos[3] = {0};
                            mem_read(&ctx.target, maybe_rc + 0x140,
                                     fpos, sizeof(fpos));
                            double fmag = sqrt((double)fpos[0]*fpos[0]
                                             + (double)fpos[1]*fpos[1]
                                             + (double)fpos[2]*fpos[2]);
                            if (fmag > 10.0 && fmag < 1e8 && mag <= 10.0) {
                                fprintf(stderr,
                                        " RC+0x%x=0x%llx "
                                        "fpos(%.0f,%.0f,%.0f)",
                                        aoff,
                                        (unsigned long long)maybe_rc,
                                        fpos[0], fpos[1], fpos[2]);
                                rc_hits++;
                            }
                        }
                        fprintf(stderr, "%s\n",
                                rc_hits == 0 ? " no-RC" : "");
                    }
                }

                /* --- PCM data map (which regions have data) --- */
                fprintf(stderr,
                        "[dump] --- PCM=0x%llx data map ---\n",
                        (unsigned long long)pcm);
                if (pcm != 0) {
                    uint8_t chunk[0x100];
                    for (doff = 0x0; doff <= 0x3000; doff += 0x100) {
                        int nonzero = 0;
                        int ci;
                        if (!mem_read(&ctx.target, pcm + doff,
                                      chunk, sizeof(chunk))) {
                            continue;
                        }
                        for (ci = 0; ci < 0x100; ++ci) {
                            if (chunk[ci] != 0) nonzero++;
                        }
                        if (nonzero > 16) {
                            fprintf(stderr,
                                    "[dump] PCM+0x%04x..%04x: "
                                    "%d/256 non-zero",
                                    doff, doff + 0xFF, nonzero);
                            /* check for position doubles */
                            int di;
                            for (di = 0; di + 24 <= 0x100; di += 8) {
                                double d3[3];
                                double dm;
                                memcpy(d3, chunk + di, 24);
                                if (d3[0] == 0.0 && d3[1] == 0.0
                                    && d3[2] == 0.0) {
                                    continue;
                                }
                                dm = sqrt(d3[0]*d3[0]
                                        + d3[1]*d3[1]
                                        + d3[2]*d3[2]);
                                if (dm > 100.0 && dm < 1e8
                                    && isfinite(d3[0])
                                    && isfinite(d3[1])
                                    && isfinite(d3[2])) {
                                    fprintf(stderr,
                                            " d+0x%x(%.0f,%.0f,%.0f)",
                                            di, d3[0], d3[1], d3[2]);
                                    break;
                                }
                            }
                            /* check float FOV in chunk */
                            for (di = 0; di + 4 <= 0x100; di += 4) {
                                float fv;
                                memcpy(&fv, chunk + di, 4);
                                if (fv > 50.0f && fv < 140.0f) {
                                    fprintf(stderr,
                                            " f+0x%x=%.1f",
                                            di, fv);
                                    break;
                                }
                            }
                            fprintf(stderr, "\n");
                        }
                    }
                }

                /* --- Scan A: GameInstance world scan --- */
                fprintf(stderr,
                        "[dump] --- GameInstance world scan ---\n");
                {
                    uint64_t gi = 0;
                    mem_read_ptr(&ctx.target,
                                 world + mutable_off.uworld_owning_game_instance,
                                 &gi);
                    fprintf(stderr, "[dump] GI=0x%llx\n",
                            (unsigned long long)gi);
                    if (gi != 0) {
                        uint32_t gi_off;
                        for (gi_off = 0x0; gi_off <= 0x800; gi_off += 8) {
                            uint64_t cand = 0;
                            uint64_t cand_level = 0;
                            uint64_t cand_act_data = 0;
                            int32_t cand_act_num = 0;

                            if (!mem_read_ptr(&ctx.target, gi + gi_off,
                                              &cand)) {
                                continue;
                            }
                            if (cand == 0 || cand == world) continue;
                            if (!mem_looks_like_uobject(&ctx.target, cand)) {
                                continue;
                            }

                            /* Candidato: check PersistentLevel pattern */
                            if (!mem_read_ptr(&ctx.target, cand + 0x20,
                                              &cand_level)) {
                                continue;
                            }
                            if (cand_level == 0) continue;
                            if (!mem_looks_like_uobject(&ctx.target,
                                                        cand_level)) {
                                continue;
                            }

                            /* Actors nel level */
                            if (!mem_read_ptr(&ctx.target,
                                              cand_level + 0xe0,
                                              &cand_act_data)) {
                                continue;
                            }
                            if (!mem_read_i32(&ctx.target,
                                              cand_level + 0xe0 + 8,
                                              &cand_act_num)) {
                                continue;
                            }
                            if (cand_act_num <= 0 || cand_act_num > 100000) {
                                continue;
                            }

                            fprintf(stderr,
                                    "[dump] GI+0x%03x -> world=0x%llx "
                                    "level=0x%llx actors=%d\n",
                                    gi_off, (unsigned long long)cand,
                                    (unsigned long long)cand_level,
                                    cand_act_num);
                        }
                    }
                }

                /* --- Scan B: GWorld RVA vicinity (+-0x2000) --- */
                fprintf(stderr,
                        "[dump] --- GWorld RVA vicinity scan ---\n");
                {
                    uint8_t rva_buf[0x4000];
                    uint64_t rva_base = ctx.target.module_base
                                      + mutable_off.gworld_rva - 0x2000;
                    if (mem_read(&ctx.target, rva_base,
                                 rva_buf, sizeof(rva_buf))) {
                        int ri;
                        for (ri = 0; ri < 0x4000; ri += 8) {
                            uint64_t cand = 0;
                            uint64_t cand_level = 0;
                            uint64_t cand_act_data = 0;
                            int32_t cand_act_num = 0;

                            if (ri == 0x2000) continue; /* skip GWorld */

                            memcpy(&cand, rva_buf + ri, 8);
                            if (cand == 0 || cand == world) continue;
                            if (!mem_ptr_plausible(cand)) continue;
                            if (!mem_looks_like_uobject(&ctx.target, cand)) {
                                continue;
                            }

                            if (!mem_read_ptr(&ctx.target, cand + 0x20,
                                              &cand_level)) {
                                continue;
                            }
                            if (cand_level == 0) continue;
                            if (!mem_looks_like_uobject(&ctx.target,
                                                        cand_level)) {
                                continue;
                            }
                            if (!mem_read_ptr(&ctx.target,
                                              cand_level + 0xe0,
                                              &cand_act_data)) {
                                continue;
                            }
                            if (!mem_read_i32(&ctx.target,
                                              cand_level + 0xe0 + 8,
                                              &cand_act_num)) {
                                continue;
                            }
                            if (cand_act_num <= 0 || cand_act_num > 100000) {
                                continue;
                            }

                            fprintf(stderr,
                                    "[dump] RVA%+d (0x%llx) -> "
                                    "world=0x%llx level=0x%llx actors=%d\n",
                                    ri - 0x2000,
                                    (unsigned long long)(rva_base + ri),
                                    (unsigned long long)cand,
                                    (unsigned long long)cand_level,
                                    cand_act_num);
                        }
                    } else {
                        fprintf(stderr,
                                "[dump] RVA bulk read failed\n");
                    }
                }

                /* --- Scan C: check se GWorld cambia tra i frame --- */
                fprintf(stderr,
                        "[dump] --- GWorld value=0x%llx ---\n",
                        (unsigned long long)world);

                /* --- Scan D: PlayerArray position scan --- */
                fprintf(stderr,
                        "[dump] --- PlayerArray pos scan "
                        "(w+0x270 -> +0x138) ---\n");
                {
                    uint64_t gs_obj = 0;
                    mem_read_ptr(&ctx.target, world + 0x270, &gs_obj);
                    fprintf(stderr, "[dump] GS=0x%llx\n",
                            (unsigned long long)gs_obj);
                    if (gs_obj != 0) {
                        uint64_t pa_data = 0;
                        int32_t pa_num = 0;
                        chain_read_tarray(&ctx, gs_obj + 0x138,
                                          &pa_data, &pa_num);
                        fprintf(stderr, "[dump] PA: data=0x%llx num=%d\n",
                                (unsigned long long)pa_data, pa_num);
                        int pi;
                        int scan_lim = pa_num < 20 ? pa_num : 20;
                        for (pi = 0; pi < scan_lim; ++pi) {
                            uint64_t ps = 0;
                            if (!mem_read_ptr(&ctx.target,
                                              pa_data + (uint64_t)pi * 8,
                                              &ps)) {
                                continue;
                            }
                            if (ps == 0) continue;
                            /* check if UObject */
                            bool is_uobj = mem_looks_like_uobject(
                                               &ctx.target, ps);
                            if (!is_uobj) {
                                /* might be encrypted handle */
                                if (pi < 5) {
                                    uint64_t raw = 0;
                                    mem_read_u64(&ctx.target,
                                                 pa_data + (uint64_t)pi * 8,
                                                 &raw);
                                    fprintf(stderr,
                                            "[dump] PA[%d] handle "
                                            "0x%llx\n",
                                            pi,
                                            (unsigned long long)raw);
                                }
                                continue;
                            }
                            /* bulk read 0x800 bytes */
                            uint8_t ps_buf[0x800];
                            if (!mem_read(&ctx.target, ps,
                                          ps_buf, sizeof(ps_buf))) {
                                continue;
                            }
                            int fhit = 0;
                            int si;
                            /* float triplets */
                            for (si = 0; si + 12 <= 0x800
                                 && fhit < 3; si += 4) {
                                float fx, fy, fz;
                                memcpy(&fx, ps_buf + si, 4);
                                memcpy(&fy, ps_buf + si + 4, 4);
                                memcpy(&fz, ps_buf + si + 8, 4);
                                if (!isfinite(fx) || !isfinite(fy)
                                    || !isfinite(fz)) {
                                    continue;
                                }
                                int nz = (fx != 0.0f) + (fy != 0.0f)
                                       + (fz != 0.0f);
                                if (nz < 2) continue;
                                double mag = sqrt((double)fx*fx
                                                + (double)fy*fy
                                                + (double)fz*fz);
                                if (mag > 1000.0 && mag < 1e7) {
                                    fprintf(stderr,
                                            "[dump] PA[%d]+0x%03x flt "
                                            "(%.1f,%.1f,%.1f) m=%.0f\n",
                                            pi, si, fx, fy, fz, mag);
                                    fhit++;
                                }
                            }
                            /* double triplets */
                            for (si = 0; si + 24 <= 0x800
                                 && fhit < 5; si += 8) {
                                double dx, dy, dz;
                                memcpy(&dx, ps_buf + si, 8);
                                memcpy(&dy, ps_buf + si + 8, 8);
                                memcpy(&dz, ps_buf + si + 16, 8);
                                if (!isfinite(dx) || !isfinite(dy)
                                    || !isfinite(dz)) {
                                    continue;
                                }
                                int nz = (dx != 0.0) + (dy != 0.0)
                                       + (dz != 0.0);
                                if (nz < 2) continue;
                                double mag = sqrt(dx*dx + dy*dy
                                                + dz*dz);
                                if (mag > 1000.0 && mag < 1e7) {
                                    fprintf(stderr,
                                            "[dump] PA[%d]+0x%03x dbl "
                                            "(%.1f,%.1f,%.1f) m=%.0f\n",
                                            pi, si, dx, dy, dz, mag);
                                    fhit++;
                                }
                            }
                            if (fhit == 0 && pi < 5) {
                                fprintf(stderr,
                                        "[dump] PA[%d]=0x%llx "
                                        "no-pos\n",
                                        pi,
                                        (unsigned long long)ps);
                            }
                        }
                    }
                }

                /* --- Scan E: deep RC double map --- */
                fprintf(stderr,
                        "[dump] --- deep RC probe "
                        "(w+0x2f0 -> +0x140) ---\n");
                {
                    uint64_t ml_obj = 0;
                    mem_read_ptr(&ctx.target, world + 0x2f0, &ml_obj);
                    if (ml_obj != 0) {
                        uint64_t ma_data = 0;
                        int32_t ma_num = 0;
                        chain_read_tarray(&ctx, ml_obj + 0x140,
                                          &ma_data, &ma_num);
                        fprintf(stderr,
                                "[dump] ML=0x%llx MA num=%d\n",
                                (unsigned long long)ml_obj, ma_num);
                        int rc_found = 0;
                        int mi2;
                        int ma_lim = ma_num < 60 ? ma_num : 60;
                        for (mi2 = 0; mi2 < ma_lim
                             && rc_found < 5; ++mi2) {
                            uint64_t actor = 0;
                            if (!mem_read_ptr(&ctx.target,
                                    ma_data + (uint64_t)mi2 * 8,
                                    &actor)) {
                                continue;
                            }
                            if (actor == 0) continue;
                            if (!mem_looks_like_uobject(&ctx.target,
                                                        actor)) {
                                continue;
                            }
                            /* read actor to find RC ptrs */
                            uint8_t ab[0x300];
                            if (!mem_read(&ctx.target, actor,
                                          ab, sizeof(ab))) {
                                continue;
                            }
                            /* find first UObject ptr in
                               +0x100..+0x280 = RC candidate */
                            uint32_t roff;
                            uint64_t rc = 0;
                            for (roff = 0x100; roff <= 0x280;
                                 roff += 8) {
                                uint64_t cand = 0;
                                memcpy(&cand, ab + roff, 8);
                                if (cand == 0) continue;
                                if (!mem_ptr_plausible(cand)) continue;
                                if (!mem_looks_like_uobject(
                                         &ctx.target, cand)) {
                                    continue;
                                }
                                rc = cand;
                                break;
                            }
                            if (rc == 0) continue;
                            rc_found++;
                            fprintf(stderr,
                                    "[dump] MA[%d]=0x%llx "
                                    "RC+0x%03x=0x%llx\n",
                                    mi2,
                                    (unsigned long long)actor,
                                    roff,
                                    (unsigned long long)rc);
                            /* read full RC 0x600 bytes */
                            uint8_t rb[0x600];
                            if (!mem_read(&ctx.target, rc,
                                          rb, sizeof(rb))) {
                                fprintf(stderr,
                                        "[dump]  RC read fail\n");
                                continue;
                            }
                            /* dump INDIVIDUAL doubles
                               |val| 50..500000 */
                            int dhit = 0;
                            uint32_t di;
                            for (di = 0x28; di + 8 <= 0x600
                                 && dhit < 20; di += 8) {
                                double dv;
                                memcpy(&dv, rb + di, 8);
                                if (!isfinite(dv)) continue;
                                double av = dv < 0 ? -dv : dv;
                                if (av >= 50.0 && av <= 500000.0) {
                                    fprintf(stderr,
                                            "[dump]  RC+0x%03x "
                                            "d=%.2f\n",
                                            di, dv);
                                    dhit++;
                                }
                            }
                            /* dump INDIVIDUAL floats
                               |val| 50..500000 */
                            int fhit = 0;
                            for (di = 0x28; di + 4 <= 0x600
                                 && fhit < 20; di += 4) {
                                float fv;
                                memcpy(&fv, rb + di, 4);
                                if (!isfinite(fv)) continue;
                                double av = fv < 0 ? -fv : fv;
                                if (av >= 50.0 && av <= 500000.0) {
                                    fprintf(stderr,
                                            "[dump]  RC+0x%03x "
                                            "f=%.2f\n",
                                            di, fv);
                                    fhit++;
                                }
                            }
                            if (dhit == 0 && fhit == 0) {
                                fprintf(stderr,
                                        "[dump]  no coords\n");
                            }
                            /* also dump actor itself
                               for individual doubles */
                            uint8_t abig[0xC00];
                            if (mem_read(&ctx.target, actor,
                                         abig, sizeof(abig))) {
                                int ahit = 0;
                                for (di = 0x80; di + 8 <= 0xC00
                                     && ahit < 10; di += 8) {
                                    double dv;
                                    memcpy(&dv, abig + di, 8);
                                    if (!isfinite(dv)) continue;
                                    double av = dv < 0 ? -dv : dv;
                                    if (av >= 500.0
                                        && av <= 500000.0) {
                                        fprintf(stderr,
                                            "[dump]  ACT+0x%03x "
                                            "d=%.2f\n",
                                            di, dv);
                                        ahit++;
                                    }
                                }
                            }
                        }
                        if (rc_found == 0) {
                            fprintf(stderr,
                                    "[dump] no RC found in "
                                    "first %d actors\n", ma_lim);
                        }
                    }
                }

                /* --- Scan E2: LocalPlayer chain
                       for ViewProjectionMatrix --- */
                fprintf(stderr,
                        "[dump] --- LP ViewProjection scan ---\n");
                {
                    uint64_t gi2 = 0;
                    uint64_t lp_arr = 0;
                    uint64_t lp = 0;
                    mem_read_ptr(&ctx.target,
                        world + mutable_off.uworld_owning_game_instance,
                        &gi2);
                    if (gi2 != 0) {
                        mem_read_ptr(&ctx.target,
                            gi2 + mutable_off.ugameinstance_local_players,
                            &lp_arr);
                    }
                    if (lp_arr != 0) {
                        mem_read_ptr(&ctx.target, lp_arr, &lp);
                    }
                    fprintf(stderr, "[dump] LP=0x%llx\n",
                            (unsigned long long)lp);
                    if (lp != 0) {
                        /* scan LP for UObject ptrs
                           (ViewportClient) */
                        uint8_t lpb[0x300];
                        if (mem_read(&ctx.target, lp,
                                     lpb, sizeof(lpb))) {
                            fprintf(stderr,
                                    "[dump] LP UObj ptrs:\n");
                            uint32_t li;
                            for (li = 0x28; li + 8 <= 0x300;
                                 li += 8) {
                                uint64_t vp = 0;
                                memcpy(&vp, lpb + li, 8);
                                if (vp == 0) continue;
                                if (!mem_ptr_plausible(vp)) continue;
                                if (!mem_looks_like_uobject(
                                         &ctx.target, vp)) {
                                    continue;
                                }
                                fprintf(stderr,
                                        "[dump]  LP+0x%03x"
                                        "=0x%llx",
                                        li,
                                        (unsigned long long)vp);
                                /* check if this obj has a
                                   4x4 float matrix anywhere
                                   (16 floats, det != 0) */
                                uint8_t vb[0x800];
                                if (mem_read(&ctx.target, vp,
                                             vb, sizeof(vb))) {
                                    uint32_t mi3;
                                    for (mi3 = 0x40;
                                         mi3 + 64 <= 0x800;
                                         mi3 += 0x10) {
                                        float m[16];
                                        memcpy(m, vb + mi3, 64);
                                        bool ok = true;
                                        int mj;
                                        int nzm = 0;
                                        for (mj = 0; mj < 16;
                                             ++mj) {
                                            if (!isfinite(m[mj])) {
                                                ok = false;
                                                break;
                                            }
                                            if (m[mj] != 0.0f)
                                                nzm++;
                                        }
                                        if (!ok || nzm < 6) continue;
                                        /* check if looks like
                                           view matrix:
                                           row magnitudes ~1 */
                                        double r0 = sqrt(
                                            (double)m[0]*m[0]
                                            + (double)m[1]*m[1]
                                            + (double)m[2]*m[2]);
                                        double r1 = sqrt(
                                            (double)m[4]*m[4]
                                            + (double)m[5]*m[5]
                                            + (double)m[6]*m[6]);
                                        if (r0 > 0.1 && r0 < 10.0
                                            && r1 > 0.1
                                            && r1 < 10.0) {
                                            fprintf(stderr,
                                                " MAT+0x%03x "
                                                "r0=%.3f r1=%.3f "
                                                "[%.4f %.4f %.4f "
                                                "%.4f / %.4f %.4f "
                                                "%.4f %.4f]",
                                                mi3,
                                                r0, r1,
                                                m[0], m[1],
                                                m[2], m[3],
                                                m[4], m[5],
                                                m[6], m[7]);
                                            break;
                                        }
                                    }
                                }
                                fprintf(stderr, "\n");
                            }
                        }
                    }
                }

                /* --- Scan F: PC ControlRotation scan --- */
                fprintf(stderr,
                        "[dump] --- PC ControlRotation ---\n");
                {
                    uint64_t pc = 0;
                    uint64_t lp = 0;
                    uint64_t lp_arr = 0;
                    uint64_t gi_f = 0;
                    mem_read_ptr(&ctx.target,
                                 world + mutable_off.uworld_owning_game_instance,
                                 &gi_f);
                    if (gi_f != 0) {
                        mem_read_ptr(&ctx.target,
                                     gi_f + mutable_off.ugameinstance_local_players,
                                     &lp_arr);
                    }
                    if (lp_arr != 0) {
                        mem_read_ptr(&ctx.target, lp_arr, &lp);
                    }
                    if (lp != 0) {
                        mem_read_ptr(&ctx.target,
                                     lp + mutable_off.ulocalplayer_player_controller,
                                     &pc);
                    }
                    fprintf(stderr, "[dump] PC=0x%llx\n",
                            (unsigned long long)pc);
                    if (pc != 0) {
                        uint8_t pc_buf[0x600];
                        if (mem_read(&ctx.target, pc,
                                     pc_buf, sizeof(pc_buf))) {
                            /* scan for FRotator (3 doubles)
                               with plausible angle range
                               -PI..PI or -180..180 */
                            int ci;
                            fprintf(stderr,
                                    "[dump] PC angle scan "
                                    "(doubles):\n");
                            for (ci = 0; ci + 24 <= 0x600;
                                 ci += 8) {
                                double a[3];
                                memcpy(a, pc_buf + ci, 24);
                                if (!isfinite(a[0])
                                    || !isfinite(a[1])
                                    || !isfinite(a[2])) {
                                    continue;
                                }
                                int nz = (a[0] != 0.0)
                                       + (a[1] != 0.0)
                                       + (a[2] != 0.0);
                                if (nz < 1) continue;
                                /* degrees: -360..360 */
                                bool deg = true;
                                int di;
                                for (di = 0; di < 3; ++di) {
                                    if (a[di] != 0.0
                                        && (a[di] < -360.0
                                            || a[di] > 360.0)) {
                                        deg = false;
                                    }
                                }
                                if (deg && nz >= 1) {
                                    fprintf(stderr,
                                            "[dump] PC+0x%03x "
                                            "deg? (%.2f,%.2f,%.2f)\n",
                                            ci, a[0], a[1], a[2]);
                                }
                            }
                            /* also float angles */
                            fprintf(stderr,
                                    "[dump] PC angle scan "
                                    "(floats):\n");
                            for (ci = 0; ci + 12 <= 0x600;
                                 ci += 4) {
                                float a[3];
                                memcpy(a, pc_buf + ci, 12);
                                if (!isfinite(a[0])
                                    || !isfinite(a[1])
                                    || !isfinite(a[2])) {
                                    continue;
                                }
                                int nz = (a[0] != 0.0f)
                                       + (a[1] != 0.0f)
                                       + (a[2] != 0.0f);
                                if (nz < 2) continue;
                                bool deg = true;
                                int di;
                                for (di = 0; di < 3; ++di) {
                                    if (a[di] != 0.0f
                                        && (a[di] < -360.0f
                                            || a[di] > 360.0f)) {
                                        deg = false;
                                    }
                                }
                                if (deg) {
                                    fprintf(stderr,
                                            "[dump] PC+0x%03x "
                                            "fdeg? "
                                            "(%.2f,%.2f,%.2f)\n",
                                            ci,
                                            a[0], a[1], a[2]);
                                }
                            }
                        }
                    }
                }

                fprintf(stderr, "[dump] --- fine ---\n");
                world_dumped = true;
            }

            /* --- Scan 1: UWorld::Levels TArray --- */
            if (!levels_found && levels_cooldown <= 0) {
                if (chain_scan_levels_offset(&ctx, world, &levels_offset)) {
                    uint64_t ld = 0;
                    int32_t lc = 0;
                    int li;
                    levels_found = true;
                    chain_read_tarray(&ctx, world + levels_offset, &ld, &lc);
                    fprintf(stderr,
                            "[scan] levels: world+0x%x, %d livelli\n",
                            levels_offset, lc);
                    for (li = 0; li < lc && li < 20; ++li) {
                        uint64_t lvl = 0;
                        uint64_t la_d = 0;
                        int32_t la_n = 0;
                        if (!mem_read_ptr(&ctx.target,
                                          ld + (uint64_t)li * 8, &lvl)) {
                            continue;
                        }
                        chain_read_tarray(&ctx,
                                          lvl + mutable_off.ulevel_actors,
                                          &la_d, &la_n);
                        fprintf(stderr,
                                "[scan]   [%d] 0x%llx actors=%d\n",
                                li, (unsigned long long)lvl, la_n);
                    }
                } else {
                    levels_cooldown = options->fps * 5;
                }
            }
            if (levels_cooldown > 0) {
                levels_cooldown--;
            }

            /* --- Scan 2: camera non-zero nel PCM --- */
            if (!camera_scanned &&
                camera.position.x == 0.0 &&
                camera.position.y == 0.0 &&
                camera.position.z == 0.0) {
                uint64_t pcm = 0;
                mem_read_ptr(&ctx.target,
                             controller + mutable_off.apc_player_camera_manager,
                             &pcm);
                if (pcm != 0) {
                    uint32_t cam_off = 0;
                    if (chain_scan_camera_offset(&ctx, pcm, &cam_off)) {
                        uint32_t old_cache = mutable_off.apcm_camera_cache_private;
                        mutable_off.apcm_camera_cache_private = cam_off;
                        mutable_off.apcm_pov_in_cache_entry = 0;
                        if (chain_read_camera(&ctx, controller, &camera)) {
                            fprintf(stderr,
                                    "[scan] camera: PCM+0x%x "
                                    "pos=(%.0f,%.0f,%.0f) fov=%.1f\n",
                                    cam_off,
                                    camera.position.x, camera.position.y,
                                    camera.position.z, camera.fov);
                        } else {
                            mutable_off.apcm_camera_cache_private = old_cache;
                            fprintf(stderr,
                                    "[scan] camera: PCM+0x%x trovato ma "
                                    "read fallita, ripristino 0x%x\n",
                                    cam_off, old_cache);
                        }
                    } else {
                        fprintf(stderr,
                                "[scan] camera: nessun dato non-zero nel PCM\n");
                    }
                    camera_scanned = true;
                }
            }

            /* --- Scan 3: RootComponent + RelativeLocation --- */
            if (!position_locked && scan_cooldown <= 0) {
                bool found_pos = false;

                if (levels_found) {
                    uint64_t ld = 0;
                    int32_t lc = 0;
                    int li;
                    chain_read_tarray(&ctx, world + levels_offset, &ld, &lc);
                    for (li = 0; li < lc && !found_pos; ++li) {
                        uint64_t lvl = 0;
                        uint32_t rc_off = 0, loc_off = 0;
                        bool is_float = false;
                        if (!mem_read_ptr(&ctx.target,
                                          ld + (uint64_t)li * 8, &lvl)) {
                            continue;
                        }
                        if (chain_autoscan_position_level(&ctx, lvl,
                                &rc_off, &loc_off, &is_float)) {
                            mutable_off.aactor_root_component = rc_off;
                            mutable_off.has_aactor_root_component = true;
                            mutable_off.uscene_relative_location = loc_off;
                            mutable_off.has_uscene_relative_location = true;
                            mutable_off.uscene_location_is_float = is_float;
                            position_locked = true;
                            found_pos = true;
                            fprintf(stderr,
                                    "[scan] posizione: levels[%d] "
                                    "rc=+0x%x loc=+0x%x %s\n",
                                    li, rc_off, loc_off,
                                    is_float ? "float" : "double");
                        }
                    }
                }

                if (!found_pos) {
                    uint32_t rc_off = 0, loc_off = 0;
                    bool is_float = false;
                    if (chain_autoscan_position(&ctx, world,
                            &rc_off, &loc_off, &is_float)) {
                        mutable_off.aactor_root_component = rc_off;
                        mutable_off.has_aactor_root_component = true;
                        mutable_off.uscene_relative_location = loc_off;
                        mutable_off.has_uscene_relative_location = true;
                        mutable_off.uscene_location_is_float = is_float;
                        position_locked = true;
                        fprintf(stderr,
                                "[scan] posizione (persistent): "
                                "rc=+0x%x loc=+0x%x %s\n",
                                rc_off, loc_off,
                                is_float ? "float" : "double");
                    } else {
                        scan_cooldown = options->fps * 2;
                    }
                }
            }
            if (scan_cooldown > 0) {
                scan_cooldown--;
            }

            /* --- Self filter --- */
            if (self_filter_ready) {
                chain_local_pawn(&ctx, controller, &self_pawn);
            }

            /* --- Raccolta attori --- */
            if (levels_found) {
                actor_count = chain_collect_actors_all_levels(
                    &ctx, world, levels_offset,
                    self_pawn, camera.position,
                    options->max_meters * 100.0,
                    actors, OJBS_MAX_ACTORS);
            } else {
                actor_count = chain_collect_actors(
                    &ctx, world, self_pawn,
                    camera.position,
                    options->max_meters * 100.0,
                    actors, OJBS_MAX_ACTORS);
            }

            /* --- Disegno ESP --- */
            box_count = build_live_boxes(&ctx, &camera, actors, actor_count,
                                         link.screen_width, link.screen_height,
                                         boxes, OJBS_MAX_BOXES);

            /* --- Diagnostica periodica --- */
            if (diag_count < diag_limit &&
                diag_tick % (options->fps * 3) == 1) {
                int32_t raw_actors = 0;
                uint64_t pawn_raw = 0;
                {
                    uint64_t pl = 0;
                    mem_read_ptr(&ctx.target,
                                 world + mutable_off.uworld_persistent_level,
                                 &pl);
                    if (pl != 0) {
                        mem_read_i32(&ctx.target, pl + 0xe0 + 8,
                                     &raw_actors);
                    }
                    mem_read_u64(&ctx.target, controller + 0x80,
                                 &pawn_raw);
                }
                fprintf(stderr,
                        "[diag] w=0x%llx pawn=0x%llx cam=(%.0f,%.0f,%.0f) "
                        "fov=%.1f raw=%d act=%d box=%d lv=%s pos=%s\n",
                        (unsigned long long)world,
                        (unsigned long long)pawn_raw,
                        camera.position.x, camera.position.y,
                        camera.position.z, camera.fov,
                        raw_actors, actor_count, box_count,
                        levels_found ? "si" : "no",
                        position_locked ? "si" : "scan");
                diag_count++;
            }
        } else {
            diag_tick++;
            if (diag_count < diag_limit &&
                diag_tick % (options->fps * 3) == 1) {
                fprintf(stderr, "[diag] catena fallita, riprovo\n");
                diag_count++;
            }
        }

        length = proto_encode_frame(boxes, (uint16_t)box_count,
                                     frame, sizeof(frame));
        if (length == 0) {
            length = proto_encode_frame(NULL, 0, frame, sizeof(frame));
        }

        if (length == 0 || !ipc_send(&link, frame, length)) {
            fprintf(stderr, "[ipc] link caduto, riprovo\n");
            if (!wait_for_link(&link, options->socket_name)) {
                break;
            }
        }

        if (kill(pid, 0) != 0 && errno == ESRCH) {
            fprintf(stderr, "[proc] il target e' uscito\n");
            break;
        }

        spent = now_ns() - started;
        sleep_ns(budget - spent);
    }

    ipc_close(&link);
    return 0;
}

int main(int argc, char **argv)
{
    cli_options options;
    const ue_offsets *off;

    if (!parse_args(argc, argv, &options)) {
        print_usage(argv[0]);
        return 1;
    }

    if (options.selftest) {
        return (selftest_run() == 0) ? 0 : 1;
    }

    signal(SIGINT, on_signal);
    signal(SIGTERM, on_signal);
    signal(SIGPIPE, SIG_IGN);

    off = offsets_default();
    offsets_report(off);

    if (options.demo) {
        return run_demo(&options);
    }

    return run_live(&options, off);
}
