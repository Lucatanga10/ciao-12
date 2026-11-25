#include "selftest.h"

#include "protocol.h"
#include "vecmath.h"

#include <math.h>
#include <stdio.h>
#include <string.h>

static int g_failures;

static void check(const char *label, int condition)
{
    if (condition) {
        printf("[pass] %s\n", label);
    } else {
        printf("[FAIL] %s\n", label);
        g_failures += 1;
    }
}

static int approx(double a, double b, double tolerance)
{
    return fabs(a - b) <= tolerance;
}

static void test_basis_identity(void)
{
    rot3 zero;
    basis3 basis;

    zero.pitch = 0.0;
    zero.yaw = 0.0;
    zero.roll = 0.0;

    basis = rot_to_basis(zero);

    check("basis zero: forward = (1,0,0)",
          approx(basis.forward.x, 1.0, 1e-9) &&
          approx(basis.forward.y, 0.0, 1e-9) &&
          approx(basis.forward.z, 0.0, 1e-9));

    check("basis zero: right = (0,1,0)",
          approx(basis.right.x, 0.0, 1e-9) &&
          approx(basis.right.y, 1.0, 1e-9) &&
          approx(basis.right.z, 0.0, 1e-9));

    check("basis zero: up = (0,0,1)",
          approx(basis.up.x, 0.0, 1e-9) &&
          approx(basis.up.y, 0.0, 1e-9) &&
          approx(basis.up.z, 1.0, 1e-9));
}

static void test_basis_yaw(void)
{
    rot3 yaw90;
    basis3 basis;

    yaw90.pitch = 0.0;
    yaw90.yaw = 90.0;
    yaw90.roll = 0.0;

    basis = rot_to_basis(yaw90);

    check("basis yaw 90: forward = (0,1,0)",
          approx(basis.forward.x, 0.0, 1e-9) &&
          approx(basis.forward.y, 1.0, 1e-9) &&
          approx(basis.forward.z, 0.0, 1e-9));

    check("basis yaw 90: right = (-1,0,0)",
          approx(basis.right.x, -1.0, 1e-9) &&
          approx(basis.right.y, 0.0, 1e-9) &&
          approx(basis.right.z, 0.0, 1e-9));
}

static void test_basis_pitch(void)
{
    rot3 pitch45;
    basis3 basis;
    const double half = sqrt(0.5);

    pitch45.pitch = 45.0;
    pitch45.yaw = 0.0;
    pitch45.roll = 0.0;

    basis = rot_to_basis(pitch45);

    check("basis pitch 45: forward sale di 45 gradi",
          approx(basis.forward.x, half, 1e-9) &&
          approx(basis.forward.y, 0.0, 1e-9) &&
          approx(basis.forward.z, half, 1e-9));
}

static void test_w2s_center(void)
{
    rot3 zero;
    basis3 basis;
    vec2 screen;
    int ok;

    zero.pitch = 0.0;
    zero.yaw = 0.0;
    zero.roll = 0.0;
    basis = rot_to_basis(zero);

    ok = world_to_screen(vec3_make(100.0, 0.0, 0.0),
                         vec3_make(0.0, 0.0, 0.0),
                         basis, 90.0, 1920.0, 1080.0, &screen);

    check("w2s: punto davanti finisce al centro",
          ok && approx(screen.x, 960.0, 1e-6) && approx(screen.y, 540.0, 1e-6));
}

static void test_w2s_right_edge(void)
{
    rot3 zero;
    basis3 basis;
    vec2 screen;
    int ok;

    zero.pitch = 0.0;
    zero.yaw = 0.0;
    zero.roll = 0.0;
    basis = rot_to_basis(zero);

    /* A 90 gradi di FOV, uno spostamento laterale pari alla profondita'
     * cade esattamente sul bordo destro. */
    ok = world_to_screen(vec3_make(100.0, 100.0, 0.0),
                         vec3_make(0.0, 0.0, 0.0),
                         basis, 90.0, 1920.0, 1080.0, &screen);

    check("w2s: 45 gradi a destra finisce sul bordo",
          ok && approx(screen.x, 1920.0, 1e-6) && approx(screen.y, 540.0, 1e-6));
}

static void test_w2s_above(void)
{
    rot3 zero;
    basis3 basis;
    vec2 screen;
    int ok;

    zero.pitch = 0.0;
    zero.yaw = 0.0;
    zero.roll = 0.0;
    basis = rot_to_basis(zero);

    ok = world_to_screen(vec3_make(100.0, 0.0, 100.0),
                         vec3_make(0.0, 0.0, 0.0),
                         basis, 90.0, 1920.0, 1080.0, &screen);

    check("w2s: Z positivo va verso l'alto dello schermo",
          ok && approx(screen.x, 960.0, 1e-6) && screen.y < 540.0);
}

