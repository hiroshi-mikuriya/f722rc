#pragma once

#include "common.h"
#include "fx_base.hpp"
#include "lib_calc.hpp"
#include "lib_delay.hpp"
#include "lib_filter.hpp"
#include "lib_osc.hpp"
#include <string.h>
#include <string.h> // strcpy

namespace fx {
class chorus;
}

/// @brief コーラス
class fx::chorus : public fx::base {
private:
    enum PARAM_TYPE {
        LEVEL = 0,
        MIX,
        FBACK,
        RATE,
        DEPTH,
        TONE,
        PARAM_TYPE_COUNT, // パラメータ種類総数
    };
    float param_[PARAM_COUNT] = { 0.0f, 0.0f, 0.0f, 1.0f, 1.0f, 1.0f };
    const int16_t PARAM_MAX[PARAM_COUNT] = { 100, 100, 99, 100, 100, 100, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1 };
    const int16_t PARAM_MIN[PARAM_COUNT] = { 0, 0, 0, 0, 0, 0 };
    char const* const PARAM_NAME[PARAM_COUNT] = { "LEVEL", "MIX", "F.BACK", "RATE", "DEPTH", "TONE" };

    signalSw bypass_;
    triangleWave tri1_;
    delayBuf del1_;
    hpf hpf1_;
    lpf2nd lpf2nd1_;
    lpf2nd lpf2nd2_;

public:
    char const* getFxName() const override { return "CHORUS"; }

    uint16_t getLedColor(bool on) const override { return on ? 0b0000000000011111 /*青*/ : 0; }

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

        del1_.set(20.0f);  // 最大ディレイタイム設定
        hpf1_.set(100.0f); // ウェット音のローカット設定
    }

    void deinit() override { del1_.erase(); }

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
            param_[MIX] = mixPot(g_fxParam[MIX].value, -20.0f); // MIX
            break;
        case 2:
            param_[FBACK] = (float)g_fxParam[FBACK].value / 100.0f; // Feedback 0～0.99
            break;
        case 3:
            param_[RATE] = 0.02f * (105.0f - (float)g_fxParam[RATE].value); // Rate 2s
            break;
        case 4:
            param_[DEPTH] = 0.1f * (float)g_fxParam[DEPTH].value; // Depth 10ms
            break;
        case 5:
            param_[TONE] = 800.0f * logPot(g_fxParam[TONE].value, 0.0f, 20.0f); // HI CUT FREQ 800 ～ 8000 Hz
            break;
        case 6:
            lpf2nd1_.set(param_[TONE]);
            break;
        case 7:
            lpf2nd2_.set(param_[TONE]);
            break;
        case 8:
            tri1_.set(param_[RATE]);
            break;
        default:
            break;
        }
    }

    void process(float xL[], float xR[], bool on) override {
        float fxL[BLOCK_SIZE] = {};

        setParam();

        for (uint32_t i = 0; i < BLOCK_SIZE; i++) {
            float dtime = param_[DEPTH] * tri1_.output() + 5.0f; // ディレイタイム5~15ms
            fxL[i] = del1_.readLerp(dtime);
            fxL[i] = lpf2nd1_.process(fxL[i]);
            fxL[i] = lpf2nd2_.process(fxL[i]);
            del1_.write(hpf1_.process(xL[i]) + param_[FBACK] * fxL[i]);
            fxL[i] = (1.0f - param_[MIX]) * xL[i] + param_[MIX] * fxL[i];
            xL[i] = bypass_.process(xL[i], fxL[i] * param_[LEVEL], on);
        }
    }
};
