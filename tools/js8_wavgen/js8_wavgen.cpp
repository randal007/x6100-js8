/*
 *  SPDX-License-Identifier: GPL-3.0-only
 *
 *  js8_wavgen: write a JS8 test WAV for the X6100 JS8 app's "Test WAV" mode.
 */

#include "testsignal.hpp"
#include "wav.hpp"

#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <string>
#include <vector>

using namespace x6100::js8;

static void usage() {
    std::fprintf(stderr,
                 "usage: js8_wavgen [-r RATE] [-n NOISE_RMS] OUT.wav STATION...\n"
                 "\n"
                 "  STATION is CALL,GRID,OFFSET_HZ,SNR_DB,TEXT  (TEXT may contain spaces)\n"
                 "  e.g.  'W1ABC,FN42,1200,-10,K2XYZ HELLO FROM THE BAND'\n"
                 "        'VE3KP,FN03,900,-5,CQ CQ CQ FN03'\n"
                 "        'W1ABC,FN42,620,-12,W1ABC: HEARTBEAT FN42'\n"
                 "\n"
                 "  Sample 0 is a 15 s slot boundary; multi-frame messages take\n"
                 "  consecutive slots. Default rate 11025 (the radio's), noise 0.02.\n");
}

int main(int argc, char **argv) {
    int   rate  = 11025;
    float noise = 0.02f;
    int   i     = 1;
    for (; i < argc && argv[i][0] == '-'; i++) {
        if (!std::strcmp(argv[i], "-r") && i + 1 < argc) rate = std::atoi(argv[++i]);
        else if (!std::strcmp(argv[i], "-n") && i + 1 < argc) noise = (float)std::atof(argv[++i]);
        else return usage(), 2;
    }
    if (argc - i < 2 || rate < 8000) return usage(), 2;

    std::string              out = argv[i++];
    std::vector<TestStation> stations;
    for (; i < argc; i++) {
        std::string              s = argv[i];
        std::vector<std::string> f;
        std::size_t              p = 0;
        for (int k = 0; k < 4; k++) {
            auto c = s.find(',', p);
            if (c == std::string::npos) break;
            f.push_back(s.substr(p, c - p));
            p = c + 1;
        }
        if (f.size() != 4) {
            std::fprintf(stderr, "bad station: %s\n", argv[i]);
            return 2;
        }
        stations.push_back({f[0], f[1], s.substr(p), std::atof(f[2].c_str()), std::atof(f[3].c_str())});
    }

    auto audio = make_test_band(stations, rate, noise);
    if (!write_wav(out, rate, audio)) {
        std::fprintf(stderr, "cannot write %s\n", out.c_str());
        return 1;
    }
    std::printf("%s: %.0f s at %d Hz, %zu station(s)\n", out.c_str(), audio.size() / (double)rate, rate,
                stations.size());
    return 0;
}
