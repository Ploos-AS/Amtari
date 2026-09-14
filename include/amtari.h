#ifndef AMTARI_H
#define AMTARI_H

#define AMTARI_VERSION "0.0.0-m0"

enum amtari_mode {
    AMTARI_MODE_NATIVE = 0,
    AMTARI_MODE_HYBRID,
    AMTARI_MODE_FULL
};

enum amtari_machine {
    AMTARI_MACHINE_ST = 0,
    AMTARI_MACHINE_STE,
    AMTARI_MACHINE_TT,
    AMTARI_MACHINE_FALCON
};

struct amtari_context {
    enum amtari_machine machine;
    enum amtari_mode mode;
    int initialized;
};

const char *amtari_version(void);
int amtari_init(struct amtari_context *ctx);

#endif
