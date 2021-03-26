#pragma once

#include "common.h"
#include "fx_base.hpp"
#include "lib_calc.hpp"
#include "lib_delayPrimeNum.hpp"
#include "lib_filter.hpp"
#include <string.h>

namespace fx {
class reverb;
}

/// @brief リバーブ
/// Pure Data [rev2~] に基づいたFDNリバーブ
class fx::reverb : public fx::base {
private:
    enum PARAM_NAME {
        LEVEL,
        MIX,
        FBACK,
        HICUT,
        LOCUT,
        HIDUMP,
        PARAM_TYPE_COUNT, // パラメータ種類総数
    };
    float param_[PARAM_COUNT] = { 0.0f, 0.0f, 0.0f, 1.0f, 1.0f, 1.0f };
    const int16_t PARAM_MAX[PARAM_COUNT] = { 100, 100, 99, 100, 100, 100, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1 };
    const int16_t PARAM_MIN[PARAM_COUNT] = { 0, 0, 0, 0, 0, 0 };
    char const* const PARAM_NAME[PARAM_COUNT] = { "LEVEL", "MIX", "F.BACK", "HiCUT", "LoCUT", "HiDUMP" };
    const uint8_t dt[10] = { 44, 26, 19, 16, 8, 4, 59, 69, 75, 86 }; // ディレイタイム配列

    signalSw bypassIn_;
    signalSw bypassOutL_;
    signalSw bypassOutR_;
    delayBufPrimeNum del_[10];
    lpf lpfIn_;
    lpf lpfFB_[4];
    hpf hpfOutL_;
    hpf hpfOutR_;

public:
    char const* getFxName() const override { return "REVERB"; }

    uint16_t getLedColor(bool on) const override { return on ? 0b1111111111111111 /*白*/ : 0; }

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
        for (int i = 0; i < 10; i++) {
            del_[i].set(dt[i]); // 最大ディレイタイム設定
        }
    }

    void deinit() override {
        for (int i = 0; i < 10; i++)
            del_[i].erase();
    }

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
        count = (count + 1) % 13; // 負荷軽減のためパラメータ計算を分散させる
        switch (count) {
        case 0:
            param_[LEVEL] = logPot(g_fxParam[LEVEL].value, -20.0f, 20.0f); // LEVEL -20 ～ +20dB
            break;
        case 1:
            param_[MIX] = mixPot(g_fxParam[MIX].value, -20.0f); // MIX
            break;
        case 2:
            param_[FBACK] = (float)g_fxParam[FBACK].value / 200.0f; // Feedback 0～0.495
            break;
        case 3:
            param_[HICUT] = 600.0f * logPot(g_fxParam[HICUT].value, 20.0f, 0.0f); // HI CUT FREQ 600 ~ 6000 Hz
            break;
        case 4:
            param_[LOCUT] = 100.0f * logPot(g_fxParam[LOCUT].value, 0.0f, 20.0f); // LOW CUT FREQ 100 ~ 1000 Hz
            break;
        case 5:
            param_[HIDUMP] =
                600.0f * logPot(g_fxParam[HIDUMP].value, 20.0f, 0.0f); // Feedback HI CUT FREQ 600 ~ 6000 Hz
            break;
        case 6:
            lpfIn_.set(param_[HICUT]);
            break;
        case 7:
            hpfOutL_.set(param_[LOCUT]);
            break;
        case 8:
            hpfOutR_.set(param_[LOCUT]);
            break;
        case 9:
            lpfFB_[0].set(param_[HIDUMP]);
            break;
        case 10:
            lpfFB_[1].set(param_[HIDUMP]);
            break;
        case 11:
            lpfFB_[2].set(param_[HIDUMP]);
            break;
        case 12:
            lpfFB_[3].set(param_[HIDUMP]);
            break;
        default:
            break;
        }
    }

    void process(float xL[], float xR[], bool on) override {
        float fxL[BLOCK_SIZE] = {};
        float fxR[BLOCK_SIZE] = {}; // Rch 不使用

        float ap, am, bp, bm, cp, cm, dp, dm, ep, em, fp, fm, gp, gm, hd, id, jd, kd, out_l, out_r;

        setParam();

        for (uint32_t i = 0; i < BLOCK_SIZE; i++) {
            fxL[i] = bypassIn_.process(0.0f, xL[i], on);
            fxL[i] = 0.25f * lpfIn_.process(fxL[i]);

            // Early Reflection

            del_[0].write(fxL[i]);
            ap = fxL[i] + del_[0].readFixed();
            am = fxL[i] - del_[0].readFixed();
            del_[1].write(am);
            bp = ap + del_[1].readFixed();
            bm = ap - del_[1].readFixed();
            del_[2].write(bm);
            cp = bp + del_[2].readFixed();
            cm = bp - del_[2].readFixed();
            del_[3].write(cm);
            dp = cp + del_[3].readFixed();
            dm = cp - del_[3].readFixed();
            del_[4].write(dm);
            ep = dp + del_[4].readFixed();
            em = dp - del_[4].readFixed();
            del_[5].write(em);

            // Late Reflection & High Freq Dumping

            hd = del_[6].readFixed();
            hd = lpfFB_[0].process(hd);

            id = del_[7].readFixed();
            id = lpfFB_[1].process(id);

            jd = del_[8].readFixed();
            jd = lpfFB_[2].process(jd);

            kd = del_[9].readFixed();
            kd = lpfFB_[3].process(kd);

            out_l = ep + hd * param_[FBACK];
            out_r = del_[5].readFixed() + id * param_[FBACK];

            fp = out_l + out_r;
            fm = out_l - out_r;
            gp = jd * param_[FBACK] + kd * param_[FBACK];
            gm = jd * param_[FBACK] - kd * param_[FBACK];
            del_[6].write(fp + gp);
            del_[7].write(fm + gm);
            del_[8].write(fp - gp);
            del_[9].write(fm - gm);

            fxL[i] = (1.0f - param_[MIX]) * xL[i] + param_[MIX] * hpfOutL_.process(out_l);
            fxR[i] = (1.0f - param_[MIX]) * xL[i] + param_[MIX] * hpfOutR_.process(out_r);

            xL[i] = bypassOutL_.process(xL[i], param_[LEVEL] * fxL[i], on);
            xR[i] = bypassOutR_.process(xR[i], param_[LEVEL] * fxR[i], on);
        }
    }
};
