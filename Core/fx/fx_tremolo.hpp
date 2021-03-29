#pragma once

#include "common.h"
#include "fx_base.h"
#include "lib_calc.hpp"
#include "lib_osc.hpp"
#include <string.h>

namespace fx {
class tremolo;
}

/// @brief トレモロ
class fx::tremolo : public fx::base {
private:
    enum PARAM_TYPE {
        LEVEL,
        RATE,
        DEPTH,
        WAVE,
        PARAM_TYPE_COUNT, // パラメータ種類総数
    };
    float param_[PARAM_COUNT] = { 0.0f, 1.0f, 1.0f, 1.0f };
    const int16_t PARAM_MAX[PARAM_COUNT] = { 100, 100, 100, 100 };
    const int16_t PARAM_MIN[PARAM_COUNT] = { 0, 0, 0, 0 };
    char const* const PARAM_NAME[PARAM_COUNT] = { "LEVEL", "RATE", "DEPTH", "WAVE" };

    signalSw bypass_;
    triangleWave tri_;

public:
    char const* getFxName() const override { return "TREMOLO"; }

    uint16_t getLedColor(bool on) const override { return on ? 0b0000011111111111 /*青緑*/ : 0; }

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
            param_[LEVEL] = logPot(g_fxParam[LEVEL].value, -20.0f, 20.0f); // LEVEL -20 ～ 20dB
            break;
        case 1:
            param_[RATE] = 0.01f * (105.0f - (float)g_fxParam[RATE].value); // Rate 0.05s ～ 1.05s
            break;
        case 2:
            param_[DEPTH] = (float)g_fxParam[DEPTH].value * 0.1f; // Depth ±10dB
            break;
        case 3:
            param_[WAVE] = logPot(g_fxParam[WAVE].value, 0.0f, 50.0f); // Wave 三角波～矩形波変形
            break;
        case 4:
            tri_.set(param_[RATE]);
            break;
        default:
            break;
        }
    }

    void process(float (&xL)[BLOCK_SIZE], float (&xR)[BLOCK_SIZE], bool on) override {
        float fxL[BLOCK_SIZE] = {};

        setParam();

        for (uint32_t i = 0; i < BLOCK_SIZE; i++) {
            float gain = 2.0f * (tri_.output() - 0.5f);    // -1 ～ 1 dB LFO
            gain = clip(gain * param_[WAVE], -1.0f, 1.0f); // 三角波～矩形波変形
            gain = gain * param_[DEPTH];                   // gain -10 ～ 10 dB

            fxL[i] = param_[LEVEL] * xL[i] * dbToGain(gain); // LEVEL
            xL[i] = bypass_.process(xL[i], fxL[i], on);
        }
    }
};