static void test_w2s_behind(void)
{
    rot3 zero;
    basis3 basis;
    vec2 screen;
    int ok;

    zero.pitch = 0.0;
    zero.yaw = 0.0;
    zero.roll = 0.0;
    basis = rot_to_basis(zero);

    screen.x = -1.0;
    screen.y = -1.0;

    ok = world_to_screen(vec3_make(-100.0, 0.0, 0.0),
                         vec3_make(0.0, 0.0, 0.0),
                         basis, 90.0, 1920.0, 1080.0, &screen);

    check("w2s: punto dietro la camera viene scartato", !ok);
    check("w2s: out non toccato quando scarta",
          approx(screen.x, -1.0, 1e-12) && approx(screen.y, -1.0, 1e-12));
}

static void test_w2s_bad_input(void)
{
    rot3 zero;
    basis3 basis;
    vec2 screen;

    zero.pitch = 0.0;
    zero.yaw = 0.0;
    zero.roll = 0.0;
    basis = rot_to_basis(zero);

    check("w2s: FOV a zero rifiutato",
          !world_to_screen(vec3_make(100.0, 0.0, 0.0),
                           vec3_make(0.0, 0.0, 0.0),
                           basis, 0.0, 1920.0, 1080.0, &screen));

    check("w2s: schermo a zero rifiutato",
          !world_to_screen(vec3_make(100.0, 0.0, 0.0),
                           vec3_make(0.0, 0.0, 0.0),
                           basis, 90.0, 0.0, 1080.0, &screen));
}

static void test_vec3_guards(void)
{
    check("vec3_plausible: coordinate assurde rifiutate",
          !vec3_plausible(vec3_make(1.0e9, 0.0, 0.0), 2.0e7));

    check("vec3_plausible: coordinate normali accettate",
          vec3_plausible(vec3_make(12345.0, -6789.0, 300.0), 2.0e7));
}

static void test_proto_single_box(void)
{
    esp_box box;
    uint8_t out[OJBS_MAX_FRAME];
    size_t written;
    size_t expected;

    memset(&box, 0, sizeof(box));
    box.left = 10.0f;
    box.top = 20.0f;
    box.right = 30.0f;
    box.bottom = 60.0f;
    box.argb = (int32_t)0xFFFF4848;
    box.health = 0.5f;
    snprintf(box.name, sizeof(box.name), "42m");

    written = proto_encode_frame(&box, 1, out, sizeof(out));
    expected = (size_t)OJBS_HEADER_SIZE + 2u + (size_t)OJBS_BOX_FIXED_SIZE + 3u;

    check("proto: lunghezza frame con un box", written == expected);

    check("proto: magic ESP1 in testa",
          written > 4 &&
          out[0] == 0x45 && out[1] == 0x53 && out[2] == 0x50 && out[3] == 0x31);

    check("proto: payload_len coerente",
          written > 8 &&
          ((uint32_t)out[4] | ((uint32_t)out[5] << 8) |
           ((uint32_t)out[6] << 16) | ((uint32_t)out[7] << 24))
          == (uint32_t)(written - OJBS_HEADER_SIZE));

    check("proto: contatore a 1",
          written > 10 && out[8] == 1 && out[9] == 0);
}

static void test_proto_empty(void)
{
    uint8_t out[OJBS_MAX_FRAME];
    size_t written = proto_encode_frame(NULL, 0, out, sizeof(out));

    check("proto: frame vuoto valido",
          written == (size_t)OJBS_HEADER_SIZE + 2u);
}

static void test_proto_rejects(void)
{
    esp_box box;
    uint8_t out[OJBS_MAX_FRAME];
    uint8_t tiny[4];

    memset(&box, 0, sizeof(box));
    box.left = 0.0f;
    box.top = 0.0f;
    box.right = (float)INFINITY;
    box.bottom = 10.0f;
    box.health = 1.0f;

    check("proto: float non finito rifiutato",
          proto_encode_frame(&box, 1, out, sizeof(out)) == 0);

    box.right = 10.0f;
    check("proto: capacita' insufficiente rifiutata",
          proto_encode_frame(&box, 1, tiny, sizeof(tiny)) == 0);
}

static void test_proto_hello(void)
{
    uint8_t hello[OJBS_HELLO_SIZE];
    uint32_t width = 0;
    uint32_t height = 0;

    hello[0] = 0x80;
    hello[1] = 0x07;
    hello[2] = 0x00;
    hello[3] = 0x00;
    hello[4] = 0x38;
    hello[5] = 0x04;
    hello[6] = 0x00;
    hello[7] = 0x00;

    check("proto: hello 1920x1080 decodificato",
          proto_decode_hello(hello, sizeof(hello), &width, &height) &&
          width == 1920u && height == 1080u);

    check("proto: hello di lunghezza sbagliata rifiutato",
          !proto_decode_hello(hello, 4, &width, &height));
}

int selftest_run(void)
{
    g_failures = 0;

    printf("-- vecmath --\n");
    test_basis_identity();
    test_basis_yaw();
    test_basis_pitch();
    test_w2s_center();
    test_w2s_right_edge();
    test_w2s_above();
    test_w2s_behind();
    test_w2s_bad_input();
    test_vec3_guards();

    printf("-- protocol --\n");
    test_proto_single_box();
    test_proto_empty();
    test_proto_rejects();
    test_proto_hello();

    printf("-- risultato: %d falliti --\n", g_failures);
    return g_failures;
}
