#pragma once

#include "common.h"
#include "fx_base.h"
#include "lib_calc.hpp"
#include "lib_filter.hpp"
#include <string.h>

namespace fx {
class overdrive;
}

/// @brief オーバードライブ
class fx::overdrive : public fx::base {
private:
    enum PARAM_NAME {
        LEVEL,
        GAIN,
        TREBLE,
        BASS,
        PARAM_TYPE_COUNT, // パラメータ種類総数
    };
    float param_[PARAM_COUNT] = { 0.0f, 0.0f, 0.0f, 0.0f };
    const int16_t PARAM_MAX[PARAM_COUNT] = { 100, 100, 100, 100 };
    const int16_t PARAM_MIN[PARAM_COUNT] = { 0, 0, 0, 0 };
    char const* const PARAM_NAME[PARAM_COUNT] = { "LEVEL", "GAIN", "TREBLE", "BASS" };

    signalSw bypass_;
    hpf hpfFixed_;
    hpf hpfBass_;
    lpf lpfFixed_;
    lpf lpfTreble_;

public:
    char const* getFxName() const override { return "OVERDRIVE"; }

    uint16_t getLedColor(bool on) const override { return on ? 0b1111111111100000 /*赤緑*/ : 0; }

    uint8_t getParamTypeCount() const override { return PARAM_TYPE_COUNT; }

    void init(int16_t const* loadData) override {
        for (uint32_t i = 0; i < PARAM_COUNT; i++) {
            g_fxParam[i].nameTxt = PARAM_NAME[i] ? PARAM_NAME[i] : "";
            g_fxParam[i].max = PARAM_MAX[i];
            g_fxParam[i].min = PARAM_MIN[i];
            if (PARAM_MIN[i] <= loadData[i] && loadData[i] <= PARAM_MAX[i]) {
                g_fxParam[i].value = loadData[i];
            }
            else {
                g_fxParam[i].value = (PARAM_MIN[i] + PARAM_MAX[i]) / 2;
            }
        }
        hpfFixed_.set(10.0f);
        lpfFixed_.set(5000.0f);
    }

    void deinit() override {}

    void setParamStr(uint8_t paramIdx) override {
        FxParam& fp = g_fxParam[paramIdx];
        if (paramIdx < PARAM_TYPE_COUNT) {
            snprintf(fp.valueTxt, sizeof(fp.valueTxt), "%d", fp.value);
        }
        else {
            memset(fp.valueTxt, 0, sizeof(fp.valueTxt));
        }
    }

    void setParam() override {
        static uint8_t count = 0;
        count = (count + 1) % 10; // 負荷軽減のためパラメータ計算を分散させる
        switch (count) {
        case 0:
            param_[LEVEL] = logPot(g_fxParam[LEVEL].value, -40.0f, 10.0f); // LEVEL -40...10 dB
            break;
        case 1:
            param_[GAIN] = logPot(g_fxParam[GAIN].value, -6.0f, 40.0f); // GAIN -6...+40 dB
            break;
        case 2:
            param_[TREBLE] = 10000.0f * logPot(g_fxParam[TREBLE].value, -28.0f, 0.0f); // TREBLE LPF 400 ~ 10k Hz
            break;
        case 3:
            param_[BASS] = 1000.0f * logPot(g_fxParam[BASS].value, 0.0f, -20.0f); // BASS HPF 100 ~ 1000 Hz
            break;
        case 4:
            lpfTreble_.set(param_[TREBLE]);
            break;
        case 5:
            hpfBass_.set(param_[BASS]);
            break;
        default:
            break;
        }
    }

    void process(float (&xL)[BLOCK_SIZE], float (&xR)[BLOCK_SIZE], bool on) override {
        float fxL[BLOCK_SIZE] = {};

        setParam();

        for (uint32_t i = 0; i < BLOCK_SIZE; i++) {
            fxL[i] = hpfBass_.process(xL[i]);   // BASS
            fxL[i] = lpfFixed_.process(fxL[i]); // 高域カット
            fxL[i] = 5.0f * fxL[i];             // 初期固定ゲイン

            if (fxL[i] < -0.5f)
                fxL[i] = -0.25f; // 2次関数による波形の非対称変形
            else
                fxL[i] = fxL[i] * fxL[i] + fxL[i];

            fxL[i] = param_[GAIN] * hpfFixed_.process(fxL[i]); // GAIN、直流カット

            if (fxL[i] < -1.0f)
                fxL[i] = -1.0f; // 2次関数による対称ソフトクリップ
            else if (fxL[i] < 0.0f)
                fxL[i] = fxL[i] * fxL[i] + 2.0f * fxL[i];
            else if (fxL[i] < 1.0f)
                fxL[i] = 2.0f * fxL[i] - fxL[i] * fxL[i];
            else
                fxL[i] = 1.0f;

            fxL[i] = param_[LEVEL] * lpfTreble_.process(fxL[i]); // LEVEL, TREBLE

            xL[i] = bypass_.process(xL[i], fxL[i], on);
        }
    }
};
