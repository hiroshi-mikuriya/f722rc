#pragma once

#include "common.h"
#include "fx_base.hpp"
#include "lib_calc.hpp"
#include "lib_osc.hpp"
#include <string.h>

namespace fx {
class phaser;
}

/// @brief フェイザー
class fx::phaser : public fx::base {
private:
    enum PARAM_TYPE {
        LEVEL,
        RATE,
        STAGE,
        PARAM_TYPE_COUNT, // パラメータ種類総数
    };
    float param_[PARAM_COUNT] = { 0.0f, 0.0f, 0.0f };
    const int16_t PARAM_MAX[PARAM_COUNT] = { 100, 100, 6, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1 };
    const int16_t PARAM_MIN[PARAM_COUNT] = { 0, 0, 1 };
    char const* const PARAM_NAME[PARAM_COUNT] = { "LEVEL", "RATE", "STAGE" };

    signalSw bypass_;
    triangleWave tri_;
    apf apfx_[12];

public:
    char const* getFxName() const override { return "PHASER"; }

    uint16_t getLedColor(bool on) const override { return on ? 0b1111100000000000 /*赤*/ : 0; }

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
            param_[RATE] = 0.02f * (105.0f - (float)g_fxParam[RATE].value); // RATE 2s
            break;
        case 2:
            param_[STAGE] = 0.1f + (float)g_fxParam[STAGE].value * 2.0f; // STAGE 2, 4, 6, 8, 12
            break;
        case 3:
            tri_.set(param_[RATE]);
            break;
        default:
            break;
        }
    }

    void process(float xL[], float xR[], bool on) override {
        float fxL[BLOCK_SIZE] = {};

        setParam();

        for (uint32_t i = 0; i < BLOCK_SIZE; i++) {
            fxL[i] = xL[i];
            float freq = 200.0f * dbToGain(20.0f * tri_.output()); // APF周波数 200～2000Hz

            for (uint8_t j = 0; j < (uint8_t)param_[STAGE]; j++) // 段数分APFをかける
            {
                apfx_[j].set(freq);                // APF周波数を設定
                fxL[i] = apfx_[j].process(fxL[i]); // APF実行
            }
            fxL[i] = 0.7f * (xL[i] + fxL[i]); // 原音ミックス
            xL[i] = bypass_.process(xL[i], fxL[i] * param_[LEVEL], on);
        }
    }
};
