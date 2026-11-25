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
    chain_actor actors[OJBS_MAX_ACTORS];
    esp_box boxes[OJBS_MAX_BOXES];
    uint8_t frame[OJBS_MAX_FRAME];
    const char *module_name;
    const long budget = 1000000000L / options->fps;
    bool core_ready;
    bool self_filter_ready;
    char missing[512];
    pid_t pid;

    memset(&ctx, 0, sizeof(ctx));
    ctx.off = off;

    module_name = (options->module != NULL) ? options->module : off->module_name;

    core_ready = offsets_feature_ready(off, FEAT_ACTOR_WALK, missing, sizeof(missing));
    if (core_ready) {
        core_ready = offsets_feature_ready(off, FEAT_ACTOR_POSITION,
                                           missing, sizeof(missing));
    }
    if (core_ready) {
        core_ready = offsets_feature_ready(off, FEAT_LOCAL_CAMERA,
                                           missing, sizeof(missing));
    }

    self_filter_ready = offsets_feature_ready(off, FEAT_SELF_FILTER, NULL, 0);

    ipc_init(&link);
    if (!wait_for_link(&link, options->socket_name)) {
        ipc_close(&link);
        return 0;
    }

    if (!core_ready) {
        /* Niente offset, niente box: il link resta su e lo dice una volta,
         * cosi' l'HUD dell'APK mostra che la metteria funziona. */
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

    while (!g_stop) {
        const long started = now_ns();
        uint64_t world = 0;
        uint64_t controller = 0;
        uint64_t self_pawn = 0;
        chain_camera camera;
        int actor_count;
        int box_count = 0;
        size_t length;
        long spent;

        if (chain_resolve_world(&ctx, &world) &&
            chain_local_controller(&ctx, world, &controller) &&
            chain_read_camera(&ctx, controller, &camera)) {

            if (self_filter_ready) {
                chain_local_pawn(&ctx, controller, &self_pawn);
            }

            actor_count = chain_collect_actors(&ctx, world, self_pawn,
                                               camera.position,
                                               options->max_meters * 100.0,
                                               actors, OJBS_MAX_ACTORS);

            box_count = build_live_boxes(&ctx, &camera, actors, actor_count,
                                         link.screen_width, link.screen_height,
                                         boxes, OJBS_MAX_BOXES);
        }

        length = proto_encode_frame(boxes, (uint16_t)box_count, frame, sizeof(frame));
        if (length == 0) {
            length = proto_encode_frame(NULL, 0, frame, sizeof(frame));
        }

        if (length == 0 || !ipc_send(&link, frame, length)) {
            fprintf(stderr, "[ipc] link caduto, riprovo\n");
            if (!wait_for_link(&link, options->socket_name)) {
                break;
            }
        }

        /* Il processo puo' uscire sotto di noi: se il modulo sparisce, basta. */
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
