#pragma once

#include "common.h"
#include "fx_base.h"
#include "lib_calc.hpp"
#include "lib_delay.hpp"
#include "lib_filter.hpp"
#include <string.h>

namespace fx {
class delay;
}

/// @brief ディレイ
class fx::delay : public fx::base {
private:
    enum PARAM_NAME {
        DTIME,
        ELEVEL,
        FBACK,
        TONE,
        OUTPUT,
        TAPDIV,
        PARAM_TYPE_COUNT, // パラメータ種類総数
    };
    float param_[PARAM_COUNT] = { 0.0f, 0.0f, 0.0f, 1.0f, 1.0f, 0 };
    const int16_t PARAM_MAX[PARAM_COUNT] = { 1500, 100, 99, 100, 100, 5 };
    const int16_t PARAM_MIN[PARAM_COUNT] = { 10, 0, 0, 0, 0, 0 };
    char const* const PARAM_NAME[PARAM_COUNT] = { "TIM", "LEVEL", "F.BACK", "TONE", "OUTPUT", "DIV" };

    // 最大ディレイタイム 16bit モノラルで2.5秒程度まで
    const float maxDelayTime = 1500.0f;

    // タップテンポ DIV定数 0←→5で循環させ、実際使うのは1～4
    const std::string tapDivStr[6] = { "1/1", "1/1", "1/2", "1/3", "3/4", "1/1" };
    const float tapDivFloat[6] = { 1.0f, 1.0f, 0.5f, 0.333333f, 0.75f, 1.0f };

    signalSw bypassIn_;
    signalSw bypassOut_;
    delayBuf del1_;
    lpf2nd lpf2ndTone_;

public:
    char const* getFxName() const override { return "DELAY"; }

    uint16_t getLedColor(bool on) const override { return on ? 0b1111100000011111 /*赤青*/ : 0; }

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
        del1_.set(maxDelayTime); // 最大ディレイタイム設定
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
        float divTapTime = g_tapTime * tapDivFloat[g_fxParam[TAPDIV].value]; // DIV計算済タップ時間
        static uint8_t count = 0;
        count = (count + 1) % 10; // 負荷軽減のためパラメータ計算を分散させる
        switch (count) {
        case 0:
            if (divTapTime > 10.0f && divTapTime < maxDelayTime) {
                param_[DTIME] = divTapTime;
                g_fxParam[DTIME].value = param_[DTIME];
            }
            else {
                param_[DTIME] = (float)g_fxParam[DTIME].value; // DELAYTIME 10 ～ 1500 ms
            }
            break;
        case 1:
            param_[ELEVEL] = logPot(g_fxParam[ELEVEL].value, -20.0f, 20.0f); // EFFECT LEVEL -20 ～ +20dB
            break;
        case 2:
            param_[FBACK] = (float)g_fxParam[FBACK].value / 100.0f; // Feedback 0 ～ 0.99 %
            break;
        case 3:
            param_[TONE] = 800.0f * logPot(g_fxParam[TONE].value, 0.0f, 20.0f); // HI CUT FREQ 800 ～ 8000 Hz
            break;
        case 4:
            param_[OUTPUT] = logPot(g_fxParam[OUTPUT].value, -20.0f, 20.0f); // OUTPUT LEVEL -20 ～ +20dB
            break;
        case 5:
            lpf2ndTone_.set(param_[TONE]);
            break;
        case 6:
            if (g_fxParam[TAPDIV].value < 1)
                g_fxParam[TAPDIV].value = 4; // TAPDIV 0←→5で循環させ、実際使うのは1～4
            if (g_fxParam[TAPDIV].value > 4)
                g_fxParam[TAPDIV].value = 1;
            break;
        default:
            break;
        }
    }

    void process(float (&xL)[BLOCK_SIZE], float (&xR)[BLOCK_SIZE], bool on) override {
        float fxL[BLOCK_SIZE] = {};

        setParam();

        for (uint32_t i = 0; i < BLOCK_SIZE; i++) {
            fxL[i] = del1_.read(param_[DTIME]);   // ディレイ音読み込み
            fxL[i] = lpf2ndTone_.process(fxL[i]); // ディレイ音のTONE（ハイカット）

            // ディレイ音と原音をディレイバッファに書き込み、原音はエフェクトオン時のみ書き込む
            del1_.write(bypassIn_.process(0.0f, xL[i], on) + param_[FBACK] * fxL[i]);

            fxL[i] = param_[OUTPUT] * (xL[i] + fxL[i] * param_[ELEVEL]); // マスターボリューム ディレイ音レベル
            xL[i] = bypassOut_.process(xL[i], fxL[i], on);
        }
    }
};
